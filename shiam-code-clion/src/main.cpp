// stillness.device — proof of concept orchestrator
//
// Pipeline (matches the README architecture diagram):
//   Mic -> Whisper (STT) -> Cognee (read memory) -> Ollama LLM (decide)
//       -> Cognee (write session) -> Piper TTS + Soundscape -> Speaker
//
// This is a push-to-talk CLI loop, not the headless always-listening
// device — that's a deliberate PoC simplification so the demo is
// predictable on stage. Swapping in VAD-based always-listening capture
// later only touches src/audio/mic_capture.cpp.
//
// Run from the project root (so relative asset/script paths resolve) —
// see README.md for the CLion run configuration setup.

#include <SDL2/SDL.h>
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "audio/mic_capture.h"
#include "audio/playback.h"
#include "stt/whisper_engine.h"
#include "llm/ollama_client.h"
#include "memory/cognee_bridge.h"
#include "tts/piper_engine.h"
#include "config/session_types.h"
#include "config/phrase_library.h"

using namespace tangent;

namespace {

struct Args {
    std::string whisperModel = "models/ggml-small.en.bin";
    std::string piperBinary = "piper";
    std::string piperVoice = "models/en_GB-alba-medium.onnx";
    std::string ollamaModel = "phi3";
    std::string ollamaHost = "http://localhost:11434";
    std::string assetsDir = "assets";
    std::string phraseLibraryPath = "assets/phrases/library.json";
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
        if (arg == "--whisper-model") a.whisperModel = next();
        else if (arg == "--piper-binary") a.piperBinary = next();
        else if (arg == "--piper-voice") a.piperVoice = next();
        else if (arg == "--ollama-model") a.ollamaModel = next();
        else if (arg == "--ollama-host") a.ollamaHost = next();
        else if (arg == "--assets-dir") a.assetsDir = next();
        else if (arg == "--help") {
            std::cout << "Usage: tangent_poc [--whisper-model PATH] [--piper-binary PATH] "
                         "[--piper-voice PATH] [--ollama-model NAME] [--ollama-host URL] "
                         "[--assets-dir DIR]\n";
            std::exit(0);
        }
    }
    return a;
}

std::string nowIso8601() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%FT%TZ");
    return oss.str();
}

void waitForEnter(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string discard;
    std::getline(std::cin, discard);
}

} // namespace

int main(int argc, char** argv) {
    Args args = parseArgs(argc, argv);

    std::cout << "=== stillness.device — proof of concept ===\n";
    std::cout << "Pipeline: Mic -> Whisper -> Cognee -> Ollama -> Cognee -> Piper/Soundscape -> Speaker\n\n";

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    // --- Init pipeline components -------------------------------------
    std::cout << "[init] loading Whisper model from " << args.whisperModel << " ...\n";
    WhisperEngine whisper(args.whisperModel);
    if (!whisper.isLoaded()) {
        std::cerr << "[init] FATAL: whisper model failed to load. Did you run scripts/setup.sh?\n";
        return 1;
    }

    std::cout << "[init] checking Ollama at " << args.ollamaHost << " (model: " << args.ollamaModel << ") ...\n";
    OllamaClient ollama(args.ollamaModel, args.ollamaHost);
    if (!ollama.healthCheck()) {
        std::cerr << "[init] FATAL: cannot reach Ollama at " << args.ollamaHost
                  << " — run `ollama serve` (or just `ollama run " << args.ollamaModel << "`) first.\n";
        return 1;
    }

    std::cout << "[init] checking Piper binary (" << args.piperBinary << ") ...\n";
    PiperEngine piper(args.piperBinary, args.piperVoice);
    if (!piper.isAvailable()) {
        std::cerr << "[init] FATAL: piper binary not found on PATH. See scripts/setup.sh.\n";
        return 1;
    }

    CogneeBridge memory; // defaults: python3, scripts/cognee_memory.py
    PhraseLibrary phrases(args.phraseLibraryPath);
    if (!phrases.isLoaded()) {
        std::cerr << "[init] FATAL: could not load phrase library at " << args.phraseLibraryPath << "\n";
        return 1;
    }

    MicCapture mic;
    if (!mic.isReady()) {
        std::cerr << "[init] FATAL: microphone capture device could not be opened.\n";
        return 1;
    }

    Playback playback;
    if (!playback.isReady()) {
        std::cerr << "[init] FATAL: audio playback (SDL_mixer) failed to initialise.\n";
        return 1;
    }

    std::cout << "\n[init] all components ready.\n\n";

    // --- Main push-to-talk loop ----------------------------------------
    while (true) {
        std::cout << "Type 'quit' + Enter to exit, or just press Enter to start talking > " << std::flush;
        std::string line;
        std::getline(std::cin, line);
        if (line == "quit" || line == "exit") {
            break;
        }

        mic.startRecording();
        std::cout << "Recording... speak now. Press Enter when you're done.\n";
        waitForEnter("");
        auto pcm = mic.stopRecording();

        std::cout << "[whisper] transcribing " << pcm.size() / 16000.0 << "s of audio...\n";
        Transcript transcript = whisper.transcribe(pcm);
        if (transcript.text.empty()) {
            std::cout << "Didn't catch anything — let's try again.\n\n";
            continue;
        }
        std::cout << "You said: \"" << transcript.text << "\"\n";

        std::cout << "[memory] reading context...\n";
        MemoryContext memCtx = memory.readContext();
        if (memCtx.has_history) {
            std::cout << "[memory] " << memCtx.session_count << " prior session(s). Last: "
                      << memCtx.last_mood << " / " << memCtx.last_soundscape << " / "
                      << memCtx.last_duration_minutes << "min\n";
        } else {
            std::cout << "[memory] no prior history — first session.\n";
        }

        std::cout << "[ollama] planning session...\n";
        SessionPlan plan = ollama.planSession(transcript, memCtx);
        std::cout << "[plan] mood=" << plan.mood << " soundscape=" << plan.soundscape
                  << " duration=" << plan.duration_minutes << "min\n";
        std::cout << "[plan] reasoning: " << plan.reasoning << "\n";

        std::string script = phrases.buildScript(plan.phrase_ids);
        std::cout << "[piper] synthesizing voice...\n";
        std::string voiceWav = piper.synthesize(script);

        std::cout << "[playback] starting session (Ctrl+C to abort early)...\n";
        playback.playSession(voiceWav, plan.soundscape, args.assetsDir, plan.duration_minutes * 60);

        SessionRecord record;
        record.mood = plan.mood;
        record.soundscape = plan.soundscape;
        record.duration_minutes = plan.duration_minutes;
        record.timestamp_iso8601 = nowIso8601();
        memory.writeSession(record);
        std::cout << "[memory] session saved.\n\n";
    }

    SDL_Quit();
    return 0;
}
