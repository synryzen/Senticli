# Senticli

Senticli is a local-first Linux desktop AI companion with a two-pane interface:

- Top pane: animated face (idle, listening, thinking, speaking, warning)
- Bottom pane: terminal-style interaction area with command history, slash commands, and approval prompts

This repository currently contains a working prototype scaffold in C++ + Qt 6 + QML.

## Current Prototype Features

- Two-pane layout matching the product concept
- Animated digital face with status-driven expressions
- Terminal-style message stream with user/assistant/system bubbles
- Mode strip (`Chat`, `Assist`, `Command`)
- OpenAI-compatible model endpoint support (example: `192.168.1.220/v1/chat/completions`)
- Provider presets for `Custom`, `LM Studio`, and `Ollama`
- Connection health check (`/v1/models` test)
- Model discovery from `/v1/models` and selectable model list in UI
- Streaming token output for remote model responses (live incremental text)
- Typing cursor during stream and cancel button for in-flight responses
- Adaptive stream smoothing to reduce bursty chunk jumps
- Dedicated AI Settings panel with:
  - provider + endpoint controls
  - model picker + refresh
  - smoothing + token-rate controls
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
  - `/endpoint <url>`
  - `/models`
  - `/test`
  - `/cancel`
  - `/model <id>`
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
3. Click `Refresh`, pick a model, then click `Use`
4. Pick smoothing profile (`Cinematic`, `Human`, `Balanced`, or `Terminal`)
5. Send a normal message (non-slash command) to route it to that model

You can do the same via commands:

```bash
/endpoint 192.168.1.220/v1/chat/completions
/models
/model your-model-id
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
