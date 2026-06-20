#!/usr/bin/env python3
"""
cognee_memory.py — memory bridge for the stillness.device PoC.

Called as a subprocess from C++ (see src/memory/cognee_bridge.cpp).
Protocol is deliberately dumb and synchronous: one invocation, one answer,
JSON on stdout, nothing else on stdout (logs go to stderr).

  python3 cognee_memory.py read
      -> prints MemoryContext JSON to stdout

  python3 cognee_memory.py write /path/to/session_record.json
      -> reads a SessionRecord JSON from the given path, persists it,
         prints {"ok": true} to stdout

Two backends:
  - LOCAL (default): a flat JSON file at ./memory_store.json. Zero
    dependencies, zero network calls, instant. This is what runs unless
    you explicitly opt in to Cognee.
  - COGNEE (opt-in via TANGENT_USE_COGNEE=1): uses the real Cognee SDK
    to add/cognify/search session history as a knowledge graph. Cognee's
    extraction pipeline calls an LLM itself — point it at your local
    Ollama instance (see configure_cognee_for_ollama() below) so nothing
    leaves the device. Wrapped in try/except end-to-end: any failure
    (Cognee not installed, graph DB not initialised, model not pulled,
    etc.) falls straight back to LOCAL so a demo never dies on stage.

For the hackathon table demo, LOCAL is the safe default. Flip the env
var once Cognee + your local LLM provider are confirmed working in your
environment, ideally tested well before you're standing at the booth.
"""

import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

STORE_PATH = Path(__file__).resolve().parent.parent / "memory_store.json"
USE_COGNEE = os.environ.get("TANGENT_USE_COGNEE", "0") == "1"


def log(msg: str) -> None:
    print(f"[cognee_memory] {msg}", file=sys.stderr)


# ---------------------------------------------------------------------------
# LOCAL backend — flat JSON file, always available
# ---------------------------------------------------------------------------

def local_load() -> dict:
    if not STORE_PATH.exists():
        return {"sessions": []}
    try:
        return json.loads(STORE_PATH.read_text())
    except Exception:
        return {"sessions": []}


def local_save(data: dict) -> None:
    STORE_PATH.write_text(json.dumps(data, indent=2))


def local_read_context() -> dict:
    data = local_load()
    sessions = data.get("sessions", [])
    if not sessions:
        return {
            "has_history": False,
            "last_mood": "",
            "last_soundscape": "",
            "last_duration_minutes": 0,
            "preference_notes": "",
            "session_count": 0,
        }
    last = sessions[-1]
    moods = [s["mood"] for s in sessions[-5:]]
    notes = f"Recent moods: {', '.join(moods)}" if len(sessions) > 1 else ""
    return {
        "has_history": True,
        "last_mood": last.get("mood", ""),
        "last_soundscape": last.get("soundscape", ""),
        "last_duration_minutes": last.get("duration_minutes", 0),
        "preference_notes": notes,
        "session_count": len(sessions),
    }


def local_write_session(record: dict) -> None:
    data = local_load()
    data.setdefault("sessions", []).append(record)
    local_save(data)


# ---------------------------------------------------------------------------
# COGNEE backend — opt-in, best-effort, never allowed to crash the caller
# ---------------------------------------------------------------------------

def configure_cognee_for_ollama():
    """
    Point Cognee's internal LLM calls at the local Ollama instance instead
    of a cloud provider, so the 'zero data leaves the device' claim holds
    even during memory extraction. Adjust model name to whatever you've
    pulled locally.
    """
    import cognee

    cognee.config.set_llm_provider("ollama")
    cognee.config.set_llm_model(os.environ.get("TANGENT_OLLAMA_MODEL", "phi3"))
    cognee.config.set_llm_endpoint(os.environ.get("OLLAMA_HOST", "http://localhost:11434"))


async def cognee_read_context() -> dict:
    import cognee

    configure_cognee_for_ollama()
    results = await cognee.search(
        query_text="What is the user's most recent meditation session: mood, soundscape, duration?",
        query_type="GRAPH_COMPLETION",
    )
    # Cognee's search returns free-form/graph results — for the PoC we fold
    # this into the same shape the LOCAL backend produces, and also keep a
    # running LOCAL copy so duration/mood fields stay structured and reliable
    # even if graph search phrasing varies.
    local_ctx = local_read_context()
    if results:
        local_ctx["preference_notes"] = (
            local_ctx.get("preference_notes", "") + " | cognee: " + str(results)[:300]
        )
    return local_ctx


async def cognee_write_session(record: dict) -> None:
    import cognee

    configure_cognee_for_ollama()
    text = (
        f"Meditation session on {record.get('timestamp_iso8601')}: "
        f"mood={record.get('mood')}, soundscape={record.get('soundscape')}, "
        f"duration={record.get('duration_minutes')} minutes."
    )
    await cognee.add(text)
    await cognee.cognify()
    # Always also keep the structured LOCAL record — it's the source of
    # truth for fields the orchestrator depends on (duration, mood, etc).
    local_write_session(record)


# ---------------------------------------------------------------------------
# Entry points
# ---------------------------------------------------------------------------

def do_read() -> dict:
    if USE_COGNEE:
        try:
            import asyncio
            return asyncio.run(cognee_read_context())
        except Exception as e:
            log(f"Cognee read failed ({e}), falling back to local store")
    return local_read_context()


def do_write(record: dict) -> None:
    if USE_COGNEE:
        try:
            import asyncio
            asyncio.run(cognee_write_session(record))
            return
        except Exception as e:
            log(f"Cognee write failed ({e}), falling back to local store")
    local_write_session(record)


def main():
    if len(sys.argv) < 2:
        log("usage: cognee_memory.py [read | write <record.json>]")
        sys.exit(1)

    command = sys.argv[1]

    if command == "read":
        ctx = do_read()
        print(json.dumps(ctx))

    elif command == "write":
        if len(sys.argv) < 3:
            log("write requires a path to a session record JSON file")
            sys.exit(1)
        record_path = Path(sys.argv[2])
        record = json.loads(record_path.read_text())
        record.setdefault(
            "timestamp_iso8601", datetime.now(timezone.utc).isoformat()
        )
        do_write(record)
        print(json.dumps({"ok": True}))

    else:
        log(f"unknown command: {command}")
        sys.exit(1)


if __name__ == "__main__":
    main()
