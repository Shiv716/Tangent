#include "whisper_engine.h"

#include <whisper.h>
#include <iostream>
#include <chrono>

namespace tangent {

WhisperEngine::WhisperEngine(const std::string& modelPath, int numThreads)
    : numThreads_(numThreads) {
    whisper_context_params cparams = whisper_context_default_params();
#ifdef GGML_USE_CUDA
    cparams.use_gpu = true;
#endif
    ctx_ = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!ctx_) {
        std::cerr << "[whisper] failed to load model from " << modelPath
                  << " — check the path and that the model file downloaded correctly.\n";
    }
}

WhisperEngine::~WhisperEngine() {
    if (ctx_) {
        whisper_free(ctx_);
    }
}

Transcript WhisperEngine::transcribe(const std::vector<float>& pcm16kMono) const {
    Transcript result;
    result.duration_seconds = static_cast<double>(pcm16kMono.size()) / 16000.0;

    if (!ctx_) {
        std::cerr << "[whisper] transcribe() called but model isn't loaded.\n";
        return result;
    }
    if (pcm16kMono.empty()) {
        std::cerr << "[whisper] empty audio buffer — was anything actually recorded?\n";
        return result;
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.translate = false;
    params.language = "en";
    params.n_threads = numThreads_;
    params.no_context = true;
    params.single_segment = false;

    auto t0 = std::chrono::steady_clock::now();
    int rc = whisper_full(ctx_, params, pcm16kMono.data(), static_cast<int>(pcm16kMono.size()));
    auto t1 = std::chrono::steady_clock::now();

    if (rc != 0) {
        std::cerr << "[whisper] whisper_full() returned error code " << rc << "\n";
        return result;
    }

    int nSegments = whisper_full_n_segments(ctx_);
    std::string text;
    for (int i = 0; i < nSegments; ++i) {
        text += whisper_full_get_segment_text(ctx_, i);
    }

    // Trim leading whitespace whisper sometimes emits.
    size_t firstNonSpace = text.find_first_not_of(" \t\n");
    result.text = (firstNonSpace == std::string::npos) ? "" : text.substr(firstNonSpace);

    double inferenceMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cerr << "[whisper] transcribed " << result.duration_seconds << "s of audio in "
              << inferenceMs << "ms\n";

    return result;
}

} // namespace tangent
