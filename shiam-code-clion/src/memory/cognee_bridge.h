#pragma once

#include <string>
#include "../config/session_types.h"

namespace tangent {

// Bridges to scripts/cognee_memory.py via subprocess. See that file's
// docstring for the LOCAL vs COGNEE backend split and why LOCAL is the
// safe default for a live demo.
class CogneeBridge {
public:
    explicit CogneeBridge(std::string pythonExe = "python3",
                           std::string scriptPath = "scripts/cognee_memory.py");

    MemoryContext readContext() const;
    bool writeSession(const SessionRecord& record) const;

private:
    std::string pythonExe_;
    std::string scriptPath_;

    // Runs a command via popen, captures stdout, returns it. Empty string on failure.
    std::string runCommand(const std::string& cmd) const;
};

} // namespace tangent
