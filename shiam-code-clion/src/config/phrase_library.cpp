#include "phrase_library.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace tangent {

PhraseLibrary::PhraseLibrary(const std::string& jsonPath) : jsonPath_(jsonPath) {
    std::ifstream f(jsonPath);
    loaded_ = f.good();
    if (!loaded_) {
        std::cerr << "[phrases] could not open " << jsonPath << "\n";
    }
}

std::string PhraseLibrary::buildScript(const std::vector<std::string>& phraseIds) const {
    std::ifstream f(jsonPath_);
    if (!f.good()) return "";

    json lib;
    try {
        f >> lib;
    } catch (const std::exception& e) {
        std::cerr << "[phrases] failed to parse " << jsonPath_ << ": " << e.what() << "\n";
        return "";
    }

    std::string script;
    auto appendPhrase = [&](const std::string& key) {
        if (lib.contains(key) && lib[key].is_string()) {
            script += lib[key].get<std::string>() + " ";
        }
    };

    appendPhrase("intro");
    for (const auto& id : phraseIds) {
        if (lib.contains(id)) {
            appendPhrase(id);
        } else {
            std::cerr << "[phrases] unknown phrase_id '" << id << "' — skipping.\n";
        }
    }
    appendPhrase("outro");

    return script;
}

} // namespace tangent
