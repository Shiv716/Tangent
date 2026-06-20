cmake_minimum_required(VERSION 3.16)
project(tangent_poc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()


# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
# CUDA ON by default — your laptop's RTX 5090 will accelerate Whisper.
# Turn OFF (-DTANGENT_USE_CUDA=OFF) to build a CPU-only binary, e.g. when
# testing on a machine without the CUDA toolkit installed.
option(TANGENT_USE_CUDA "Build whisper.cpp with CUDA acceleration" ON)

# ---------------------------------------------------------------------------
# System dependencies (audio + http).
# These three are standard distro packages and stay as system deps:
#   sudo apt install libsdl2-dev libsdl2-mixer-dev libcurl4-openssl-dev
# Everything else (whisper.cpp, ggml, nlohmann/json) is VENDORED under
# third_party/ so the build needs no internet access.
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
find_package(CURL REQUIRED)

# ---------------------------------------------------------------------------
# Vendored whisper.cpp (third_party/whisper.cpp) — built from local source,
# NOT fetched. This is what makes the build air-gap friendly.
# ---------------------------------------------------------------------------
if(TANGENT_USE_CUDA)
    set(GGML_CUDA ON CACHE BOOL "" FORCE)
endif()
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/whisper.cpp EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Vendored nlohmann/json (third_party/nlohmann/json.hpp) — header only.
# ---------------------------------------------------------------------------
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

# ---------------------------------------------------------------------------
# tangent_poc executable
# ---------------------------------------------------------------------------
add_executable(tangent_poc
    src/main.cpp
    src/audio/mic_capture.cpp
    src/audio/playback.cpp
    src/audio/soundscape_gen.cpp
    src/stt/whisper_engine.cpp
    src/llm/ollama_client.cpp
    src/memory/cognee_bridge.cpp
    src/tts/piper_engine.cpp
    src/config/phrase_library.cpp
)

target_include_directories(tangent_poc PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)

target_link_directories(tangent_poc PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_MIXER_LIBRARY_DIRS}
)

target_link_libraries(tangent_poc PRIVATE
    whisper                       # vendored, built via add_subdirectory above
    nlohmann_json                 # vendored header-only
    CURL::libcurl                 # system
    ${SDL2_LIBRARIES}             # system
    ${SDL2_MIXER_LIBRARIES}       # system
)

if(TANGENT_USE_CUDA)
    target_compile_definitions(tangent_poc PRIVATE GGML_USE_CUDA)
endif()

# Run from the project root in CLion so relative paths (assets/, scripts/,
# models/) resolve. See README.md "CLion run configuration".
set_target_properties(tangent_poc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "==========================================================")
message(STATUS " tangent_poc configured")
message(STATUS "   whisper.cpp : vendored (third_party/whisper.cpp)")
message(STATUS "   nlohmann    : vendored (third_party/nlohmann)")
message(STATUS "   CUDA accel  : ${TANGENT_USE_CUDA}")
message(STATUS "==========================================================")
