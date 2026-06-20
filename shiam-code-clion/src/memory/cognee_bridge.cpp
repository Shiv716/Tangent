#include "cognee_bridge.h"

#include <nlohmann/json.hpp>
#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>

#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace tangent {

CogneeBridge::CogneeBridge(std::string pythonExe, std::string scriptPath)
    : pythonExe_(std::move(pythonExe)), scriptPath_(std::move(scriptPath)) {}

std::string CogneeBridge::runCommand(const std::string& cmd) const {
    std::array<char, 4096> buffer{};
    std::string result;

    // Redirect stderr to a separate stream so python's log() calls (which go
    // to stderr by design) don't corrupt the JSON we expect on stdout.
    std::string fullCmd = cmd + " 2>/tmp/tangent_memory_stderr.log";

    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "[memory] failed to launch: " << cmd << "\n";
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

MemoryContext CogneeBridge::readContext() const {
    MemoryContext ctx;
    std::string cmd = pythonExe_ + " " + scriptPath_ + " read";
    std::string output = runCommand(cmd);

    if (output.empty()) {
        std::cerr << "[memory] empty response from memory bridge — treating as no history.\n";
        return ctx;
    }

    try {
        json j = json::parse(output);
        ctx.has_history = j.value("has_history", false);
        ctx.last_mood = j.value("last_mood", "");
        ctx.last_soundscape = j.value("last_soundscape", "");
        ctx.last_duration_minutes = j.value("last_duration_minutes", 0);
        ctx.preference_notes = j.value("preference_notes", "");
        ctx.session_count = j.value("session_count", 0);
    } catch (const std::exception& e) {
        std::cerr << "[memory] failed to parse memory context JSON: " << e.what()
                  << "\n[memory] raw output was: " << output << "\n";
    }

    return ctx;
}

bool CogneeBridge::writeSession(const SessionRecord& record) const {
    json j = {
        {"mood", record.mood},
        {"soundscape", record.soundscape},
        {"duration_minutes", record.duration_minutes},
    };
    if (!record.timestamp_iso8601.empty()) {
        j["timestamp_iso8601"] = record.timestamp_iso8601;
    }

    fs::path tmpPath = fs::temp_directory_path() / "tangent_session_write.json";
    std::ofstream out(tmpPath);
    if (!out) {
        std::cerr << "[memory] could not write temp record file at " << tmpPath << "\n";
        return false;
    }
    out << j.dump();
    out.close();

    std::string cmd = pythonExe_ + " " + scriptPath_ + " write " + tmpPath.string();
    std::string output = runCommand(cmd);

    try {
        json resp = json::parse(output);
        return resp.value("ok", false);
    } catch (...) {
        std::cerr << "[memory] write did not return expected JSON ack: " << output << "\n";
        return false;
    }
}

} // namespace tangent
