#include "mic_capture.h"

#include <iostream>

namespace tangent {

MicCapture::MicCapture() {
    SDL_AudioSpec want{};
    SDL_AudioSpec have{};
    SDL_zero(want);
    want.freq = 16000;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = nullptr; // we pull via SDL_DequeueAudio instead of a callback

    // allowed_changes = 0: we want exactly this format back. SDL inserts an
    // internal audio stream to convert from the mic's native format if needed.
    deviceId_ = SDL_OpenAudioDevice(nullptr, /*iscapture=*/1, &want, &have, 0);
    if (deviceId_ == 0) {
        std::cerr << "[mic] SDL_OpenAudioDevice failed: " << SDL_GetError() << "\n";
        std::cerr << "[mic] is a microphone connected and does this process have audio permissions?\n";
    } else {
        std::cerr << "[mic] capture device opened: " << have.freq << "Hz, "
                  << static_cast<int>(have.channels) << " channel(s)\n";
    }
}

MicCapture::~MicCapture() {
    if (recording_) {
        stopRecording();
    }
    if (deviceId_ != 0) {
        SDL_CloseAudioDevice(deviceId_);
    }
}

void MicCapture::startRecording() {
    if (!isReady()) {
        std::cerr << "[mic] startRecording() called but device isn't open.\n";
        return;
    }
    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        buffer_.clear();
    }
    SDL_ClearQueuedAudio(deviceId_);
    SDL_PauseAudioDevice(deviceId_, 0); // 0 = unpause = start capturing
    recording_ = true;
    pollThread_ = std::thread(&MicCapture::pollLoop, this);
}

std::vector<float> MicCapture::stopRecording() {
    recording_ = false;
    if (pollThread_.joinable()) {
        pollThread_.join();
    }
    SDL_PauseAudioDevice(deviceId_, 1); // 1 = pause = stop capturing

    // Drain anything left in the queue after the thread stopped polling.
    Uint32 remaining = SDL_GetQueuedAudioSize(deviceId_);
    if (remaining > 0) {
        std::vector<Uint8> tail(remaining);
        SDL_DequeueAudio(deviceId_, tail.data(), remaining);
        size_t nSamples = remaining / sizeof(float);
        const float* samples = reinterpret_cast<const float*>(tail.data());
        std::lock_guard<std::mutex> lock(bufferMutex_);
        buffer_.insert(buffer_.end(), samples, samples + nSamples);
    }

    std::lock_guard<std::mutex> lock(bufferMutex_);
    return buffer_;
}

void MicCapture::pollLoop() {
    std::vector<Uint8> chunk(4096);
    while (recording_) {
        Uint32 available = SDL_GetQueuedAudioSize(deviceId_);
        if (available > 0) {
            Uint32 toRead = std::min<Uint32>(available, static_cast<Uint32>(chunk.size()));
            // Keep it sample-aligned (float = 4 bytes).
            toRead -= (toRead % sizeof(float));
            if (toRead > 0) {
                SDL_DequeueAudio(deviceId_, chunk.data(), toRead);
                size_t nSamples = toRead / sizeof(float);
                const float* samples = reinterpret_cast<const float*>(chunk.data());
                std::lock_guard<std::mutex> lock(bufferMutex_);
                buffer_.insert(buffer_.end(), samples, samples + nSamples);
            }
        }
        SDL_Delay(30);
    }
}

} // namespace tangent
