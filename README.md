# Libremidi4UE

An Unreal Engine plugin that wraps [libremidi](https://github.com/celtera/libremidi) (v5.4.2), providing cross-platform MIDI 1.0 and MIDI 2.0 (UMP) support with hotplug device discovery.

## Features

- **MIDI 1.0 and MIDI 2.0 (UMP)** — Dual-protocol support with separate message types, no runtime branching
- **Cross-platform** — Windows (WinMM + Windows MIDI Services), macOS (CoreMIDI), Linux (ALSA)
- **Hotplug detection** — Automatic device discovery with add/remove delegates
- **Blueprint-ready** — All core types, enums, and subsystem methods exposed to Blueprints
- **Engine Subsystem** — `ULibremidiEngineSubsystem` manages the observer lifecycle and port tracking
- **Editor Settings** — Backend API and protocol configurable via Project Settings > Plugins > Libremidi4UE
- **Header-only libremidi** — No pre-built binaries needed; compiles directly from source

## Requirements

- **Unreal Engine** 5.x (tested with 5.7)
- **C++20** compiler support
- **Platform-specific**:
  - **Windows**: Visual Studio 2022, Windows 10 19H1+ (Windows 11 recommended for MIDI 2.0)
  - **macOS**: macOS 11+ for MIDI 2.0
  - **Linux**: ALSA development libraries (`libasound2-dev`), kernel 6.5+ for MIDI 2.0

## Installation

1. Clone into your project's `Plugins/` directory:
   ```bash
   git clone --recurse-submodules https://github.com/yuki-is-taka/Libremidi4UE.git
   ```

2. If you already cloned without `--recurse-submodules`:
   ```bash
   cd Plugins/Libremidi4UE
   git submodule update --init --recursive
   ```

3. Regenerate project files and build.

### Windows MIDI Services setup (Windows only)

The `Source/ThirdParty/WindowsMidiServices/` directory contains a minimal set of C++/WinRT projection headers for Windows MIDI Services (generated with `cppwinrt.exe v2.0.250303.1`). These are required for MIDI 2.0 support on Windows. See [Source/ThirdParty/WindowsMidiServices/README.md](Source/ThirdParty/WindowsMidiServices/README.md) for details.

### Known build issue

After updating the libremidi submodule, you may need to patch:

**File**: `Source/ThirdParty/libremidi/libremidi/include/libremidi/backends/winmidi/helpers.hpp`

Remove the duplicate `using namespace Windows::Devices::Enumeration;` line (without `winrt::` prefix). The line with `winrt::Windows::Devices::Enumeration` should remain.

## Quick Start

### C++

```cpp
#include "LibremidiEngineSubsystem.h"
#include "LibremidiInput.h"

// Get the subsystem
auto* Subsystem = GEngine->GetEngineSubsystem<ULibremidiEngineSubsystem>();

// List available input ports
TArray<FLibremidiInputInfo> Inputs = Subsystem->GetInputPorts();

// Create and open an input
ULibremidiInput* MidiIn = Subsystem->CreateInput();
MidiIn->OnMidi1Message.AddDynamic(this, &UMyComponent::HandleMidiMessage);
MidiIn->OpenInput(Inputs[0]);
```

### Blueprint

1. Get the `LibremidiEngineSubsystem` node
2. Call `Get Input Ports` / `Get Output Ports` to enumerate devices
3. Use `Create Input` / `Create Output` to create MIDI I/O objects
4. Bind to `On Midi1 Message` or `On Ump Message` events
5. Call `Open Input` / `Open Output` with a port info

## Architecture

```
ULibremidiEngineSubsystem (UEngineSubsystem)
  +-- Observer (libremidi::observer) — hotplug detection
  +-- ULibremidiInput[]  — MIDI input ports
  +-- ULibremidiOutput[] — MIDI output ports

ULibremidiSettings (UDeveloperSettings)
  +-- BackendProtocol (Midi1 / Midi2)
  +-- BackendAPIName ("Auto" / specific backend)
  +-- Track filters (hardware / virtual / any)
```

### Key types

| Type | Description |
|------|-------------|
| `FLibremidiMidi1Message` | MIDI 1.0 message wrapper |
| `FLibremidiUmpMessage` | MIDI 2.0 UMP message wrapper |
| `FLibremidiInputInfo` / `FLibremidiOutputInfo` | Port metadata with identification |
| `ULibremidiInput` / `ULibremidiOutput` | MIDI I/O objects with delegates |

## Platform backends

| Platform | MIDI 1.0 | MIDI 2.0 |
|----------|----------|----------|
| Windows  | WinMM    | Windows MIDI Services |
| macOS    | CoreMIDI | CoreMIDI (macOS 11+) |
| Linux    | ALSA     | ALSA (kernel 6.5+) |

## License

This plugin is licensed under the [BSD 2-Clause License](LICENSE).

### Third-party licenses

- **libremidi** — BSD 2-Clause / MIT (see [Source/ThirdParty/libremidi/libremidi/LICENSE.md](Source/ThirdParty/libremidi/libremidi/LICENSE.md))
- **Windows MIDI Services SDK** — MIT (see [Source/ThirdParty/WindowsMidiServices/Licenses/LICENSE.txt](Source/ThirdParty/WindowsMidiServices/Licenses/LICENSE.txt))
