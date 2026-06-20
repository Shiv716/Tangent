#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# setup.sh — ONE-TIME online setup for stillness.device
#
# Downloads the three model artefacts the device needs and installs the
# Python-side tooling. Run this ONCE while you have internet. After it
# completes, the whole pipeline runs fully offline / air-gapped — which is
# the entire point of the product.
#
#   bash scripts/setup.sh
#
# What it does NOT do: it does not need to run at demo time, and nothing
# here is called during a meditation session.
# ---------------------------------------------------------------------------
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODELS_DIR="$PROJECT_ROOT/models"
mkdir -p "$MODELS_DIR"

echo "=================================================="
echo " stillness.device — one-time setup"
echo " Project root: $PROJECT_ROOT"
echo "=================================================="

# ---------------------------------------------------------------------------
# 1. Whisper STT model (GGML format, for whisper.cpp)
#    small.en is a good speed/quality balance on a 5090. Swap to base.en
#    for faster/lighter, or medium.en for higher accuracy.
# ---------------------------------------------------------------------------
WHISPER_MODEL="${WHISPER_MODEL:-small.en}"
WHISPER_FILE="$MODELS_DIR/ggml-${WHISPER_MODEL}.bin"

if [[ -f "$WHISPER_FILE" ]]; then
    echo "[1/4] Whisper model already present: $WHISPER_FILE"
else
    echo "[1/4] Downloading Whisper model: $WHISPER_MODEL"
    # Use the vendored download script from whisper.cpp.
    bash "$PROJECT_ROOT/third_party/whisper.cpp/scripts/download-ggml-model.sh" \
        "$WHISPER_MODEL" "$MODELS_DIR"
    # The script names it ggml-<model>.bin inside MODELS_DIR.
fi

# ---------------------------------------------------------------------------
# 2. Piper TTS + an en_GB voice
#    Installs the actively-maintained piper-tts package (OHF-Voice/piper1-gpl)
#    and pulls the alba (en_GB) medium voice into models/.
# ---------------------------------------------------------------------------
PIPER_VOICE="${PIPER_VOICE:-en_GB-alba-medium}"

echo "[2/4] Installing piper-tts (pip) ..."
pip install --quiet piper-tts || pip install --quiet --break-system-packages piper-tts

echo "[2/4] Downloading Piper voice: $PIPER_VOICE -> $MODELS_DIR"
if [[ -f "$MODELS_DIR/${PIPER_VOICE}.onnx" ]]; then
    echo "      Voice already present."
else
    python3 -m piper.download_voices "$PIPER_VOICE" --download-dir "$MODELS_DIR"
fi

# ---------------------------------------------------------------------------
# 3. Ollama + the LLM
#    We don't install Ollama for you (it's a system service), but we check
#    it's reachable and pull the model if so.
# ---------------------------------------------------------------------------
OLLAMA_MODEL="${OLLAMA_MODEL:-phi3}"
echo "[3/4] Checking Ollama ..."
if command -v ollama >/dev/null 2>&1; then
    echo "      ollama found. Pulling model: $OLLAMA_MODEL"
    ollama pull "$OLLAMA_MODEL" || echo "      (pull failed — make sure 'ollama serve' is running)"
else
    echo "      !! ollama not found on PATH."
    echo "      Install from https://ollama.com/download then run: ollama pull $OLLAMA_MODEL"
fi

# ---------------------------------------------------------------------------
# 4. Optional: Cognee (the memory backend). LOCAL JSON fallback works
#    without this; only needed if you set TANGENT_USE_COGNEE=1.
# ---------------------------------------------------------------------------
echo "[4/4] (optional) Cognee memory engine"
echo "      The PoC defaults to a local JSON memory store (no install needed)."
echo "      To use real Cognee: pip install cognee  and set TANGENT_USE_COGNEE=1"

echo ""
echo "=================================================="
echo " Setup complete. Artefacts in: $MODELS_DIR"
ls -lh "$MODELS_DIR" 2>/dev/null || true
echo ""
echo " Next: open the project in CLion, build, and run with:"
echo "   --whisper-model models/ggml-${WHISPER_MODEL}.bin \\"
echo "   --piper-voice  models/${PIPER_VOICE}.onnx \\"
echo "   --ollama-model ${OLLAMA_MODEL}"
echo " (these are also the defaults baked into main.cpp where they match)"
echo "=================================================="
