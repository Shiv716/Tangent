# Project Tangent: stillness.device

**An on-device AI meditation system that listens, understands, and responds — with zero data ever leaving the device.**

Built for [Localhost: On-Device Agent Hackathon](https://luma.com/8og1gx56) · 20 June 2026 · London

> 🏛 Hackathon Track Partners: **Cognee** · **ElevenLabs**
 
> 🔗 **MVP:** [stillness-tap-config.lovable.app](https://stillness-tap-config.lovable.app/library)

> 🎥 **Demo Video:** [Watch the walkthrough](https://youtu.be/WapV_jvI01Y)
---

## Problem Statement

Meditation apps today require you to hand over your most vulnerable moments to the cloud. When you tell Calm or Headspace that you're anxious, that you can't sleep, that your mind won't stop racing — that data travels to servers you don't control, gets processed by systems you can't audit, and lives in databases you can't delete from.

This creates a fundamental trust problem. The people who need meditation most — those dealing with anxiety, insomnia, chronic stress — are the least likely to speak candidly to a device they know is transmitting their words to a third party. The result is that AI-powered meditation tools are either generic (because users don't share enough for personalisation to work) or invasive (because personalisation requires sensitive data to leave the user's hands).

Existing solutions also lack memory. Every session starts from zero. The app doesn't know that you've been anxious three nights in a row, that ocean sounds work better for you than forest sounds, or that you prefer shorter sessions with minimal guidance. There is no continuity, no adaptation, no learning.

The hardware landscape offers neurofeedback wearables (Muse, Pulsetto, Sens.ai) that provide biometric data but no generative intelligence — they can measure your brain state but can't hold a conversation, interpret your words, or compose a personalised meditation in real time. On the software side, voice-enabled AI assistants (Alexa, Siri, Google) can generate responses but send everything to the cloud and have no meditation-specific intelligence.

No product currently combines voice-driven AI, personalised meditation generation, adaptive memory, and absolute data privacy in a single device.

---

## Solution

**stillness.device** is a dedicated physical meditation device powered by a Raspberry Pi 5 that runs entirely offline. The user speaks to it, describes how they feel, and the device produces a personalised meditation session — guided voice layered over ambient soundscapes — all processed and stored locally on the board itself.

The device runs a complete AI pipeline on-chip: speech-to-text (Whisper), an LLM for mood interpretation and session planning (Ollama/Phi-3), a persistent memory engine that learns across sessions (Cognee), and neural text-to-speech for voice output (Piper TTS). No component requires an internet connection. No data is transmitted. The device has no active network interface during operation.

A companion web app allows users to browse and customise creative meditation configurations — voice presets, soundscape selections, phrase libraries, session parameters. These configs are transferred to the device via NFC tap. The transfer is strictly one-way: config data flows into the device, no user data flows out. The app itself works offline after initial load.

The core differentiators are the intersection of four properties that no existing product combines: a purpose-built hardware form factor for meditation, generative AI that interprets and responds to the user's emotional state, persistent memory that adapts across sessions, and an air-gapped architecture where zero data leaves the device under any circumstance.

---

## Architecture

```
Mic → Whisper (STT) → Cognee (read memory) → Ollama LLM (decide) → Cognee (write session) → Piper TTS + Soundscape → Speaker
```

All components run on a single Raspberry Pi 5. Fully offline. Zero data exfiltration surface.

**[→ View the interactive architecture diagram](https://claude.ai/public/artifacts/f510e760-b8cb-4c95-9713-97c70c11a01a)**

The system is divided into four zones:

| Zone | Role | Components |
|------|------|------------|
| **Frontend App** | Config creation and delivery | Config Store, Config Builder, Offline Cache |
| **NFC Bridge** | One-way config transfer (app → device) | NFC Type 4 NDEF payload |
| **Raspberry Pi 5** | On-device AI pipeline | Whisper, Cognee, Ollama, Piper, Soundscape Engine |
| **Local Storage** | Encrypted on-device persistence | SQLite (Cognee DB), Config files, Audio assets |

A hard security boundary separates the app from the device. Config data crosses via physical NFC tap. User voice data, session history, mood patterns, and preference profiles never cross this boundary in any direction.

### Architecture Diagram Source

The interactive architecture diagram is a React component with animated data flows, clickable nodes with full tech specs, and flow filters (Full System, Config Flow, Voice Pipeline, Memory Loop). The source is embedded below for portability.

<details>
<summary><strong>stillness-architecture.jsx</strong> (click to expand)</summary>

```jsx
import { useState, useEffect, useRef, useCallback } from "react";

const NODES = {
  // App Side
  configStore: { x: 80, y: 100, w: 150, h: 56, label: "Config Store", sub: "Voice packs · Soundscapes · Phrases", zone: "app", icon: "◈" },
  configBuilder: { x: 80, y: 200, w: 150, h: 56, label: "Config Builder", sub: "Customise session parameters", zone: "app", icon: "◆" },
  appOffline: { x: 80, y: 300, w: 150, h: 56, label: "Offline Cache", sub: "Configs stored locally on app", zone: "app", icon: "◫" },

  // NFC Bridge
  nfc: { x: 355, y: 200, w: 110, h: 56, label: "NFC Transfer", sub: "One-way config push", zone: "bridge", icon: "⟐" },

  // Device Side - Input
  mic: { x: 570, y: 68, w: 130, h: 50, label: "Microphone", sub: "Voice capture · I2S MEMS", zone: "device", icon: "◯" },

  // Device Side - Pipeline
  whisper: { x: 570, y: 148, w: 130, h: 50, label: "Whisper STT", sub: "Speech → Text · tiny.en", zone: "device", icon: "▤" },
  cogneeRead: { x: 570, y: 228, w: 130, h: 50, label: "Cognee Read", sub: "Fetch user memory", zone: "device", icon: "◰" },
  ollama: { x: 570, y: 308, w: 130, h: 50, label: "Ollama LLM", sub: "Phi-3 Mini · Decision", zone: "device", icon: "⬡" },
  cogneeWrite: { x: 570, y: 388, w: 130, h: 50, label: "Cognee Write", sub: "Store session data", zone: "device", icon: "◱" },
  piper: { x: 570, y: 468, w: 130, h: 50, label: "Piper TTS", sub: "Text → Voice · ONNX", zone: "device", icon: "▥" },

  // Device Side - Output
  soundscape: { x: 740, y: 388, w: 130, h: 50, label: "Soundscape", sub: "Ambient audio engine", zone: "device", icon: "≋" },
  speaker: { x: 740, y: 468, w: 130, h: 50, label: "Speaker", sub: "I2S DAC + Amplifier", zone: "device", icon: "◉" },

  // Storage
  cogneeDb: { x: 740, y: 228, w: 130, h: 50, label: "Local Memory", sub: "SQLite · On-device only", zone: "storage", icon: "⊡" },
  configFiles: { x: 740, y: 148, w: 130, h: 50, label: "Config Files", sub: "NFC-received configs", zone: "storage", icon: "⊞" },
};

const CONNECTIONS = [
  // App flow
  { from: "configStore", to: "configBuilder", label: "templates", zone: "app" },
  { from: "configBuilder", to: "appOffline", label: "save", zone: "app" },
  { from: "appOffline", to: "nfc", label: "config payload", zone: "bridge" },

  // NFC to device
  { from: "nfc", to: "configFiles", label: "one-way push", zone: "bridge", isOneWay: true },

  // Device pipeline
  { from: "mic", to: "whisper", label: "raw audio", zone: "device" },
  { from: "whisper", to: "cogneeRead", label: "transcript", zone: "device" },
  { from: "cogneeRead", to: "ollama", label: "context + memory", zone: "device" },
  { from: "ollama", to: "cogneeWrite", label: "session data", zone: "device" },
  { from: "ollama", to: "soundscape", label: "sound selection", zone: "device" },
  { from: "cogneeWrite", to: "piper", label: "phrase sequence", zone: "device" },
  { from: "piper", to: "speaker", label: "voice audio", zone: "device" },
  { from: "soundscape", to: "speaker", label: "ambient mix", zone: "device" },

  // Storage connections
  { from: "cogneeRead", to: "cogneeDb", label: "query", zone: "storage", dashed: true },
  { from: "cogneeWrite", to: "cogneeDb", label: "persist", zone: "storage", dashed: true },
  { from: "configFiles", to: "cogneeRead", label: "preferences", zone: "storage", dashed: true },
];

const ZONE_COLORS = {
  app: { fill: "#0F2E4A", stroke: "#1B6B93", text: "#5CB8E4", accent: "#5CB8E4" },
  bridge: { fill: "#2A1A3E", stroke: "#7B3FA0", text: "#C490D1", accent: "#C490D1" },
  device: { fill: "#0A2A1F", stroke: "#1A7A50", text: "#4ADE80", accent: "#4ADE80" },
  storage: { fill: "#1A1A0A", stroke: "#6B6B2A", text: "#BDB76B", accent: "#BDB76B" },
};

function AnimatedParticle({ x1, y1, x2, y2, color, delay, duration }) {
  const [pos, setPos] = useState(0);
  useEffect(() => {
    const timeout = setTimeout(() => {
      const interval = setInterval(() => {
        setPos(p => (p >= 1 ? 0 : p + 0.02));
      }, duration / 50);
      return () => clearInterval(interval);
    }, delay);
    return () => clearTimeout(timeout);
  }, [delay, duration]);

  const cx = x1 + (x2 - x1) * pos;
  const cy = y1 + (y2 - y1) * pos;
  const opacity = pos < 0.1 ? pos * 10 : pos > 0.9 ? (1 - pos) * 10 : 1;

  return (
    <g>
      <circle cx={cx} cy={cy} r={2.5} fill={color} opacity={opacity * 0.9}>
        <animate attributeName="r" values="2;3.5;2" dur="1s" repeatCount="indefinite" />
      </circle>
      <circle cx={cx} cy={cy} r={6} fill={color} opacity={opacity * 0.15}>
        <animate attributeName="r" values="4;8;4" dur="1s" repeatCount="indefinite" />
      </circle>
    </g>
  );
}

function NodeDetail({ node, nodeKey, onClose }) {
  const details = {
    configStore: {
      title: "Configuration Store",
      specs: ["Pre-built meditation templates", "ElevenLabs-generated phrase packs", "Soundscape audio bundles", "Community-shared configs"],
      tech: "JSON config manifests + WAV audio bundles"
    },
    configBuilder: {
      title: "Configuration Builder",
      specs: ["Voice tone selection (calm, warm, deep)", "Soundscape picker", "Session duration presets", "Guidance intensity control"],
      tech: "React PWA · works offline after first load"
    },
    appOffline: {
      title: "Offline Cache",
      specs: ["Configs stored in browser IndexedDB", "Full offline functionality", "Queue configs for next NFC tap", "No account or login required"],
      tech: "Service Worker + IndexedDB"
    },
    nfc: {
      title: "NFC Data Transfer",
      specs: ["Config data flows TO device only", "No data returns from device", "Physical tap required — no wireless", "Payload: ~2-50 KB JSON + file refs"],
      tech: "NFC Type 4 · NDEF message format"
    },
    mic: {
      title: "Microphone Input",
      specs: ["I2S MEMS microphone (INMP441)", "Connected via GPIO pins", "16-bit, 16kHz sample rate", "Always-listening with wake detection"],
      tech: "I2S protocol · ALSA driver"
    },
    whisper: {
      title: "Whisper Speech-to-Text",
      specs: ["OpenAI Whisper tiny.en model", "~75MB model file on SD card", "Real-time transcription on Pi 5", "English-optimised for low latency"],
      tech: "whisper.cpp · GGML quantised"
    },
    cogneeRead: {
      title: "Cognee Memory — Read",
      specs: ["Queries user session history", "Retrieves preference patterns", "Surfaces mood trends over time", "Feeds context to LLM for decisions"],
      tech: "Cognee SDK · SQLite backend"
    },
    ollama: {
      title: "Ollama LLM Engine",
      specs: ["Phi-3 Mini 3.8B (Q4 quantised)", "~2.3 GB model on SD card", "Interprets mood from transcript", "Selects phrases + soundscape + duration"],
      tech: "Ollama · llama.cpp · ARM NEON"
    },
    cogneeWrite: {
      title: "Cognee Memory — Write",
      specs: ["Stores current session parameters", "Updates user preference profile", "Logs mood + soundscape + duration", "Enables 'same as last time' recall"],
      tech: "Cognee SDK · append-only local log"
    },
    piper: {
      title: "Piper Text-to-Speech",
      specs: ["Neural TTS · VITS architecture", "~100 MB medium-quality voice model", "Real-time on Pi 5 · <50ms latency", "Sequences pre-selected phrases"],
      tech: "Piper · ONNX Runtime · en_GB-medium"
    },
    soundscape: {
      title: "Soundscape Engine",
      specs: ["Pre-loaded ambient audio files", "Ocean, rain, forest, bowls, white noise", "Crossfade mixing with voice output", "Volume auto-adjusts during speech"],
      tech: "SDL2 audio mixer · WAV playback"
    },
    speaker: {
      title: "Speaker Output",
      specs: ["I2S DAC (MAX98357A) + amplifier", "3W-5W mini speaker driver", "Mixed output: voice + ambient", "GPIO-connected to Pi 5"],
      tech: "I2S protocol · ALSA output sink"
    },
    cogneeDb: {
      title: "Local Memory Store",
      specs: ["SQLite database on SD card", "All user data encrypted at rest", "Never synced, never exported", "Structured session + preference data"],
      tech: "SQLite 3 · AES-256 encryption"
    },
    configFiles: {
      title: "Config File Storage",
      specs: ["NFC-received configuration files", "Voice pack audio (Piper-compatible)", "Soundscape WAV files", "Session parameter presets (JSON)"],
      tech: "/etc/stillness/configs/ · ext4 fs"
    },
  };

  const d = details[nodeKey] || { title: node.label, specs: [], tech: "" };
  const zoneColor = ZONE_COLORS[node.zone];

  return (
    <div style={{
      position: "fixed", top: 0, left: 0, right: 0, bottom: 0,
      background: "rgba(4,4,12,0.85)", backdropFilter: "blur(8px)",
      display: "flex", alignItems: "center", justifyContent: "center",
      zIndex: 1000, cursor: "pointer",
    }} onClick={onClose}>
      <div onClick={e => e.stopPropagation()} style={{
        background: "#0a0a16", border: `1px solid ${zoneColor.stroke}`,
        borderRadius: 12, padding: 28, maxWidth: 400, width: "90%",
        boxShadow: `0 0 60px ${zoneColor.accent}15`,
        cursor: "default",
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 16 }}>
          <span style={{
            fontSize: 20, width: 36, height: 36, borderRadius: 8,
            background: `${zoneColor.accent}15`, border: `1px solid ${zoneColor.stroke}`,
            display: "flex", alignItems: "center", justifyContent: "center",
            color: zoneColor.text,
          }}>{node.icon}</span>
          <div>
            <div style={{ fontSize: 16, fontWeight: 700, color: "#e8e8f4", letterSpacing: "-0.01em" }}>{d.title}</div>
            <div style={{ fontSize: 10, color: zoneColor.text, marginTop: 2, fontFamily: "'SF Mono', 'Fira Code', monospace" }}>{d.tech}</div>
          </div>
        </div>
        <div style={{ display: "flex", flexDirection: "column", gap: 6 }}>
          {d.specs.map((s, i) => (
            <div key={i} style={{
              display: "flex", alignItems: "flex-start", gap: 8,
              fontSize: 12, color: "#b0b0c8", lineHeight: 1.5,
            }}>
              <span style={{ color: zoneColor.accent, fontSize: 8, marginTop: 5 }}>●</span>
              {s}
            </div>
          ))}
        </div>
        <button onClick={onClose} style={{
          marginTop: 20, width: "100%", padding: "8px 0", borderRadius: 6,
          border: `1px solid ${zoneColor.stroke}`, background: "transparent",
          color: zoneColor.text, fontSize: 11, fontWeight: 600, cursor: "pointer",
          letterSpacing: "0.05em",
        }}>CLOSE</button>
      </div>
    </div>
  );
}

export default function ArchitectureDiagram() {
  const [selectedNode, setSelectedNode] = useState(null);
  const [activeFlow, setActiveFlow] = useState("all");
  const [tick, setTick] = useState(0);

  useEffect(() => {
    const interval = setInterval(() => setTick(t => t + 1), 100);
    return () => clearInterval(interval);
  }, []);

  const svgW = 920;
  const svgH = 560;

  const getNodeCenter = (key) => {
    const n = NODES[key];
    return { x: n.x + n.w / 2, y: n.y + n.h / 2 };
  };

  const getEdgePoint = (fromKey, toKey) => {
    const f = NODES[fromKey];
    const t = NODES[toKey];
    const fc = { x: f.x + f.w / 2, y: f.y + f.h / 2 };
    const tc = { x: t.x + t.w / 2, y: t.y + t.h / 2 };
    const dx = tc.x - fc.x;
    const dy = tc.y - fc.y;
    const angle = Math.atan2(dy, dx);

    let fx, fy, tx, ty;
    if (Math.abs(dx) * (f.h / 2) > Math.abs(dy) * (f.w / 2)) {
      fx = dx > 0 ? f.x + f.w : f.x;
      fy = fc.y + Math.tan(angle) * (dx > 0 ? f.w / 2 : -f.w / 2);
      tx = dx > 0 ? t.x : t.x + t.w;
      ty = tc.y - Math.tan(angle) * (dx > 0 ? t.w / 2 : -t.w / 2);
    } else {
      fy = dy > 0 ? f.y + f.h : f.y;
      fx = fc.x + (dy > 0 ? f.h / 2 : -f.h / 2) / Math.tan(angle);
      ty = dy > 0 ? t.y : t.y + t.h;
      tx = tc.x - (dy > 0 ? t.h / 2 : -t.h / 2) / Math.tan(angle);
    }
    return { x1: fx, y1: fy, x2: tx, y2: ty };
  };

  const flows = [
    { key: "all", label: "Full System" },
    { key: "config", label: "Config Flow" },
    { key: "voice", label: "Voice Pipeline" },
    { key: "memory", label: "Memory Loop" },
  ];

  const isConnectionVisible = (conn) => {
    if (activeFlow === "all") return true;
    if (activeFlow === "config") return ["configStore", "configBuilder", "appOffline", "nfc", "configFiles"].includes(conn.from) || ["configStore", "configBuilder", "appOffline", "nfc", "configFiles"].includes(conn.to);
    if (activeFlow === "voice") return ["mic", "whisper", "cogneeRead", "ollama", "cogneeWrite", "piper", "soundscape", "speaker"].includes(conn.from) && ["mic", "whisper", "cogneeRead", "ollama", "cogneeWrite", "piper", "soundscape", "speaker"].includes(conn.to);
    if (activeFlow === "memory") return ["cogneeRead", "cogneeWrite", "cogneeDb", "ollama", "configFiles"].includes(conn.from) || ["cogneeRead", "cogneeWrite", "cogneeDb"].includes(conn.to);
    return true;
  };

  return (
    <div style={{
      minHeight: "100vh", background: "#04040C",
      fontFamily: "'Inter', 'SF Pro Display', -apple-system, system-ui, sans-serif",
      color: "#e0e0f0",
    }}>
      <div style={{
        padding: "20px 28px 0",
        display: "flex", alignItems: "flex-end", justifyContent: "space-between",
        flexWrap: "wrap", gap: 12,
      }}>
        <div>
          <div style={{
            fontSize: 9, fontWeight: 700, letterSpacing: "0.2em", color: "#4ADE80",
            fontFamily: "'SF Mono', 'Fira Code', monospace", marginBottom: 6,
          }}>SYSTEM ARCHITECTURE · REV 1.0</div>
          <h1 style={{
            fontSize: 22, fontWeight: 700, margin: 0, letterSpacing: "-0.03em", color: "#f0f0f8",
          }}>
            stillness<span style={{ fontWeight: 300, color: "#4ADE80" }}>.device</span>
          </h1>
          <p style={{ fontSize: 11, color: "#5a5a7a", margin: "4px 0 0", maxWidth: 500, lineHeight: 1.5 }}>
            On-device AI meditation system · Raspberry Pi 5 · Air-gapped · Zero data exfiltration
          </p>
        </div>
        <div style={{ display: "flex", gap: 4 }}>
          {flows.map(f => (
            <button key={f.key} onClick={() => setActiveFlow(f.key)} style={{
              padding: "6px 14px", borderRadius: 6, border: "1px solid",
              borderColor: activeFlow === f.key ? "#4ADE80" : "#1a1a2e",
              background: activeFlow === f.key ? "#4ADE8012" : "transparent",
              color: activeFlow === f.key ? "#4ADE80" : "#5a5a7a",
              fontSize: 10, fontWeight: 600, cursor: "pointer",
              transition: "all 0.25s ease", letterSpacing: "0.02em",
            }}>{f.label}</button>
          ))}
        </div>
      </div>

      <div style={{ padding: "12px 16px", overflowX: "auto" }}>
        <svg viewBox={`0 0 ${svgW} ${svgH}`} style={{ width: "100%", maxHeight: "calc(100vh - 200px)" }}>
          <defs>
            <pattern id="grid" width="20" height="20" patternUnits="userSpaceOnUse">
              <path d="M 20 0 L 0 0 0 20" fill="none" stroke="#0f0f1f" strokeWidth="0.5" />
            </pattern>
            <filter id="glow">
              <feGaussianBlur stdDeviation="3" result="blur" />
              <feMerge><feMergeNode in="blur" /><feMergeNode in="SourceGraphic" /></feMerge>
            </filter>
            <marker id="arrowGreen" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">
              <path d="M0,0 L8,3 L0,6" fill="#4ADE80" opacity="0.6" />
            </marker>
            <marker id="arrowPurple" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">
              <path d="M0,0 L8,3 L0,6" fill="#C490D1" opacity="0.6" />
            </marker>
            <marker id="arrowBlue" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">
              <path d="M0,0 L8,3 L0,6" fill="#5CB8E4" opacity="0.6" />
            </marker>
            <marker id="arrowYellow" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">
              <path d="M0,0 L8,3 L0,6" fill="#BDB76B" opacity="0.6" />
            </marker>
          </defs>

          <rect width={svgW} height={svgH} fill="url(#grid)" />

          <rect x={30} y={50} width={220} height={340} rx={10}
            fill="#0F2E4A08" stroke="#1B6B9330" strokeWidth={1} strokeDasharray="4 4" />
          <text x={42} y={73} fontSize={8} fill="#5CB8E466" fontWeight={700}
            fontFamily="'SF Mono', monospace" letterSpacing="0.15em">FRONTEND APP</text>

          <line x1={310} y1={50} x2={310} y2={390} stroke="#ff4444" strokeWidth={1} strokeDasharray="6 3" opacity={0.4} />
          <text x={312} y={73} fontSize={7} fill="#ff444488" fontWeight={700}
            fontFamily="'SF Mono', monospace" letterSpacing="0.12em" transform="rotate(90, 312, 73)">
            SECURITY BOUNDARY
          </text>

          <rect x={330} y={170} width={160} height={100} rx={10}
            fill="#2A1A3E08" stroke="#7B3FA030" strokeWidth={1} strokeDasharray="4 4" />
          <text x={342} y={190} fontSize={7} fill="#C490D166" fontWeight={700}
            fontFamily="'SF Mono', monospace" letterSpacing="0.15em">NFC BRIDGE · ONE-WAY</text>

          <rect x={530} y={40} width={200} height={500} rx={10}
            fill="#0A2A1F08" stroke="#1A7A5030" strokeWidth={1} strokeDasharray="4 4" />
          <text x={542} y={63} fontSize={8} fill="#4ADE8066" fontWeight={700}
            fontFamily="'SF Mono', monospace" letterSpacing="0.15em">RASPBERRY PI 5</text>

          <rect x={710} y={118} width={190} height={200} rx={10}
            fill="#1A1A0A08" stroke="#6B6B2A30" strokeWidth={1} strokeDasharray="4 4" />
          <text x={722} y={138} fontSize={8} fill="#BDB76B66" fontWeight={700}
            fontFamily="'SF Mono', monospace" letterSpacing="0.15em">LOCAL STORAGE</text>

          <rect x={710} y={358} width={190} height={175} rx={10}
            fill="#0A2A1F08" stroke="#1A7A5030" strokeWidth={1} strokeDasharray="4 4" />
          <text x={722} y={378} fontSize={8} fill="#4ADE8066" fontWeight={700}
            fontFamily="'SF Mono', monospace" letterSpacing="0.15em">AUDIO OUTPUT</text>

          {CONNECTIONS.map((conn, i) => {
            const visible = isConnectionVisible(conn);
            const pts = getEdgePoint(conn.from, conn.to);
            const zoneColor = ZONE_COLORS[conn.zone];
            const markerMap = { app: "arrowBlue", bridge: "arrowPurple", device: "arrowGreen", storage: "arrowYellow" };
            const midX = (pts.x1 + pts.x2) / 2;
            const midY = (pts.y1 + pts.y2) / 2;

            return (
              <g key={i} opacity={visible ? 1 : 0.08} style={{ transition: "opacity 0.5s ease" }}>
                <line
                  x1={pts.x1} y1={pts.y1} x2={pts.x2} y2={pts.y2}
                  stroke={zoneColor.accent}
                  strokeWidth={conn.isOneWay ? 1.5 : 1}
                  strokeDasharray={conn.dashed ? "3 3" : "none"}
                  opacity={0.35}
                  markerEnd={`url(#${markerMap[conn.zone]})`}
                />
                {visible && (
                  <AnimatedParticle
                    x1={pts.x1} y1={pts.y1} x2={pts.x2} y2={pts.y2}
                    color={zoneColor.accent}
                    delay={i * 400}
                    duration={2000 + i * 200}
                  />
                )}
                <text x={midX} y={midY - 5} fontSize={6.5} fill={zoneColor.text} textAnchor="middle"
                  fontFamily="'SF Mono', monospace" opacity={0.5}>{conn.label}</text>
              </g>
            );
          })}

          {Object.entries(NODES).map(([key, node]) => {
            const zoneColor = ZONE_COLORS[node.zone];
            const isVisible = activeFlow === "all" ||
              (activeFlow === "config" && ["configStore", "configBuilder", "appOffline", "nfc", "configFiles"].includes(key)) ||
              (activeFlow === "voice" && ["mic", "whisper", "cogneeRead", "ollama", "cogneeWrite", "piper", "soundscape", "speaker"].includes(key)) ||
              (activeFlow === "memory" && ["cogneeRead", "cogneeWrite", "cogneeDb", "ollama", "configFiles"].includes(key));

            return (
              <g key={key}
                opacity={isVisible ? 1 : 0.12}
                style={{ cursor: "pointer", transition: "opacity 0.5s ease" }}
                onClick={() => setSelectedNode(key)}
              >
                <rect
                  x={node.x} y={node.y} width={node.w} height={node.h}
                  rx={8} fill={zoneColor.fill} stroke={zoneColor.stroke}
                  strokeWidth={1}
                />
                <rect
                  x={node.x} y={node.y} width={node.w} height={node.h}
                  rx={8} fill="transparent" stroke={zoneColor.accent}
                  strokeWidth={0} opacity={0}
                >
                  <animate attributeName="stroke-width" values="0;2;0" dur="3s" repeatCount="indefinite" />
                  <animate attributeName="opacity" values="0;0.3;0" dur="3s" repeatCount="indefinite" />
                </rect>

                <text x={node.x + 12} y={node.y + 19} fontSize={9} fill={zoneColor.text}
                  fontWeight={700} fontFamily="'SF Mono', 'Fira Code', monospace" letterSpacing="0.02em">
                  <tspan fill={zoneColor.accent} fontSize={11} dx={-2}>{node.icon} </tspan>
                  {node.label}
                </text>
                <text x={node.x + 12} y={node.y + 34} fontSize={7} fill={zoneColor.text}
                  opacity={0.5} fontFamily="'SF Mono', monospace">
                  {node.sub}
                </text>

                <rect x={node.x} y={node.y} width={node.w} height={node.h}
                  rx={8} fill="transparent" />
              </g>
            );
          })}

          <text x={440} y={260} fontSize={18} fill="#C490D1" textAnchor="middle" opacity={0.6}>→</text>
          <text x={440} y={275} fontSize={7} fill="#C490D1" textAnchor="middle" opacity={0.4}
            fontFamily="'SF Mono', monospace">CONFIG ONLY</text>

          <g opacity={0.5}>
            <text x={330} y={340} fontSize={7} fill="#ff4444" textAnchor="middle"
              fontFamily="'SF Mono', monospace" fontWeight={700} letterSpacing="0.1em">
              ✕ NO DATA OUT
            </text>
            <line x1={303} y1={330} x2={357} y2={330} stroke="#ff4444" strokeWidth={1.5} opacity={0.4} />
          </g>

          <text x={svgW / 2} y={svgH - 12} fontSize={7} fill="#2a2a4a" textAnchor="middle"
            fontFamily="'SF Mono', monospace" letterSpacing="0.1em">
            RPi 5 · 8GB · Bookworm 64-bit · Ollama 0.6 · Whisper tiny.en · Piper en_GB-medium · Cognee 0.2 · SQLite 3
          </text>
        </svg>
      </div>

      <div style={{
        padding: "8px 28px 20px",
        display: "flex", gap: 20, flexWrap: "wrap", alignItems: "center",
      }}>
        <span style={{ fontSize: 9, color: "#3a3a5a", fontFamily: "'SF Mono', monospace", letterSpacing: "0.1em" }}>
          ZONES:
        </span>
        {Object.entries(ZONE_COLORS).map(([key, val]) => (
          <div key={key} style={{ display: "flex", alignItems: "center", gap: 6 }}>
            <div style={{ width: 10, height: 10, borderRadius: 3, background: val.fill, border: `1px solid ${val.stroke}` }} />
            <span style={{ fontSize: 9, color: val.text, fontFamily: "'SF Mono', monospace", textTransform: "uppercase", letterSpacing: "0.1em" }}>
              {key === "app" ? "Frontend App" : key === "bridge" ? "NFC Bridge" : key === "device" ? "Raspberry Pi" : "Local Storage"}
            </span>
          </div>
        ))}
        <div style={{ flex: 1 }} />
        <span style={{ fontSize: 9, color: "#3a3a5a", fontFamily: "'SF Mono', monospace" }}>
          Click any node for specs · Tap flow filters above to isolate paths
        </span>
      </div>

      {selectedNode && (
        <NodeDetail
          node={NODES[selectedNode]}
          nodeKey={selectedNode}
          onClose={() => setSelectedNode(null)}
        />
      )}
    </div>
  );
}
```

</details>

---

## How It Works

The user speaks to the device. That's the entire interface. No screen, no buttons, no app required for operation.

**"I'm feeling really anxious today, my mind won't stop racing."**

The device captures audio via an I2S MEMS microphone, transcribes it locally using Whisper (tiny.en), queries Cognee for any stored user history and preferences, then passes the transcript plus memory context to a local Phi-3 Mini LLM running on Ollama. The LLM interprets the user's emotional state, selects an appropriate soundscape (e.g. rainfall for grounding), chooses a sequence of guided meditation phrases from the on-device phrase library, and determines session duration. Cognee writes the session parameters back to local storage. Piper TTS converts the phrase sequence into spoken audio, which plays through the speaker mixed with the ambient soundscape.

Next session, the user says **"Same as last time but longer."** Cognee retrieves the previous session's parameters — mood, soundscape, phrases, duration — and the LLM extends the duration while keeping everything else consistent. The device learns. The data stays.

---

## Tech Stack

| Layer | Component | Model / Tech | Size |
|-------|-----------|-------------|------|
| Speech-to-Text | Whisper | tiny.en via whisper.cpp (GGML) | ~75 MB |
| Memory Engine | Cognee | SQLite backend, AES-256 at rest | Minimal |
| LLM | Ollama | Phi-3 Mini 3.8B (Q4 quantised) | ~2.3 GB |
| Text-to-Speech | Piper | en_GB medium (ONNX Runtime) | ~100 MB |
| Soundscapes | SDL2 mixer | Pre-loaded WAV files | ~200 MB |
| Phrase Library | Pre-generated | ElevenLabs (build-time) + Piper (runtime) | ~100 MB |

**Hardware:** Raspberry Pi 5 (8GB), I2S MEMS microphone (INMP441), I2S DAC + amplifier (MAX98357A), 3–5W speaker, USB-C power.

**Total on-device footprint:** ~3 GB on SD card.

---

## Data Security Model

The device operates with no active network interface. There is no Wi-Fi, no Bluetooth, no cellular radio enabled during operation. The only data ingress point is NFC, which accepts configuration payloads (2–50 KB JSON) from the companion app. This transfer is strictly one-way — no data is ever read from the device via NFC or any other channel.

User voice recordings are transcribed in-memory and discarded. Only the text transcript is processed. Session metadata (mood detected, soundscape selected, duration, timestamp) is stored locally in an encrypted SQLite database managed by Cognee. This data is never exported, synced, or made accessible to any external system.

The companion web app has no telemetry, no analytics, no user accounts, and no tracking. It is a configuration tool only.

---

## Hackathon Context

**Event:** Localhost: On-Device Agent Hackathon
**Organiser:** AI Engine (Zoe Qin, Tyler Edwards)
**Venue:** Dawn Capital, Level 7, Ilona Rose House, London W1D 4AL
**Date:** Saturday, 20 June 2026
**Constraint:** All AI processing must run entirely offline on physical hardware with zero internet connectivity.

**Track partners used:**

**Cognee** — Open-source AI memory engine providing persistent, queryable memory across meditation sessions without cloud infrastructure. Enables the device to adapt to user preferences over time and recall previous session parameters on request.

**ElevenLabs** — Used at build time to pre-generate a library of high-quality meditation voice phrases stored as WAV files on the device. Not called at runtime. No ElevenLabs API interaction occurs during device operation.

---

## References

Kim, T., Bae, S., Kim, H.A., Lee, S., Hong, H., Yang, C., & Kim, Y.H. (2024). *MindfulDiary: Harnessing Large Language Model to Support Psychiatric Patients' Journaling.* CHI 2024. ACM. doi:10.1145/3613904.3642937

*Towards Privacy-aware Mental Health AI Models.* (2025). Nature Computational Science. doi:10.1038/s43588-025-00875-w

*AI-powered mental health application with data privacy preservation.* (2025). ScienceDirect. doi:10.1016/S2215-0161(25)00600-4

*Evaluating Voice-Enabled Generative AI for Mental Health: Real-Time Performance and Safety Analyses.* (2025). medRxiv. doi:10.1101/2025.11.14.25340246

Yvanoff-Frenchin, C. et al. (2020). *Edge Computing Robot Interface for Automatic Elderly Mental Health Care Based on Voice.* MDPI Electronics 9(3), 419.

---

## License

[MIT](./LICENSE)
