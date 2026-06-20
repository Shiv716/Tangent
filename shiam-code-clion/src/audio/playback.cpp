#include "playback.h"
#include "soundscape_gen.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <thread>
#include <chrono>

namespace tangent {

Playback::Playback() {
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::cerr << "[playback] SDL_InitSubSystem(AUDIO) failed: " << SDL_GetError() << "\n";
            return;
        }
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
        std::cerr << "[playback] Mix_OpenAudio failed: " << Mix_GetError() << "\n";
        return;
    }
    Mix_AllocateChannels(4);
    ready_ = true;
}

Playback::~Playback() {
    if (ready_) {
        Mix_HaltChannel(-1);
        Mix_CloseAudio();
    }
}

void Playback::playSession(const std::string& voiceWavPath,
                            const std::string& soundscapeName,
                            const std::string& assetsDir,
                            int totalSeconds) const {
    if (!ready_) {
        std::cerr << "[playback] not initialised — skipping audio output.\n";
        return;
    }

    std::string soundscapePath = ensureSoundscape(soundscapeName, assetsDir);

    Mix_Chunk* ambient = soundscapePath.empty() ? nullptr : Mix_LoadWAV(soundscapePath.c_str());
    Mix_Chunk* voice = voiceWavPath.empty() ? nullptr : Mix_LoadWAV(voiceWavPath.c_str());

    if (!ambient) {
        std::cerr << "[playback] could not load soundscape: " << Mix_GetError() << "\n";
    } else {
        Mix_VolumeChunk(ambient, MIX_MAX_VOLUME * 0.35); // bed, quiet
        Mix_PlayChannel(0, ambient, -1); // loop forever on channel 0
    }

    if (!voice) {
        std::cerr << "[playback] could not load voice clip: " << Mix_GetError() << "\n";
    } else {
        Mix_VolumeChunk(voice, MIX_MAX_VOLUME * 0.95);
        Mix_PlayChannel(1, voice, 0); // play once on channel 1
    }

    std::cerr << "[playback] session running for " << totalSeconds << "s "
              << "(soundscape: " << soundscapeName << ")\n";

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(totalSeconds);
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Mix_HaltChannel(-1);
    if (ambient) Mix_FreeChunk(ambient);
    if (voice) Mix_FreeChunk(voice);
}

} // namespace tangent
