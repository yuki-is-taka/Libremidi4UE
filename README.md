# Libremidi4UE

An Unreal Engine plugin that wraps [libremidi](https://github.com/celtera/libremidi), providing cross-platform MIDI 1.0 and MIDI 2.0 (UMP) support with hotplug device discovery.

libremidi is carried as a git submodule from a fork, [yuki-is-taka/libremidi](https://github.com/yuki-is-taka/libremidi) (branch `libremidi4ue`): upstream `master` plus any fixes this plugin needs that upstream does not have yet. Every pinned commit has an immutable `libremidi4ue-pin-*` tag on the fork. See [Docs/decisions/adr-0003](Docs/decisions/adr-0003-libremidi-fork-strategy.md).

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

3. If your clone predates the switch of the submodule to the fork (October 2026), point it at the fork once:
   ```bash
   cd Plugins/Libremidi4UE
   git submodule sync --recursive
   git submodule update --init --recursive
   ```

4. Regenerate project files and build.

### Windows MIDI Services headers (Windows only)

The `Source/ThirdParty/WindowsMidiServices/` directory contains a minimal set of C++/WinRT projection headers for Windows MIDI Services (generated with `cppwinrt.exe v2.0.250303.1`). These are required for MIDI 2.0 support on Windows. See [Source/ThirdParty/WindowsMidiServices/README.md](Source/ThirdParty/WindowsMidiServices/README.md) for details.

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

## Port Identifiers and Device Grouping

Each `FLibremidiInputInfo` / `FLibremidiOutputInfo` carries a set of identifiers sourced from the host MIDI API. On top of what libremidi exposes in C++, this plugin serializes all identifier fields as `UPROPERTY`s, making them accessible from Blueprint and enabling device-level grouping across the input/output boundary.

### Identifier fields

| Field | Type | What it represents |
|-------|------|-------------------|
| `ContainerId` | UUID / string / integer / none | Physical device container. **Ports on the same device share a `ContainerId`**, regardless of whether they are inputs or outputs. |
| `DeviceId` | string / integer / USB VID+PID / none | Endpoint- or device-level identifier within the container |
| `PortHandle` | `int64` | API-internal port reference. Encoding is platform-specific (see below). Do not compare across platforms. |
| `ClientHandle` | `int64` | API-internal client reference. Session-scoped; never persist this value. |
| `Serial` | `FString` | Manufacturer-supplied serial number. Theoretically the strongest persistent identifier, but virtually no MIDI device exposes one — expect this to be empty in practice. |
| `Ordinal` | `int32` | Per-device port index for stable ordering. Computed from API-specific logic (CoreMIDI entity index, WinMIDI terminal block number, ALSA subdevice number, etc.). |

### Identifier persistence by platform

Persistence has three levels:

| Level | Meaning |
|-------|---------|
| **True** | Survives replug to any port, reboot, and new sessions |
| **Partial** | Stable while plugged into the **same physical USB port**; breaks if the device is moved |
| **Session** | Valid only while the device stays connected; changes on replug |
| **None / type-level** | May change at any time, or identifies the device model rather than the individual unit |

| Field | Windows MIDI Services | WinMM | macOS (CoreMIDI) | ALSA |
|-------|-----------------------|-------|------------------|------|
| **`ContainerId`** | ContainerID GUID — **Partial** (Windows may re-enumerate on replug) | *(not provided)* | USBLocationID — **Partial** (USB port position) | udev `ID_PATH` — **Partial** (USB bus path; stable across reboots on same port) |
| **`DeviceId`** | EndpointDeviceId string — **Session** (changes on reboot) | MIDI caps VID:PID — **type-level only**¹ | USBVendorProduct — **type-level only**¹ | udev sysfs path — **Partial / unstable**² |
| **`PortHandle`** | TerminalBlockNumber — **True** (hardware-defined, 1-based) | Port index — **None** (enumeration order, changes on hotplug) | MIDIUniqueID — **Session** (CoreMIDI issues a new ID on every reconnect) | Packed card/device/subdevice — **None** (card numbers are dynamically assigned) |
| **`Serial`** | SerialNumber — **True**³ | *(not provided)* | *(not provided)* | udev `ID_SERIAL` — **True** (if present) |

> ¹ VID:PID identifies the device model, not the individual unit. Two identical devices have the same value.
>
> ² The sysfs path includes a USB device number that can be reassigned on reboot, even for the same physical port.
>
> ³ libremidi reads `SerialNumber` (Windows MIDI Services) and udev `ID_SERIAL` (ALSA) when available, but per libremidi's own documentation, virtually no MIDI device manufacturer implements a per-unit serial number. In practice this field is almost always empty.

**Bottom line:** The only configuration that guarantees true per-unit persistent identification is **Windows MIDI Services with a MIDI 2.0 device that exposes a serial number**. Every other platform provides at best partial, port-location-based persistence.

### Grouping ports by physical device

Ports that belong to the same physical device share the same `ContainerId`. This means you can cross-reference `GetInputPorts()` and `GetOutputPorts()` to find, for example, the output port that pairs with a given input — including across the input/output boundary. No dedicated API exists for this; grouping requires comparing `ContainerId` values manually (checking `Type` and the relevant field: `UUIDBytes`, `String`, or `Integer`).

> **Note:** `ContainerId` is absent (`Type == None`) on WinMM and most virtual/software ports.

### Reconnection: FindClosestPort

When a device disconnects and reconnects, its identifiers may change (see table above). `FLibremidiPortInfo::FindClosestPort` uses a weighted heuristic to find the best candidate in a new port list. This wraps `libremidi::find_closest_port` with full UPROPERTY-serialized inputs, making saved port state usable across sessions.

| Priority | Field(s) | Hit / Miss |
|----------|----------|------------|
| 1 | `ContainerId` + `DeviceId` | +1000 / −2000 |
| 2 | `Serial` | +800 / −1000 |
| 3 | `DisplayName`, `PortName`, `DeviceName` | up to +400 each (Levenshtein similarity ≥ 0.5) |
| 4 | `Manufacturer`, `Product` | +100 / −2000 |
| 5 | `PortHandle` | +50 (tie-break only) |

A result with `Score ≤ 0` or `!IsValid()` means no match was found. Ports from a different API never match.

```cpp
// After a hotplug event, re-match a previously saved port
FLibremidiPortInfo::FMatchResult Result = SavedPortInfo.FindClosestPort(
    TArrayView<const FLibremidiPortInfo>(NewInputPorts));

if (Result.IsValid())
{
    MidiIn->OpenInput(NewInputPorts[Result.Index]);
}
```

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
