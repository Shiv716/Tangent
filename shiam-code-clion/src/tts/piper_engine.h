#pragma once

#include <string>

namespace tangent {

// Wraps the `piper` CLI binary as a subprocess. Piper takes text on stdin
// and writes a WAV file. We write the phrase text to a temp file rather
// than piping through a shell string, so phrase content never has to be
// shell-escaped.
class PiperEngine {
public:
    PiperEngine(std::string piperBinary, std::string voiceModelPath);

    // Synthesizes `text` to a WAV file and returns the path, or empty
    // string on failure. Each call overwrites the same scratch file —
    // fine for a sequential PoC pipeline, not for concurrent use.
    std::string synthesize(const std::string& text) const;

    bool isAvailable() const;

private:
    std::string piperBinary_;
    std::string voiceModelPath_;
};

} // namespace tangent
