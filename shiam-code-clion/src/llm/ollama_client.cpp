#include "ollama_client.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

namespace tangent {

namespace {

size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t total = size * nmemb;
    out->append(static_cast<char*>(contents), total);
    return total;
}

// Ollama is generally well-behaved with json mode, but small/quantised models
// sometimes wrap output in ```json fences or add a stray sentence. This pulls
// out the first top-level {...} block so parsing doesn't fall over on stage.
std::string extractJsonBlock(const std::string& raw) {
    auto start = raw.find('{');
    auto end = raw.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end < start) {
        return raw;
    }
    return raw.substr(start, end - start + 1);
}

std::string buildSystemPrompt() {
    return R"(You are the session-planning brain inside an offline meditation device.
You receive a transcript of what the user just said out loud, plus their memory
context from previous sessions. Your job is to decide the shape of THIS session.

Respond with STRICT JSON ONLY. No markdown, no commentary, no code fences.
Schema:
{
  "mood": "<one or two words describing detected emotional state>",
  "soundscape": "<one of: ocean, rain, forest, bowls, white_noise>",
  "phrase_ids": ["<2 to 5 short snake_case ids describing phrase themes, e.g. grounding_breath, body_scan_intro, releasing_tension>"],
  "duration_minutes": <integer, 2 to 20>,
  "reasoning": "<one sentence explaining your choices, for transparency>"
}

If the user references a previous session (e.g. "same as last time", "longer than usual"),
honour that using the memory context provided. If memory context shows no history, treat
this as a first-time user and keep the session shorter and gentler.)";
}

std::string buildUserPrompt(const Transcript& transcript, const MemoryContext& memory) {
    std::ostringstream oss;
    oss << "User said: \"" << transcript.text << "\"\n\n";
    oss << "Memory context:\n";
    if (memory.has_history) {
        oss << "- Sessions so far: " << memory.session_count << "\n";
        oss << "- Last mood: " << memory.last_mood << "\n";
        oss << "- Last soundscape: " << memory.last_soundscape << "\n";
        oss << "- Last duration: " << memory.last_duration_minutes << " minutes\n";
        if (!memory.preference_notes.empty()) {
            oss << "- Notes: " << memory.preference_notes << "\n";
        }
    } else {
        oss << "- No prior sessions. This is a first-time user.\n";
    }
    oss << "\nRespond with the JSON object now.";
    return oss.str();
}

} // namespace

OllamaClient::OllamaClient(std::string model, std::string host)
    : model_(std::move(model)), host_(std::move(host)) {}

bool OllamaClient::healthCheck() const {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    std::string url = host_ + "/api/tags";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        return false;
    }

    // Bonus check: warn (but don't fail) if the requested model isn't pulled yet.
    try {
        auto j = json::parse(response);
        bool found = false;
        for (auto& m : j.value("models", json::array())) {
            std::string name = m.value("name", "");
            if (name == model_ || name.rfind(model_ + ":", 0) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "[ollama] warning: model '" << model_
                      << "' not found in `ollama list` — run `ollama pull " << model_
                      << "` before the demo.\n";
        }
    } catch (...) {
        // Non-fatal — health check on reachability already passed.
    }

    return true;
}

std::string OllamaClient::generate(const std::string& systemPrompt, const std::string& userPrompt) const {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    json payload = {
        {"model", model_},
        {"system", systemPrompt},
        {"prompt", userPrompt},
        {"stream", false},
        {"format", "json"},
        {"options", {{"temperature", 0.4}}}
    };
    std::string body = payload.dump();

    std::string response;
    std::string url = host_ + "/api/generate";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[ollama] request failed: " << curl_easy_strerror(res) << "\n";
        return "";
    }
    return response;
}

SessionPlan OllamaClient::planSession(const Transcript& transcript, const MemoryContext& memory) const {
    SessionPlan plan;

    std::string envelope = generate(buildSystemPrompt(), buildUserPrompt(transcript, memory));
    if (envelope.empty()) {
        plan.reasoning = "LLM unreachable — falling back to default plan.";
        plan.mood = "unknown";
        plan.soundscape = "rain";
        plan.phrase_ids = {"grounding_breath", "body_scan_intro"};
        plan.duration_minutes = 5;
        return plan;
    }

    try {
        json outer = json::parse(envelope);
        std::string innerText = outer.value("response", "");
        json inner = json::parse(extractJsonBlock(innerText));

        plan.mood = inner.value("mood", "unknown");
        plan.soundscape = inner.value("soundscape", "rain");
        plan.duration_minutes = inner.value("duration_minutes", 5);
        plan.reasoning = inner.value("reasoning", "");
        if (inner.contains("phrase_ids") && inner["phrase_ids"].is_array()) {
            for (auto& p : inner["phrase_ids"]) {
                if (p.is_string()) plan.phrase_ids.push_back(p.get<std::string>());
            }
        }
        if (plan.phrase_ids.empty()) {
            plan.phrase_ids = {"grounding_breath"};
        }
        plan.valid = true;
    } catch (const std::exception& e) {
        std::cerr << "[ollama] failed to parse model output as JSON: " << e.what() << "\n";
        std::cerr << "[ollama] raw envelope was: " << envelope << "\n";
        plan.mood = "unknown";
        plan.soundscape = "rain";
        plan.phrase_ids = {"grounding_breath"};
        plan.duration_minutes = 5;
        plan.reasoning = "Fallback plan — model output failed to parse.";
        plan.valid = false;
    }

    return plan;
}

} // namespace tangent
