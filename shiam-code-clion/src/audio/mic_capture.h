#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <SDL2/SDL.h>

namespace tangent {

// Push-to-talk capture, not voice-activity-detection. For a noisy hackathon
// floor, "press Enter, talk, press Enter again" is far more reliable on
// stage than energy-threshold silence detection misfiring on crowd noise.
//
// Requests AUDIO_F32SYS mono @ 16000Hz directly from SDL2 (it handles
// resampling internally if the mic's native format differs), which is
// exactly what whisper.cpp wants — no manual resampling needed downstream.
class MicCapture {
public:
    MicCapture();
    ~MicCapture();

    bool isReady() const { return deviceId_ != 0; }

    void startRecording();
    // Stops recording and returns the captured mono float32 16kHz buffer.
    std::vector<float> stopRecording();

private:
    SDL_AudioDeviceID deviceId_ = 0;
    std::atomic<bool> recording_{false};
    std::thread pollThread_;
    std::mutex bufferMutex_;
    std::vector<float> buffer_;

    void pollLoop();
};

} // namespace tangent
