# Senticli

Senticli is a local-first Linux desktop AI companion with a two-pane interface:

- Top pane: animated face (idle, listening, thinking, speaking, warning)
- Bottom pane: terminal-style interaction area with command history, slash commands, and approval prompts

This repository currently contains a working prototype scaffold in C++ + Qt 6 + QML.

## Current Prototype Features

- Two-pane layout matching the product concept
- Animated digital face with status-driven expressions
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
  - personality preset selector
  - voice gender + voice style selectors
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
  - `/models`
  - `/test`
  - `/cancel`
  - `/model <id>`
  - `/speed <Instant|Terminal|Balanced|Human|Cinematic>`
  - `/name <assistant-name>`
  - `/wake <on|off>`
  - `/conversation <on|off>`
  - `/personality <Helpful|Professional|Witty|Teacher|Hacker|Calm>`
  - `/gender <Neutral|Male|Female>`
  - `/voice-style <Default|Soft|Bright|Narrator>`
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
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts
```

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
/models
/model your-model-id
/speed instant
/name Nova
/wake on
/conversation on
/personality teacher
/gender female
/voice-style narrator
/profile-save my-cloudflare-route
```

## Packaging

```bash
cmake -S . -B build -G Ninja
cmake --build build
cd build
cpack
```

Artifacts include `.tar.gz` and `.deb` packages.

## Next Milestones

1. Add speech-to-text backend (push-to-talk)
2. Add richer folder search (recursive + filters + previews)
3. Add theme packs and companion face packs
4. Add plugin/tool system for custom actions
5. Add installer scripts and auto-update channel
