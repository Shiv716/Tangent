# stillness.device — Proof of Concept

A working C++ implementation of the **stillness.device** pipeline: an on-device
AI meditation system that listens, understands, and responds — with **zero data
ever leaving the device**.

This PoC runs the full pipeline from the architecture diagram on a single
machine (your laptop / RTX 5090 for the hackathon, a Raspberry Pi 5 for the
final device):

```
Mic → Whisper (STT) → Memory (read) → Ollama LLM (decide) → Memory (write) → Piper TTS + Soundscape → Speaker
```

It is a **push-to-talk CLI**, not the headless always-listening device — a
deliberate simplification so a live demo is predictable. Everything that runs
during a session runs **locally and offline**.

---

## Why this is "offline by construction"

Two senses of offline matter, and this project handles both:

1. **The build needs no internet.** `whisper.cpp` (plus its `ggml` backend) and
   `nlohmann/json` are *vendored as real source* under `third_party/`, and CMake
   builds them from there — there is no `FetchContent`, no `git submodule`, no
   download step at configure time. The only system packages required are the
   three standard audio/HTTP libraries below.

2. **The runtime needs no internet.** Once the models are present (one-time
   download via `scripts/setup.sh`), nothing in a meditation session touches the
   network. Whisper, the LLM, memory, and TTS all run on-device.

The one unavoidable online step is downloading the model weights themselves
(Whisper `.bin`, Piper voice `.onnx`, and the Ollama model). Those are too large
and too license-bound to commit to git, so `scripts/setup.sh` fetches them once.
After that you can pull the network cable and the demo still works.

---

## Prerequisites

System packages (Ubuntu / Debian):

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
                 libsdl2-dev libsdl2-mixer-dev libcurl4-openssl-dev
```

For GPU-accelerated Whisper (recommended on the 5090): the **CUDA toolkit**
(`nvcc` on `PATH`). Without it, build with `-DTANGENT_USE_CUDA=OFF` for a
CPU-only binary.

Services / tools fetched by setup:
- **Ollama** — install from https://ollama.com/download (system service).
- **Piper** and a voice — installed via pip by `setup.sh`.
- **Whisper model** — fetched by `setup.sh` using the vendored download script.

---

## One-time setup

With internet available, run:

```bash
bash scripts/setup.sh
```

This downloads into `models/`:
- `ggml-small.en.bin` — Whisper STT model
- `en_GB-alba-medium.onnx` (+ `.json`) — Piper voice
- pulls the `phi3` model into Ollama

You can override choices via env vars, e.g.:

```bash
WHISPER_MODEL=base.en PIPER_VOICE=en_GB-cori-medium OLLAMA_MODEL=phi3 bash scripts/setup.sh
```

Make sure Ollama is running before the demo:

```bash
ollama serve        # or just: ollama run phi3
```

---

## Building in CLion

1. **Open the project**: File → Open → select the project root folder (the one
   containing the top-level `CMakeLists.txt`). CLion auto-detects CMake.

2. **Set the build profile**: Settings → Build, Execution, Deployment → CMake.
   - For the 5090: leave CMake options empty (CUDA is ON by default).
   - For a CPU-only machine: add `-DTANGENT_USE_CUDA=OFF` to *CMake options*.
   - Build type: `Release` (much faster Whisper inference than Debug).

3. **Let CLion configure**: it runs CMake, which builds the vendored
   `whisper.cpp` the first time (a few minutes; subsequent builds are
   incremental).

4. **Set the working directory** (important — the app uses relative paths for
   `assets/`, `scripts/`, `models/`):
   Run → Edit Configurations → select `tangent_poc` →
   set **Working directory** to the **project root** (`$ProjectFileDir$`).

5. **Build & Run** the `tangent_poc` target (the green hammer / play button).

### Or build from the command line

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..        # add -DTANGENT_USE_CUDA=OFF if no CUDA
cmake --build . -j$(nproc)
cd ..                                      # run from project root
./build/bin/tangent_poc
```

---

## Running the demo

From the project root:

```bash
./build/bin/tangent_poc
```

Flow per session:
1. Press **Enter** to start talking.
2. Speak (e.g. *"I'm feeling really anxious today, my mind won't stop racing"*).
3. Press **Enter** to stop.
4. Watch the pipeline log each stage: transcript → memory → LLM plan → TTS.
5. The meditation plays (guided voice over an ambient soundscape).
6. The session is written to memory.

Next session, try *"Same as last time but longer"* to demonstrate memory recall.

CLI overrides (all optional — defaults match `setup.sh`):

```bash
./build/bin/tangent_poc \
  --whisper-model models/ggml-small.en.bin \
  --piper-voice   models/en_GB-alba-medium.onnx \
  --ollama-model  phi3 \
  --ollama-host   http://localhost:11434
```

---

## Project layout

```
tangent-poc/
├── CMakeLists.txt              # builds from vendored sources (no fetch)
├── scripts/
│   ├── setup.sh                # one-time online model download
│   └── cognee_memory.py        # memory bridge (local JSON default / Cognee opt-in)
├── src/
│   ├── main.cpp                # orchestrator — the push-to-talk loop
│   ├── audio/
│   │   ├── mic_capture.*       # SDL2 push-to-talk capture (16kHz mono f32)
│   │   ├── playback.*          # SDL_mixer voice + soundscape mixing
│   │   ├── soundscape_gen.*    # procedural ambient WAV generation
│   │   └── wav_writer.h        # minimal 16-bit PCM WAV writer
│   ├── stt/whisper_engine.*    # whisper.cpp wrapper (library, CUDA-capable)
│   ├── llm/ollama_client.*     # Ollama REST client → structured SessionPlan
│   ├── memory/cognee_bridge.*  # C++→python subprocess bridge
│   ├── tts/piper_engine.*      # Piper CLI wrapper
│   └── config/
│       ├── session_types.h     # shared structs
│       └── phrase_library.*    # phrase_id → spoken text resolver
├── assets/
│   ├── phrases/library.json    # meditation phrase library
│   └── soundscapes/            # generated on first run
├── models/                     # downloaded weights (gitignored)
└── third_party/                # VENDORED whisper.cpp + nlohmann/json
```

---

## Memory: local JSON vs Cognee

The memory engine has two backends (see `scripts/cognee_memory.py`):

- **LOCAL (default):** a flat `memory_store.json`. Zero dependencies, instant,
  never fails. This is what runs unless you opt in. Safe for a live demo.
- **COGNEE (opt-in):** real Cognee knowledge-graph memory. Enable with
  `TANGENT_USE_COGNEE=1` and `pip install cognee`. Point Cognee's own LLM calls
  at local Ollama (see `configure_cognee_for_ollama()`) so the offline guarantee
  holds even during memory extraction. Any failure falls back to LOCAL
  automatically, so the pipeline never breaks on stage.

To demonstrate the Cognee track partner integration, flip the flag **after**
confirming it works in your environment well before the demo.

---

## Mapping to the device (later)

| PoC (this repo)              | Final device                          |
| ---------------------------- | ------------------------------------- |
| Laptop + RTX 5090            | Raspberry Pi 5 (8GB)                   |
| SDL2 mic (push-to-talk)      | I2S MEMS mic + wake word (always-on)  |
| SDL_mixer → laptop speakers  | I2S DAC + amplifier + speaker         |
| `--ollama-model phi3`        | Phi-3 Mini Q4 via Ollama on ARM       |
| local JSON / Cognee          | Cognee on encrypted SQLite            |
| NFC config transfer          | (companion app — not in this PoC)     |

Only `mic_capture.cpp` and `playback.cpp` need hardware-specific changes to move
from laptop to Pi; the pipeline logic is identical.

---

## License

MIT (see `LICENSE`).
