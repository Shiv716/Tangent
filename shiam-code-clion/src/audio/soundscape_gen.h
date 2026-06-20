#pragma once

#include <string>

namespace tangent {

// Generates a loopable ambient WAV for one of: ocean, rain, forest, bowls,
// white_noise — and caches it to assets/soundscapes/<name>.wav. Returns the
// path. If a file already exists at that path (e.g. you've swapped in a
// real recording), it's used as-is and nothing is regenerated.
//
// This exists so the device's "no last-minute downloads, nothing leaves or
// enters the device at runtime" story holds even for the demo: every
// soundscape is generated locally from noise + simple synthesis, not
// fetched from anywhere.
std::string ensureSoundscape(const std::string& name, const std::string& assetsDir);

} // namespace tangent
