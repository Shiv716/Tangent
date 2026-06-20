#pragma once

#include <string>
#include <vector>
#include <optional>

namespace tangent {

// What Whisper hands back after transcribing a push-to-talk utterance.
struct Transcript {
    std::string text;
    double duration_seconds = 0.0;
};

// What Cognee hands back when we ask "what do we know about this user".
// Kept deliberately flat — this is a PoC, not the final schema.
struct MemoryContext {
    bool has_history = false;
    std::string last_mood;
    std::string last_soundscape;
    int last_duration_minutes = 0;
    std::string preference_notes;   // freeform notes Cognee has accumulated
    int session_count = 0;
};

// The structured decision the LLM is asked to produce.
struct SessionPlan {
    std::string mood;                       // e.g. "anxious", "restless", "calm-seeking"
    std::string soundscape;                 // e.g. "ocean", "rain", "forest", "bowls", "white_noise"
    std::vector<std::string> phrase_ids;    // ids into the phrase library
    int duration_minutes = 5;
    std::string reasoning;                  // short explanation, shown in terminal for demo transparency
    bool valid = false;                     // false if LLM output failed to parse
};

// What we persist back to memory after a session.
struct SessionRecord {
    std::string mood;
    std::string soundscape;
    int duration_minutes = 0;
    std::string timestamp_iso8601;
};

} // namespace tangent
