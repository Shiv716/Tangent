#include "piper_engine.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sys/wait.h>

namespace fs = std::filesystem;

namespace tangent {

PiperEngine::PiperEngine(std::string piperBinary, std::string voiceModelPath)
    : piperBinary_(std::move(piperBinary)), voiceModelPath_(std::move(voiceModelPath)) {}

bool PiperEngine::isAvailable() const {
    std::string check = "command -v " + piperBinary_ + " > /dev/null 2>&1";
    return std::system(check.c_str()) == 0;
}

std::string PiperEngine::synthesize(const std::string& text) const {
    fs::path inputPath = fs::temp_directory_path() / "tangent_piper_input.txt";
    fs::path outputPath = fs::temp_directory_path() / "tangent_piper_output.wav";

    std::ofstream in(inputPath);
    if (!in) {
        std::cerr << "[piper] could not write input text file\n";
        return "";
    }
    in << text;
    in.close();

    // Piper finds <model>.onnx.json automatically when it sits beside the
    // .onnx, but we pass it explicitly so a non-standard layout still works.
    std::string configFlag;
    std::string configPath = voiceModelPath_ + ".json";
    if (fs::exists(configPath)) {
        configFlag = " --config " + configPath;
    }

    std::string cmd = piperBinary_ +
        " --model " + voiceModelPath_ +
        configFlag +
        " --input-file " + inputPath.string() +
        " --output_file " + outputPath.string() +
        " > /tmp/tangent_piper_stdout.log 2>&1";

    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        std::cerr << "[piper] synthesis failed (exit " << rc
                  << "). See /tmp/tangent_piper_stdout.log\n";
        return "";
    }

    if (!fs::exists(outputPath)) {
        std::cerr << "[piper] expected output WAV not found at " << outputPath << "\n";
        return "";
    }

    return outputPath.string();
}

} // namespace tangent
