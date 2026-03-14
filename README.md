# AudioLoom

<img src="Resources/Icon128.png" width="256" height="256" alt="AudioLoom" />

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-support-yellow?logo=buymeacoffee)](https://buymeacoffee.com/notpunny)

Unreal Engine plugin for routing audio to specific output devices and channels, with OSC support for external control and monitoring. Use for multi-speaker setups, spatial audio routing, and show control integration. [GitHub](https://github.com/dhhuston/AudioLoom)

---

## Table of Contents

- [Installation](#installation)
- [Requirements](#requirements)
- [Features](#features)
- [Quick Start](#quick-start)
- [Setup & Configuration](#setup--configuration)
- [Management Window](#management-window)
- [OSC Triggers & Monitoring](#osc-triggers--monitoring)
- [Blueprint API](#blueprint-api)
- [Project Settings](#project-settings)
- [Walkthrough Guides](#walkthrough-guides)
- [License](#license)

---

## Installation

### As a project plugin (recommended)

1. Copy the `AudioLoom` folder into your project’s `Plugins/` directory:
   ```
   YourProject/
   └── Plugins/
       └── AudioLoom/
   ```

2. Restart Unreal Editor (or recompile if Live Coding is enabled).

3. Enable the plugin if needed:
   - **Edit → Plugins**
   - Search for “AudioLoom”
   - Ensure it is enabled

### Dependencies

AudioLoom requires the **OSC** plugin (included with Unreal Engine). If it is disabled:

- **Edit → Plugins** → search for “OSC” → enable it
- Restart the editor if prompted

---






---

## Requirements

| Requirement | Details |
|-------------|---------|
| **Engine** | Unreal Engine 5.x (tested on 5.4, 5.7) |
| **Platforms** | Windows (WASAPI), macOS (CoreAudio) |
| **Plugins** | OSC (built-in) |

---

## Features

- **Device routing** — Send audio to any system output device (speakers, interfaces, virtual devices)
- **Per-channel routing** — Route to specific output channels (e.g. channel 3 only)
- **Low latency mode** — Bypass system mixer for lower latency (Windows)
- **OSC control** — Play, stop, and loop via OSC messages
- **OSC monitoring** — Emit play/stop state to external software
- **Manager panel** — Central UI for all components, devices, and OSC
- **Blueprint support** — Full control from Blueprints
- **WAV / SoundWave** — Uses standard `USoundWave` assets (WAV import)

---

## Quick Start

1. Add an **Audio Loom WASAPI** actor to your level:
   - **Place Actors** → search “Audio Loom” → drag into the level

2. Assign a sound:
   - Select the actor → Details → **Sound Wave**

3. Choose output:
   - **Device**: pick a device or use “Default Output”
   - **Output Channel**: 0 = all channels, or 1, 2, 3… for one channel

4. Open the management window:
   - **Window → Audio Loom**

5. Use the manager to play/stop and configure OSC, or call `Play` / `Stop` from Blueprints.

---

## Setup & Configuration

### Component vs Actor

- **Audio Loom WASAPI** actor — Pre-made actor with a `UAudioLoomWasapiComponent`. Good for quick setup.
- **AudioLoomWasapiComponent** — Can be added to any actor via **Add Component** → search “Audio Loom”.

Both share the same features.

### Routing

| Property | Description |
|----------|-------------|
| **Sound Wave** | The `USoundWave` to play |
| **Device Id** | Empty = default output; otherwise the ID of the target device |
| **Output Channel** | 0 = all channels; 1–N = route only to that channel |
| **Low Latency Mode** | Bypass system mixer (Windows). Falls back to shared if unsupported |
| **Buffer Size (ms)** | Buffer duration for low latency mode. 0 = driver default. Typical 3–20 ms |

### Playback

| Property | Description |
|----------|-------------|
| **Play on Begin Play** | Auto-play when the level starts |
| **Loop** | Loop the sound continuously |

### OSC Address

- Leave **OSC Address** blank to use the default: `/audioloom/OwnerName/Index`
- Or set a custom address (e.g. `/myshow/speaker1`) — must start with `/` and follow OSC 1.0 path rules

---

## Management Window

**Window → Audio Loom**

Central panel for:

- All `AudioLoomWasapiComponent` instances in the current level
- **Refresh** — Update the list
- **Sound** — Assign sounds directly in the table (click, drag-drop, or “Use Selected” from the Content Browser)
- **Device** — Dropdown of output devices
- **Channel** — Output channel per component
- **Low Latency** — Toggle low latency mode (Windows)
- **Buffer (ms)** — Buffer size for low latency mode
- **OSC Address** — Shows validity (✓/✗) and lets you edit inline
- **Loop** / **Begin** — Toggles for each component
- **Play** / **Stop** — Buttons per row
- **Select** — Select the actor in the viewport

### OSC Section (top of panel)

- **Listen Port** — OSC receive port (default 9000)
- **Send IP** / **Send Port** — Where monitoring messages are sent
- **Check Port** — Test if the listen port is free
- **Start / Stop OSC** — Toggle the OSC server
- **OSC Commands Reference** — Expandable help for address format and commands

---

## OSC Triggers & Monitoring

### Overview

- AudioLoom runs an **OSC server** that receives play/stop/loop messages.
- It runs an **OSC client** that sends play/stop state for monitoring.

### Address Format

Each component has a **base** address (e.g. `/audioloom/MyActor/0`). Commands use suffixes:

| Address | Action | Arguments |
|---------|--------|-----------|
| `/base/play` | Start playback | Float ≥0.5, int ≥1, or none |
| `/base/stop` | Stop playback | Any message triggers stop |
| `/base/loop` | Set loop on/off | 1 = loop, 0 = no loop |

### Monitoring (outgoing)

When a component starts or stops playing, AudioLoom sends:

| Address | Value | Meaning |
|---------|-------|---------|
| `/base/state` | 1.0 | Playing |
| `/base/state` | 0.0 | Stopped |

### Example (base = `/audioloom/speaker1`)

- Play: `/audioloom/speaker1/play` (with or without args)
- Stop: `/audioloom/speaker1/stop`
- Loop on: `/audioloom/speaker1/loop` with float 1
- Loop off: `/audioloom/speaker1/loop` with float 0
- State: `/audioloom/speaker1/state` → 1 or 0

---

## Blueprint API

### Playback

| Node | Description |
|------|-------------|
| `Play` | Start playback |
| `Stop` | Stop playback |
| `IsPlaying` | Returns true if currently playing |
| `Set Loop` | Turn loop on/off |
| `Get Loop` | Current loop state |

### Routing

| Node | Description |
|------|-------------|
| `Get Device Id` | Current device ID |
| `Set Device Id` | Set device (empty = default) |
| `Get Output Channel` | Current channel (0 = all) |
| `Set Output Channel` | Set output channel |

### OSC

| Node | Description |
|------|-------------|
| `Get OSC Address` | Resolved base address |
| `Set OSC Address` | Set custom address (validated) |
| `Is OSC Address Valid` | Blueprint library — validate address |
| `Normalize OSC Address` | Blueprint library — normalize format |

### OSC Subsystem (from Get World Subsystem)

| Node | Description |
|------|-------------|
| `Start Listening` | Start OSC server |
| `Stop Listening` | Stop OSC server |
| `Is Listening` | Whether server is running |
| `Is Port Available` | Static — check if port is free |
| `Update Monitoring Target` | Refresh Send IP/port |
| `Rebuild Component Registry` | Refresh address → component map |

---

## Project Settings

**Edit → Project Settings → Audio Loom > OSC**

| Setting | Default | Description |
|---------|---------|-------------|
| **Listen Port** | 9000 | UDP port for OSC receive |
| **Send IP** | 127.0.0.1 | IP for monitoring output |
| **Send Port** | 9001 | Port for monitoring output |

---

## Walkthrough Guides

### A. Basic routing to a speaker

1. Place an **Audio Loom WASAPI** actor.
2. Assign a `USoundWave` in Details or the manager.
3. Set **Device** to the target output in the manager.
4. Set **Output Channel** to 0 (all) or a specific channel.
5. Click **Play** in the manager or call `Play` from Blueprint.

### B. OSC control from external software

1. Open **Window → Audio Loom**.
2. Set **Listen Port** (e.g. 9000) and click **Check Port**.
3. Click **Start OSC**.
4. From your external app, send to your machine’s IP and the listen port:
   - `/audioloom/ActorName/0/play` — play
   - `/audioloom/ActorName/0/stop` — stop

### C. Multi-channel speaker map

1. Add one Audio Loom actor per speaker (or one actor with multiple components).
2. Give each a unique **OSC Address** (e.g. `/show/L`, `/show/R`, `/show/C`).
3. Route each component to the correct device and channel.
4. Use OSC to trigger each speaker independently.

### D. Monitoring in another app

1. In **Project Settings → Audio Loom > OSC**, set **Send IP** and **Send Port**.
2. In your external app (e.g. Max/MSP, Pure Data), open a UDP receiver on that port.
3. Start OSC in the Audio Loom panel.
4. When components play or stop, you’ll receive `/base/state` with 1 or 0.

### E. Blueprint-triggered playback

1. Add `AudioLoomWasapiComponent` to an actor.
2. Configure sound, device, and channel.
3. In Blueprint, call **Play** (e.g. from Event BeginPlay or a custom event).
4. Use **Stop** and **Set Loop** as needed.

---

## Troubleshooting

- **No sound** — Check device selection, volume, and that the SoundWave is valid.
- **Port in use** — Use **Check Port** in the manager or choose another port in Project Settings.
- **OSC not working** — Ensure the OSC server is started (green “Stop OSC” state) and addresses match.
- **Wrong device on Mac** — AudioLoom uses CoreAudio; verify devices in macOS Audio MIDI Setup.
- **Low latency mode fails** — Not all devices support it; the plugin falls back to shared automatically.

---

## License

AudioLoom is released under the [MIT License](LICENSE).
