#pragma once

#include <string>
#include <vector>

namespace tangent {

// Loads assets/phrases/library.json and resolves the LLM's chosen
// phrase_ids into the actual spoken-text script for Piper. Unknown ids
// are skipped (logged, not fatal) so a slightly-off LLM output never
// kills the demo.
class PhraseLibrary {
public:
    explicit PhraseLibrary(const std::string& jsonPath);

    bool isLoaded() const { return loaded_; }

    // Joins intro + each resolved phrase + outro into one script,
    // separated by pauses, ready to hand to Piper.
    std::string buildScript(const std::vector<std::string>& phraseIds) const;

private:
    bool loaded_ = false;
    std::string jsonPath_;
};

} // namespace tangent
