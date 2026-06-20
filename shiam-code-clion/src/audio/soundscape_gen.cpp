#include "soundscape_gen.h"
#include "wav_writer.h"

#include <cmath>
#include <random>
#include <vector>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tangent {

namespace {

constexpr int kSampleRate = 22050;
constexpr int kDurationSeconds = 12; // loop length

std::vector<float> whiteNoise(int n, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> out(n);
    for (auto& s : out) s = dist(rng);
    return out;
}

// One-pole low-pass: turns white noise progressively "browner"/softer as
// alpha decreases. Used as the base texture for rain/ocean/forest.
std::vector<float> lowPass(const std::vector<float>& in, float alpha) {
    std::vector<float> out(in.size());
    float prev = 0.0f;
    for (size_t i = 0; i < in.size(); ++i) {
        prev = alpha * in[i] + (1.0f - alpha) * prev;
        out[i] = prev;
    }
    return out;
}

void normalizeAndFade(std::vector<float>& buf) {
    float peak = 0.0001f;
    for (float v : buf) peak = std::max(peak, std::fabs(v));
    for (float& v : buf) v /= peak * 1.2f; // leave headroom

    // Short fade in/out so the loop point doesn't click.
    int fadeLen = std::min<int>(static_cast<int>(buf.size()) / 20, kSampleRate / 4);
    for (int i = 0; i < fadeLen; ++i) {
        float g = static_cast<float>(i) / fadeLen;
        buf[i] *= g;
        buf[buf.size() - 1 - i] *= g;
    }
}

std::vector<int16_t> toInt16(const std::vector<float>& buf) {
    std::vector<int16_t> out(buf.size());
    for (size_t i = 0; i < buf.size(); ++i) {
        float clamped = std::max(-1.0f, std::min(1.0f, buf[i]));
        out[i] = static_cast<int16_t>(clamped * 32000.0f);
    }
    return out;
}

std::vector<float> genRain(std::mt19937& rng) {
    int n = kSampleRate * kDurationSeconds;
    auto noise = whiteNoise(n, rng);
    auto base = lowPass(noise, 0.35f); // hiss-like
    // Layer in sparse "droplet" ticks via brief amplitude spikes.
    std::uniform_int_distribution<int> gapDist(200, 1200);
    std::uniform_real_distribution<float> ampDist(0.3f, 0.8f);
    int i = 0;
    while (i < n - 50) {
        i += gapDist(rng);
        if (i >= n - 50) break;
        float amp = ampDist(rng);
        for (int k = 0; k < 40 && i + k < n; ++k) {
            base[i + k] += amp * std::exp(-k / 8.0f) * (k % 2 == 0 ? 1.0f : -0.6f);
        }
    }
    return base;
}

std::vector<float> genOcean(std::mt19937& rng) {
    int n = kSampleRate * kDurationSeconds;
    auto noise = whiteNoise(n, rng);
    auto base = lowPass(noise, 0.15f); // deeper rumble
    // Slow swell envelope, ~0.1Hz, like waves rolling in.
    for (int i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float swell = 0.5f + 0.5f * std::sin(2.0 * M_PI * 0.08 * t);
        base[i] *= (0.3f + 0.7f * swell);
    }
    return base;
}

std::vector<float> genForest(std::mt19937& rng) {
    int n = kSampleRate * kDurationSeconds;
    auto noise = whiteNoise(n, rng);
    auto base = lowPass(noise, 0.5f);
    for (auto& v : base) v *= 0.25f; // quiet ambient bed
    // Occasional bird-like chirps: short sine sweeps.
    std::uniform_int_distribution<int> gapDist(kSampleRate, kSampleRate * 3);
    std::uniform_real_distribution<float> freqDist(1800.0f, 3200.0f);
    int i = 0;
    while (i < n - kSampleRate / 5) {
        i += gapDist(rng);
        if (i >= n - kSampleRate / 5) break;
        int chirpLen = kSampleRate / 8;
        float f0 = freqDist(rng);
        for (int k = 0; k < chirpLen && i + k < n; ++k) {
            float t = static_cast<float>(k) / kSampleRate;
            float freq = f0 * (1.0f + t * 1.5f); // upward sweep
            float env = std::sin(M_PI * k / chirpLen); // smooth in/out
            base[i + k] += 0.35f * env * std::sin(2.0 * M_PI * freq * t);
        }
    }
    return base;
}

std::vector<float> genBowls(std::mt19937& rng) {
    int n = kSampleRate * kDurationSeconds;
    std::vector<float> base(n, 0.0f);
    // A few harmonically-related sustained tones, each with slow tremolo,
    // approximating a singing bowl's overtone-rich drone.
    std::vector<float> freqs = {196.0f, 294.0f, 392.0f, 587.0f}; // G3, D4, G4, D5-ish
    std::uniform_real_distribution<float> phaseDist(0.0f, 2.0f * M_PI);
    for (float f : freqs) {
        float phase = phaseDist(rng);
        float tremoloRate = 0.15f + 0.05f * (f / 200.0f);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            float tremolo = 0.6f + 0.4f * std::sin(2.0 * M_PI * tremoloRate * t + phase);
            base[i] += (0.15f / freqs.size()) * tremolo * std::sin(2.0 * M_PI * f * t + phase);
        }
    }
    return base;
}

std::vector<float> genWhiteNoise(std::mt19937& rng) {
    int n = kSampleRate * kDurationSeconds;
    auto noise = whiteNoise(n, rng);
    for (auto& v : noise) v *= 0.3f;
    return noise;
}

} // namespace

std::string ensureSoundscape(const std::string& name, const std::string& assetsDir) {
    fs::path dir = fs::path(assetsDir) / "soundscapes";
    fs::create_directories(dir);
    fs::path path = dir / (name + ".wav");

    if (fs::exists(path)) {
        return path.string();
    }

    std::cerr << "[soundscape] generating '" << name << "' procedurally (first run only)\n";

    std::mt19937 rng(std::hash<std::string>{}(name)); // deterministic per-name
    std::vector<float> buf;

    if (name == "rain") buf = genRain(rng);
    else if (name == "ocean") buf = genOcean(rng);
    else if (name == "forest") buf = genForest(rng);
    else if (name == "bowls") buf = genBowls(rng);
    else buf = genWhiteNoise(rng); // covers "white_noise" and any unrecognised name

    normalizeAndFade(buf);
    auto pcm = toInt16(buf);

    if (!writeWavMono16(path.string(), pcm, kSampleRate)) {
        std::cerr << "[soundscape] failed to write " << path << "\n";
        return "";
    }

    return path.string();
}

} // namespace tangent
