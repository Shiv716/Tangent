#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace tangent {

// Tiny header-only WAV writer. No dependencies, no edge cases beyond
// 16-bit PCM mono — exactly what we need to bake procedural soundscapes
// to disk once at startup.
inline bool writeWavMono16(const std::string& path,
                            const std::vector<int16_t>& samples,
                            int sampleRate) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    const int numChannels = 1;
    const int bitsPerSample = 16;
    const int byteRate = sampleRate * numChannels * bitsPerSample / 8;
    const int blockAlign = numChannels * bitsPerSample / 8;
    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    const uint32_t chunkSize = 36 + dataSize;

    fwrite("RIFF", 1, 4, f);
    fwrite(&chunkSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1; // PCM
    fwrite(&subchunk1Size, 4, 1, f);
    fwrite(&audioFormat, 2, 1, f);
    uint16_t ch = numChannels;
    fwrite(&ch, 2, 1, f);
    uint32_t sr = sampleRate;
    fwrite(&sr, 4, 1, f);
    uint32_t br = byteRate;
    fwrite(&br, 4, 1, f);
    uint16_t ba = blockAlign;
    fwrite(&ba, 2, 1, f);
    uint16_t bps = bitsPerSample;
    fwrite(&bps, 2, 1, f);

    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    fwrite(samples.data(), sizeof(int16_t), samples.size(), f);

    fclose(f);
    return true;
}

} // namespace tangent
