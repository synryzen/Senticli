# Senticli

Senticli is a local-first Linux desktop AI companion with a two-pane interface:

- Top pane: animated face (idle, listening, thinking, speaking, warning)
- Bottom pane: terminal-style interaction area with command history, slash commands, and approval prompts

This repository currently contains a working prototype scaffold in C++ + Qt 6 + QML.

## Current Prototype Features

- Two-pane layout matching the product concept
- Animated digital face with status-driven expressions
- Loona-inspired companion expression set (happy/sad/angry/confused/sleeping + look-around)
- Selectable face styles (`Loona`, `Terminal`, `Orb`)
- Optional camera snapshot analysis (captures a frame with `ffmpeg` and sends it to your vision-capable model)
- Terminal-style message stream with user/assistant/system bubbles
- Terminal-native bottom pane with line-based output and CLI prompt styling
- Terminal niceties: Up/Down command history and ANSI-colored shell output
- Mode strip (`Chat`, `Assist`, `Command`)
- OpenAI-compatible model endpoint support (example: `192.168.1.220/v1/chat/completions`)
- Cloudflare/custom endpoint friendly auth via optional bearer API key
- Provider presets for `Custom`, `LM Studio`, and `Ollama`
- Connection health check (`/v1/models` test)
- Model discovery from `/v1/models` and selectable model list in UI
- Streaming token output for remote model responses (live incremental text)
- Typing cursor during stream and cancel button for in-flight responses
- Adaptive stream smoothing to reduce bursty chunk jumps
- `Instant` stream mode for lowest-latency output on powerful hardware
- Sentence-level streaming TTS with barge-in interrupt for smoother conversation flow
- Duplex voice beta (continuous mic chunks + STT endpoint + wake phrase routing)
- Echo suppression in duplex mode to avoid self-transcription while speaking
- Tunable duplex controls: VAD sensitivity + smoothness preset (Responsive/Balanced/Natural/Studio)
- One-click duplex quick-tune presets in AI Settings (Fast/Balanced/Human/Studio)
- Voice-utterance buffering combines partial STT fragments before dispatch for smoother turn-taking
- Live listening meter on the face panel while mic is active
- Word-cadence mouth timing (less pre-audio flapping, smoother speech sync)
- Adjustable lip-sync delay control in AI Settings (fine-tune mouth/audio alignment)
- Dedicated AI Settings panel with:
  - saved connection profiles (save/load/delete presets)
  - provider + endpoint controls
  - optional models endpoint override
  - optional API key field
  - model picker + refresh
  - manual model ID entry (type and set any model name)
  - speed mode + token-rate controls
  - companion name setting
  - wake phrase enable/disable
  - editable wake responses
  - conversational mode toggle
  - duplex voice toggle
  - duplex smoothness + VAD sensitivity controls
  - STT endpoint + STT model controls
  - personality preset selector
  - voice gender + voice style selectors
  - voice engine selector (`Auto`, `Speech Dispatcher`, `eSpeak`, `Piper`)
  - optional Piper model path
  - voice and memory toggles
  - granted folder permissions manager
  - audit log viewer/clear
- First-run setup wizard
- Persistent settings via `QSettings`
- Real shell execution with timeout/cancel and action audit
- Project memory notes (`/remember`, `/recall`, `/forget`)
- Slash command parser prototype:
  - `/help`
  - `/provider <Custom|LM Studio|Ollama>`
  - `/profiles`
  - `/profile <name>`
  - `/profile-save <name>`
  - `/profile-delete <name>`
  - `/endpoint <url>`
  - `/models-endpoint <url|auto>`
  - `/camera <on|off|snap>`
  - `/models`
  - `/test`
  - `/cancel`
  - `/model <id>`
  - `/speed <Instant|Terminal|Balanced|Human|Cinematic>`
  - `/lip-sync <0-1100>`
  - `/name <assistant-name>`
  - `/wake <on|off>`
  - `/conversation <on|off>`
  - `/duplex <on|off>`
  - `/duplex-smooth <Responsive|Balanced|Natural|Studio>`
  - `/duplex-preset <fast|balanced|human|studio>`
  - `/vad <1-100>`
  - `/stt-endpoint <url|auto>`
  - `/stt-model <id>`
  - `/personality <Helpful|Professional|Witty|Teacher|Hacker|Calm>`
  - `/face-style <Loona|Terminal|Orb>`
  - `/gender <Neutral|Male|Female>`
  - `/voice-style <Default|Soft|Bright|Narrator>`
  - `/voice-engine <Auto|Speech Dispatcher|eSpeak|Piper>`
  - `/piper-model <path|clear>`
  - `/voices`
  - `/grant <folder>`
  - `/revoke <folder>`
  - `/remember <note>`
  - `/recall`
  - `/forget`
  - `/voice on|off|stop`
  - `/open <app>` (approval-gated)
  - `/find <query>`
  - `/run <command>` (risk detection + approval for destructive patterns)
- Explicit approval card for gated actions
- Permission-first behavior for risky actions

## Project Structure

```text
Senticli/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── AppController.cpp
│   │   └── include/
│   │       └── AppController.h
│   └── models/
│       ├── MessageModel.cpp
│       └── MessageModel.h
└── qml/
    ├── Main.qml
    ├── components/
    │   ├── CommandBubble.qml
    │   ├── Eye.qml
    │   └── Mouth.qml
    ├── theme/
    │   ├── Colors.qml
    │   └── Typography.qml
    └── views/
        ├── FaceView.qml
        ├── InputBar.qml
        ├── StatusStrip.qml
        └── TerminalView.qml
```

## Dependencies (Ubuntu/Debian)

Install Qt 6 development packages:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  speech-dispatcher alsa-utils
```

For camera snapshot analysis, install `ffmpeg`.

For realistic local voices with Piper, install `piper` (and keep `aplay` from `alsa-utils`).

If your distro package names differ, install equivalent Qt 6 Quick + QML + Quick Controls dev/runtime modules.

## Build and Run

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/senticli
```

## Remote Model Quick Start

1. Click `AI Settings`
2. Set endpoint to something like `192.168.1.220/v1/chat/completions`
3. If needed, set API key in AI Settings
4. If your provider exposes models on a custom path, set `Models Endpoint` override
5. Click `Refresh`, pick a model, then click `Use`
6. If your model does not appear, type the exact model ID and click `Set Typed Model`
7. Pick speed profile (`Instant`, `Terminal`, `Balanced`, `Human`, or `Cinematic`)
8. Send a normal message (non-slash command) to route it to that model
9. Save the setup as a profile so you can switch quickly later

You can do the same via commands:

```bash
/endpoint 192.168.1.220/v1/chat/completions
/models-endpoint https://lm.msidragon.com/v1/models
/apikey your-token-here
/camera on
/camera snap
/models
/model your-model-id
/speed instant
/lip-sync 420
/name Nova
/wake on
/conversation on
/duplex on
/duplex-preset human
/duplex-smooth natural
/vad 65
/stt-endpoint https://lm.msidragon.com/v1/audio/transcriptions
/stt-model whisper-1
/personality teacher
/gender female
/voice-style narrator
/voice-engine Piper
/piper-model /home/you/models/en_US-lessac-medium.onnx
/profile-save my-cloudflare-route
```

If your STT endpoint is blank, Senticli auto-derives it from your chat completion URL.

## Packaging

```bash
cmake -S . -B build -G Ninja
cmake --build build
cd build
cpack
```

Artifacts include `.tar.gz` and `.deb` packages.

## Next Milestones

1. Add selectable realistic TTS voice packs (Piper/voice models)
2. Add richer folder search (recursive + filters + previews)
3. Add theme packs and companion face packs
4. Add plugin/tool system for custom actions
5. Add installer scripts and auto-update channel
