#pragma once

#include <string>
#include <vector>
#include "../config/session_types.h"

struct whisper_context;

namespace tangent {

// Wraps whisper.cpp directly (linked as a library via CMake FetchContent,
// not a subprocess) — this is the one hot-path component where process
// spawn overhead would actually matter for a "feels responsive" demo.
class WhisperEngine {
public:
    explicit WhisperEngine(const std::string& modelPath, int numThreads = 4);
    ~WhisperEngine();

    WhisperEngine(const WhisperEngine&) = delete;
    WhisperEngine& operator=(const WhisperEngine&) = delete;

    bool isLoaded() const { return ctx_ != nullptr; }

    // pcm16kMono: float samples in [-1, 1] at 16000 Hz, single channel.
    Transcript transcribe(const std::vector<float>& pcm16kMono) const;

private:
    whisper_context* ctx_ = nullptr;
    int numThreads_;
};

} // namespace tangent
