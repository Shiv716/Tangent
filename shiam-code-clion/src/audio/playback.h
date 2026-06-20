#pragma once

#include <string>

namespace tangent {

class Playback {
public:
    Playback();
    ~Playback();

    bool isReady() const { return ready_; }

    // Loops `soundscapeName` (generating it procedurally on first use if
    // needed — see soundscape_gen.h) at low volume, layers `voiceWavPath`
    // (Piper's output) over the top, and blocks until `totalSeconds` have
    // elapsed, then stops both. assetsDir is the project's assets/ folder.
    void playSession(const std::string& voiceWavPath,
                      const std::string& soundscapeName,
                      const std::string& assetsDir,
                      int totalSeconds) const;

private:
    bool ready_ = false;
};

} // namespace tangent
