<p align="center">
  <img src="Resources/EditorStyles/Icon128.png" width="256" height="256" alt="AudioLoom" />
</p>

# AudioLoom

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-support-yellow?logo=buymeacoffee)](https://buymeacoffee.com/notpunny)

Unreal Engine plugin for routing audio to specific output devices and channels, with OSC support for external control and monitoring. Use for multi-speaker setups, spatial audio routing, and show control integration. [GitHub](https://github.com/NPLAudio/AudioLoom)

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
- [For developers (code map)](#for-developers-code-map)
- [License](#license)

---

## Installation

### From a release (recommended)

1. Download the latest `AudioLoom-vX.X.zip` from [Releases](https://github.com/NPLAudio/AudioLoom/releases).
2. Extract the zip — it contains a single `AudioLoom` folder.
3. Copy the `AudioLoom` folder into your project’s `Plugins/` directory:
   ```
   YourProject/
   └── Plugins/
       └── AudioLoom/
   ```

4. Restart Unreal Editor (or recompile if Live Coding is enabled).
5. Enable the plugin if needed:
   - **Edit → Plugins**
   - Search for “AudioLoom”
   - Ensure it is enabled

### From source (git clone)

For contributors or if you need the latest unreleased code:

1. Clone the repo, or copy the `AudioLoom` folder from a clone, into your project's `Plugins/` directory.
2. Restart Unreal Editor (or recompile if Live Coding is enabled).
3. Enable the plugin if needed via **Edit → Plugins**.

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
| **Platforms** | Windows, macOS |
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

1. Add an **Audio Loom** actor to your level:
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

- **Audio Loom** actor — Pre-made actor with an `AudioLoomComponent`. Good for quick setup.
- **AudioLoomComponent** — Can be added to any actor via **Add Component** → search “Audio Loom”.

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

- Leave **OSC Address** blank to use the default: `/audioloom/<instanceSegment>/<n>` — `<n>` is **1-based** per owner (`1`, `2`, `3` for the first, second, third Audio Loom on that actor). The middle segment uses the **hierarchy root** actor’s **instance label** (**Actor Label** in the Outliner), sanitized for OSC (spaces → `_`, etc.). If the label is empty, the internal actor name is used instead. The root is found by walking **attach parent**, then **parent actor**, so nested actors (e.g. a status mesh) still resolve to the top-level label. **Two different hierarchy roots with the same Actor Label** can produce colliding default paths — the **Audio Loom** panel shows a warning; use **unique** labels or set a custom **OSC Address** per component.
- Or set a custom address (e.g. `/myshow/speaker1`) — must start with `/` and follow OSC 1.0 path rules

---

## Management Window

**Window → Audio Loom**

Central panel for:

- All `AudioLoomComponent` instances in the current level
- **Refresh** — Update the list
- **Export CSV** / **Import CSV** — Snapshot or apply **device**, **output channel**, **sound asset path**, and optional **OSC address override** for all Audio Loom components in the **current world**; exports also list **resolved OSC paths** for planning triggers (see [CSV routing](#csv-routing-importexport) below)
- **Sound** — Assign sounds directly in the table (click, drag-drop, or “Use Selected” from the Content Browser)
- **Device** — Dropdown of output devices
- **Channel** — Output channel per component
- **Low Latency** — Toggle low latency mode (Windows)
- **Buffer (ms)** — Buffer size for low latency mode
- **OSC Address** — Shows validity (✓/✗) and lets you edit inline
- **Loop** / **Begin** — Toggles for each component
- **Play** / **Stop** — Buttons per row
- **Select** — Select the actor in the viewport
- **Out: _ms_** (row header) — Estimated **OS output buffer / stream latency** while playing (Windows: `IAudioClient::GetStreamLatency` when available, else buffer duration; macOS: device buffer frame size ÷ nominal sample rate). This is **not** full round-trip input→output latency; decoding/resample cost is usually tiny compared to the device buffer.
- **Editor:** (expanded row body) — Live **Slate/editor FPS** and average frame time. This is **not** the DAC buffer number in the header; it reflects how busy the editor/PIE UI thread is. Very low FPS can make these labels update less smoothly; it does not change the OS-reported audio buffer latency while a stream is running.

**Performance tips (latency vs CPU):**

| Topic | Tip |
|--------|-----|
| **Windows low latency** | Enable **Low Latency Mode** (exclusive) and set **Buffer (ms)** to a stable value (often **3–10 ms**); unsupported devices fall back to shared mode with a larger typical buffer. |
| **Shared mode** | Larger buffers (~tens of ms) — higher latency, more headroom. |
| **Resampling** | AudioLoom resamples non-48 kHz sources to 48 kHz; **export WAV at 48 kHz** when possible. |
| **Editor vs packaged** | The editor can be CPU-heavy; **packaged builds** are fairer for overall performance testing, but **output buffer latency** is still dominated by OS/device settings, not Unreal FPS. |
| **Devices** | Prefer a **dedicated interface** with stable drivers; virtual cables often add buffering. |

### CSV routing import/export

Use **Export CSV** to write a UTF-8 file (comma-separated, quoted fields when needed). Use **Import CSV** to apply routing from a file produced by this plugin or edited by hand.

**Columns (header row, fixed order):**

| Column | Description |
|--------|-------------|
| `ActorName` | Owner actor **name** (`AActor::GetName()`), e.g. `MyActor_C_0` |
| `ActorLabel` | **Viewport label** (`AActor::GetActorLabel()`). If empty in the CSV, only `ActorName` is used for matching. If non-empty, both must match. |
| `ComponentName` | **Unreal object name** of the component (`UAudioLoomComponent::GetName()`), e.g. `AudioLoomComponent_0` — unique per component on that actor. Use this to tell rows apart. **Import** matches by `ComponentName` when present. |
| `ComponentIndex` | **Per-actor** index after sorting by component name (`0` = first in that sorted list). **Each actor’s first Audio Loom is `0`**, so **multiple rows in the file can all show `0` when they are different actors** (one component each). With the same actor and multiple Audio Loom components, indices are `0`, `1`, `2`, … |
| `SoundWave` | Asset path (e.g. `/Game/Audio/MySound.MySound`) or empty |
| `DeviceId` | OS device id string, or empty for **default output** |
| `OutputChannel` | Integer; `0` = all channels |
| `OscAddress` | *(Optional on import; included on export.)* **Override** for the OSC base path (same as the Details field). Empty = default from **hierarchy root instance label** + component index (`GetOscAddress()`). **Import** updates this column only if the header includes `OscAddress` **and** the row has enough fields for that column — short rows leave existing overrides unchanged. Invalid non-empty values log a warning and **routing still applies**; OSC override for that row is skipped. |
| `OscBase` | *(Export only, informational.)* Resolved base path after overrides — what the subsystem uses for registry keys. |
| `OscPlay` | *(Export only.)* Full OSC path for **play** (`<OscBase>/play`). |
| `OscStop` | *(Export only.)* Full OSC path for **stop** (`<OscBase>/stop`). |
| `OscLoop` | *(Export only.)* Full OSC path for **loop** toggle (`<OscBase>/loop`). |
| `OscState` | *(Export only.)* Full OSC path for **state** monitoring (`<OscBase>/state`). |

**v1 CSV (older exports):** Six columns without `ComponentName` — `ActorName,ActorLabel,ComponentIndex,SoundWave,DeviceId,OutputChannel` — still import; matching uses `ComponentIndex` only.

**v2 without OSC columns:** Seven routing columns (with `ComponentName`) import as before; OSC fields are optional.

**Matching:** Import finds an actor whose **name** equals `ActorName` and, if `ActorLabel` is not empty in the row, whose **label** equals `ActorLabel`. If more than one actor matches (e.g. duplicate names across sublevels), the row is skipped with a warning — give actors **unique labels** and include `ActorLabel` in the CSV.

**Editor vs PIE:** The panel targets the **editor** or **PIE** world (same as the component list). Importing during **PIE** changes **PIE** instances; **save the level** in the editor to persist routing on placed actors.

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

Each component has a **base** address (e.g. `/audioloom/Mike/1`, `/audioloom/Mike/2` for multiple Audio Looms on the same actor), using the hierarchy root’s **Actor Label** (sanitized) and a **1-based** index when **OSC Address** is blank. Commands use suffixes:

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

### Diagnostics

| Node | Description |
|------|-------------|
| `Get Output Latency Ms` | Estimated OS output buffer / stream latency (ms), or 0 when not playing |

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

### A. Install the plugin (first-time setup)

1. Download `AudioLoom-vX.X.zip` from [Releases](https://github.com/NPLAudio/AudioLoom/releases) and extract it.
2. Copy the extracted `AudioLoom` folder into your project's `Plugins/` directory.
3. Restart Unreal Editor (or recompile if Live Coding is enabled).
4. **Edit → Plugins** → search for "AudioLoom" → ensure it is enabled.

### B. Basic routing to a speaker

1. Place an **Audio Loom** actor: **Place Actors** → search "Audio Loom" → drag into the level.
2. Assign a `USoundWave` in the actor's Details panel or in **Window → Audio Loom**.
3. Open **Window → Audio Loom**. Set **Device** to the target output (or leave default).
4. Set **Output Channel** to 0 (all channels) or 1, 2, 3… for a specific channel.
5. Click **Play** in the manager or call `Play` from Blueprint.

### C. OSC control from external software

1. Place an **Audio Loom** actor and assign a sound (see B).
2. Open **Window → Audio Loom**. Set **Listen Port** (e.g. 9000) and click **Check Port**.
3. Click **Start OSC** (the button turns green when running).
4. From your external app (Max/MSP, TouchOSC, etc.), send to your machine’s IP and the listen port:
   - `/audioloom/YourActorName/1/play` — start playback (1 = first Audio Loom on that actor)
   - `/audioloom/YourActorName/1/stop` — stop playback

### D. Multi-channel speaker map

1. Add one Audio Loom actor per speaker (or one actor with multiple components).
2. Give each a unique **OSC Address** in the manager (e.g. `/show/L`, `/show/R`, `/show/C`).
3. Route each component to the correct **Device** and **Output Channel**.
4. Use OSC to trigger each speaker independently (e.g. `/show/L/play`, `/show/R/play`).

### E. Monitoring in another app

1. In **Project Settings → Audio Loom > OSC**, set **Send IP** and **Send Port** (where your monitoring app listens).
2. In your external app (e.g. Max/MSP, Pure Data), open a UDP receiver on that port.
3. Open **Window → Audio Loom** and click **Start OSC**.
4. When components play or stop, you’ll receive `/base/state` with 1.0 (playing) or 0.0 (stopped).

### F. Blueprint-triggered playback

1. Add **AudioLoomComponent** to an actor: **Add Component** → search "Audio Loom".
2. Assign a sound, device, and channel in the Details panel.
3. In Blueprint, call **Play** (e.g. from Event BeginPlay or a custom event).
4. Use **Stop** and **Set Loop** nodes as needed.

---

## For developers (code map)

Source is documented inline (`/** … */` file and class headers, `//` for non-obvious blocks). High-level layout:

| Area | Path | Role |
|------|------|------|
| **Runtime module** | `Source/AudioLoom/Public/AudioLoom.h`, `Private/AudioLoom.cpp` | Module + `LogAudioLoom` |
| **Component** | `AudioLoomComponent.*` | PCM load, resample to 48 kHz, `Play`/`Stop`, auto-replay tick, OSC address helpers |
| **Backend** | `AudioLoomPlaybackBackend.*` | WASAPI (Windows) / Core Audio (macOS) output thread or IO proc |
| **Devices** | `AudioOutputDeviceEnumerator.*`, `AudioOutputDeviceInfo.h` | Output device list + channel probing |
| **PCM** | `AudioLoomPcmLoader.*` | WAV parse + `USoundWave` → float PCM |
| **OSC** | `AudioLoomOscSubsystem.*`, `AudioLoomOscSettings.*`, `AudioLoomBlueprintLibrary.*` | UDP server/client, settings, address validation |
| **Convenience actor** | `AudioLoomActor.*` | Placeable actor with one component |
| **Editor module** | `AudioLoomEditorModule.cpp`, `AudioLoomEditor.h` | Details registration + nomad tab |
| **Details UI** | `AudioLoomComponentDetails.*` | Property editor customization |
| **Manager window** | `SAudioLoomPanel.*`, `SAudioLoomExpandableRow.*` | **Window → Audio Loom** Slate UI |

**Extending:** add properties on `UAudioLoomComponent` and thread them through `Play()` → `FAudioLoomPlaybackBackend::Start` if the backend needs them. For new OSC commands, extend `UAudioLoomOscSubsystem::HandleOSCMessage` and document addresses in the panel’s OSC reference box.

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
