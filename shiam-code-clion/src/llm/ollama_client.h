#pragma once

#include <string>
#include "../config/session_types.h"

namespace tangent {

// Thin wrapper around Ollama's local REST API (/api/generate).
// Talks to http://localhost:11434 by default — change via OLLAMA_HOST env var.
class OllamaClient {
public:
    explicit OllamaClient(std::string model, std::string host = "http://localhost:11434");

    // Returns true if Ollama is reachable and the model is loaded/loadable.
    bool healthCheck() const;

    // Sends the transcript + memory context, gets back a structured SessionPlan.
    // Internally prompts the model to respond with strict JSON and parses it.
    SessionPlan planSession(const Transcript& transcript, const MemoryContext& memory) const;

private:
    std::string model_;
    std::string host_;

    // Low-level POST to /api/generate, non-streaming, returns raw response text.
    std::string generate(const std::string& systemPrompt, const std::string& userPrompt) const;
};

} // namespace tangent
