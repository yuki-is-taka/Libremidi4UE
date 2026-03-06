# Device Aggregation & Persistent ID Implementation

**Date**: January 2026  
**Status**: Implementation Complete (Device cache with hotplug support)

## Overview

This document describes the device aggregation and persistent identification system for Libremidi4UE. The goal is to group individual MIDI ports into logical devices and assign stable, device-unique identifiers that survive disconnect/reconnect cycles.

## Problem Statement

- **Per-port limitation**: Current implementation tracks individual MIDI ports (input/output) separately
- **No device grouping**: Multiple ports belonging to the same physical device are treated independently
- **Unstable identification**: Port handles (ClientHandle/PortHandle) change on reconnection, breaking persistence
- **Backend fragmentation**: Different MIDI backends expose device identity differently (CoreMIDI vs Windows MIDI Services vs ALSA)

## Solution Architecture

### 1. Device ID (`FLibremidiDeviceId`)

A cross-platform, stable identifier for a device based on backend-specific metadata.

**Location**: `LibremidiTypes.h/cpp`

**Fields**:
- `Backend`: ELibremidiAPI (which MIDI backend was used to discover this device)
- `Authority`: ELibremidiDeviceIdAuthority (how the ID was derived)
- `CanonicalHashHex`: MD5 hash of normalized backend-specific identifiers (the primary key)
- `VendorProduct`: Optional uint32 (e.g., CoreMIDI USBVendorProduct as 32-bit VID:PID)
  - Used in TransportAddress authority hash for device type classification
  - Not used in DeviceUnique authority (stored as metadata only)
- `TransportLocation`: Optional uint64 (e.g., CoreMIDI USBLocationID, ALSA card/device number)
  - Used in TransportAddress authority hash as physical location identifier
  - Not used in DeviceUnique authority (stored as metadata only)
- `TransportAddressStr`: FString (e.g., Windows MIDI Services EndpointDeviceId, ALSA hw:X,Y)
  - Used in TransportAddress authority hash as a human-readable / backend-specific address
  - Not used in DeviceUnique authority (stored as metadata only)
- `DeviceString`: FString (e.g., Windows MIDI Services DeviceInterfaceId, USB serial number)
  - Used in DeviceUnique authority as true device identifier
  - May also store supplementary data for TransportAddress/Generated authorities
- `ContainerUUID`: FString (e.g., Windows MIDI Services container GUID)
- `DebugLabel`: FString (human-readable hint for debugging)

**Authority Levels**:
- `DeviceUnique`: Stable device-level identifier provided by backend (highest priority)
  - Examples: MIDI 2.0 Device Identity, Windows MIDI Services DeviceInterfaceId, device serial number
  - Persistence: Survives USB port moves and system reboots
- `TransportAddress`: Physical address (USB location, ALSA card.device) or stable system-generated identifier
  - Used when true device ID is unavailable
  - Persistence: Stable while connected to same physical port, or system-scoped (backend-dependent)
- `Generated`: Last-resort generated ID from names (lowest priority, may change on reconnect)

### 2. Device Container (`FLibremidiDevice`)

Groups all ports belonging to a single physical device.

**Location**: `LibremidiTypes.h`

**Fields**:
- `Id`: FLibremidiDeviceId (device-unique stable identifier)
- `Manufacturer`: FString (e.g., "Roland", "Yamaha")
- `Model`: FString (device model name)
- `DisplayName`: FString (user-friendly name for UI)
- `PortTypeBits`: int32 (bitwise OR of ELibremidiPortType across all ports)
- `Inputs`: TArray<FLibremidiInputPort> (all input ports on this device)
- `Outputs`: TArray<FLibremidiOutputPort> (all output ports on this device)

**Helper**:
- `GetLabel()`: Returns `DisplayName [CanonicalHashHex]` for unambiguous UI identification

### 3. Aggregation API (`ULibremidiEngineSubsystem`)

**Location**: `LibremidiEngineSubsystem.h/cpp`

**New Function (C++ only for now)**:
```cpp
TArray<FLibremidiDevice> GetAvailableDevices() const;
```

Returns the cached device list, automatically updated on initialization and hotplug events.

**Internal Device Cache**:
- `TArray<FLibremidiDevice> CachedDevices` - Cached device list maintained by subsystem
- `TMap<FString, int32> DeviceIndexByKey` - Fast lookup map from grouping key to device index
- `void RebuildDeviceCache()` - Rebuilds entire device cache from observer snapshot (called on init)
- `void AddOrUpdateDevicePort(port, bIsInput)` - Incrementally adds port to existing device or creates new device (called on hotplug add)
- `void RemoveDevicePort(port, bIsInput)` - Removes port from device, deletes device if empty (called on hotplug remove)

**Cache Update Strategy**:
- **Initialization**: `RebuildDeviceCache()` aggregates all ports into `CachedDevices` after observer starts
- **Hotplug add**: `AddOrUpdateDevicePort()` searches for existing device by grouping key, adds port to it (or creates new device)
- **Hotplug remove**: `RemoveDevicePort()` removes port from device, removes device from cache if no ports remain

**Performance**:
- `GetAvailableDevices()` is O(1) copy of cached array
- Hotplug updates are O(1) for device lookup via `DeviceIndexByKey`
- No expensive per-frame aggregation required

## Backend-Specific ID Strategies

All backends follow the same two-tier strategy based on the availability of true device identifiers:

### Tier 1: True Device Identifier Available (Authority: `DeviceUnique`)

When a backend provides a stable, device-unique identifier that survives physical reconnection.

**Hash Formula**: Backend-specific (see Grouping Details sections below)

**Backend Examples**:
- **Windows MIDI Services (MIDI 2.0 only)**: EndpointDeviceId derived from MIDI 2.0 Device Identity
- **CoreMIDI (Future)**: MIDI 2.0 Device Identity, or kMIDIPropertyDeviceID if available
- **ALSA (Rare)**: USB serial number if exposed by device

**Characteristics**:
- ✅ Survives disconnect/reconnect to same or different USB port
- ✅ Survives system reboot
- ✅ Distinguishes multiple identical devices (each has unique ID)
- ✅ Device-originated identifier (not system-generated)

**Persistence**: Maximum stability; device ID remains constant across all connection scenarios.

**Note**: MIDI 2.0 devices expose true device-unique identifiers via protocol. MIDI 1.0 devices do not have this capability and must use Tier 2.

---

### Tier 2: Transport Address Only (Authority: `TransportAddress`)

When true device ID is unavailable, use physical transport address or system-generated identifier.

**Hash Formula**: Backend-specific (see Grouping Details sections below)

**Backend Examples**:
- **Windows MIDI Services (MIDI 1.0)**: EndpointDeviceId (system-generated GUID, stable but not device-originated)
- **CoreMIDI (Current)**: USBLocationID + VendorProduct + DeviceName
- **ALSA**: Card number + device number (hw:X,Y) + DeviceName
- **WinMM**: Device index + DeviceName (unstable)
- **Jack/PipeWire**: Client name + port name

**Characteristics**:
- ✅ Stable while device remains in system (MIDI 1.0 on Windows MIDI Services)
- ✅ Stable while connected to same physical port (other backends)
- ✅ Distinguishes multiple identical devices (different physical ports or system-assigned IDs)
- ✅ Handles devices without VendorProduct via DeviceName fallback
- ⚠️ Device ID may change if moved to different USB port (CoreMIDI, ALSA)
- ⚠️ May change after system reboot or driver update (backend-dependent)

**Persistence**: Session-scoped or system-scoped (backend-dependent); ID remains stable under normal operation but lacks device-originated guarantees.

**Note for Windows MIDI Services + MIDI 1.0**: EndpointDeviceId is system-generated but persistent across USB moves and reboots. While not a true device-originated ID, it provides stability comparable to DeviceUnique in practice. Consider this a hybrid case.

---

### Fallback: Generated ID (Authority: `Generated`)

When neither device ID nor stable transport address is available:

**Hash Formula**:
```
hash(backend + auth="generated" + display_name + device_string)
```

**Backend Examples**:
- Legacy/virtual MIDI devices without hardware metadata
- Software MIDI synthesizers

**Characteristics**:
- ⚠️ May change on any reconnection
- ⚠️ Cannot reliably distinguish multiple identical devices

**Persistence**: Minimal; use only as last resort.

## Aggregation Algorithm

1. **Snapshot**: Fetch all input and output ports from libremidi observer

2. **Detect Authority & Group Ports**: For each port, determine available identification metadata and group accordingly
   
**DeviceUnique Authority** (if available):
- **Windows MIDI Services (MIDI 2.0)**: Group by EndpointDeviceId (derived from MIDI 2.0 Device Identity)
- **CoreMIDI (Future)**: Group by MIDI 2.0 Device Identity or kMIDIPropertyDeviceID
- **ALSA (Rare)**: Group by USB serial number if exposed
   
**TransportAddress Authority** (fallback):
- **Windows MIDI Services (MIDI 1.0)**: Group by EndpointDeviceId (system-generated, but stable)
- **CoreMIDI**: Group by (VendorProduct, USBLocationID, DeviceName)
- **ALSA**: Group by (card number, device number, DeviceName)
- **WinMM**: Group by (device index, DeviceName)
- **Jack/PipeWire**: Group by (client name, DeviceName)
   
**Generated Authority** (last resort):
- Group by DisplayName only

3. **Fold Ports**: For each group, collect metadata (manufacturer, model, type bits) and populate Inputs/Outputs arrays

4. **Finalize Device ID**: Build FLibremidiDeviceId using detected authority
   - DeviceUnique: Call `MakeDeviceUnique(backend, device_id_string)`
   - TransportAddress: Call `MakeTransportAddress(backend, vendor_product, transport_location, transport_address, device_name)`
   - Generated: Call `MakeGeneric(backend, Generated, display_name, ...)`

5. **Return**: Array of FLibremidiDevice sorted by hash

**Important**: Authority determination happens during grouping (step 2), not after. This ensures ports with the same true device ID are grouped together, even if they have different physical addresses.

## Implementation Details

### Hash Helpers (Private Namespace)

**Location**: `LibremidiTypes.cpp`

- `HexEncode(const uint8* Data, int32 Length)`: Converts bytes to hex string
- `HashStringUtf8(const FString& Payload)`: MD5 hash of UTF-8 string
- `HashBytes(const TArray<uint8>& Payload)`: MD5 hash of byte array

### Device ID Builders

**Location**: `LibremidiTypes.cpp`

#### `FLibremidiDeviceId::MakeDeviceUnique(...)`
- Accepts backend, true device identifier string (e.g., DeviceInterfaceId, serial number)
- Uses `DeviceUnique` authority for maximum stability
- Encodes backend + auth + device identifier into payload
- Returns FLibremidiDeviceId with CanonicalHashHex set

#### `FLibremidiDeviceId::MakeTransportAddress(...)`
- Accepts backend, optional vendor/product code (uint32), optional transport location (uint64), transport address string, device name
- Uses `TransportAddress` authority for session-scoped stability
- Encodes backend + auth + optional(vendor_product) + optional(transport_location) + transport_address + device_name into payload
- DeviceName serves as fallback identifier when VendorProduct is unavailable (virtual MIDI devices)
- Returns FLibremidiDeviceId with CanonicalHashHex set

#### `FLibremidiDeviceId::MakeGeneric(...)`
- Accepts backend, authority, payload label, and raw byte payload
- Appends label, backend, and authority to payload for mixing
- Generic builder for platforms without specific ID logic
- Returns FLibremidiDeviceId with Generated authority

### Port Aggregation Helpers (Private Namespace)

**Location**: `LibremidiEngineSubsystem.cpp`

- `FDeviceAggregate`: Temporary struct to hold ports during aggregation
- `DetectAuthorityAndBuildKey(...)`: Determines authority level based on available metadata, then generates grouping key
  - Returns tuple of (Authority, GroupingKey)
  - Ensures ports with same true device ID are grouped together
- `FoldPortIntoAggregate(...)`: Merges port metadata into aggregate
- `FinalizeDeviceId(...)`: Builds FLibremidiDeviceId using authority and metadata collected during grouping

## Future Design Considerations

### Blueprint Exposure
Currently `FLibremidiDevice` is C++-only (not exposed to Blueprint). Future work may wrap it for Blueprint compatibility via USTRUCT/UPROPERTY.

### Device-Level Hotplug Events
Current hotplug delegates fire per-port. Future enhancement: aggregate ports into devices and fire device-level events (OnDeviceAdded/OnDeviceRemoved).

### Legacy Port ID Migration
`FLibremidiPortInfo::GetPersistentID()` currently prioritizes ContainerUUID/DeviceInteger/DeviceString. TODO: Consider delegating to FLibremidiDeviceId for unified UI label strategy once device aggregation is fully integrated.

### MIDI 2.0 Device Unique IDs
If future libremidi versions expose native MIDI 2.0 Device Identity or other true device-unique identifiers at the container level, the implementation can automatically upgrade to `DeviceUnique` authority for maximum persistence. The two-tier strategy (DeviceUnique vs TransportAddress) is designed to accommodate this future enhancement seamlessly.

### Authority Upgrade Path
When a device is discovered with a true device identifier for the first time, existing saved configurations using `TransportAddress` authority can be migrated to `DeviceUnique` authority via user confirmation or heuristic matching.

### Authority Downgrade Handling
If a device loses its true identifier (e.g., driver downgrade, firmware update), saved configurations using old `DeviceUnique` ID cannot auto-migrate. Consider storing DisplayName + Manufacturer as secondary matching keys for heuristic recovery in user-facing configuration dialogs.

### Known Limitation: Multi-Backend Duplicates
When multiple backends expose the same physical device (e.g., CoreMIDI + Jack on macOS), the device will appear twice with different IDs in `GetAvailableDevices()`. This is expected behavior; users should select their preferred backend via subsystem configuration to avoid duplicates.

## Testing Strategy

1. **Single Device, Multiple Ports**: Connect a multi-port MIDI interface (e.g., 4-in/4-out) and verify all inputs/outputs are grouped into one FLibremidiDevice with matching ID

2. **Device Reconnect (Same Port)**: Disconnect and reconnect the same device to the same USB port; verify CanonicalHashHex remains constant for both `DeviceUnique` and `TransportAddress` authorities

3. **Device Move (Different USB Port)**:
   - **DeviceUnique authority** (e.g., Windows MIDI Services): Verify CanonicalHashHex remains constant
   - **TransportAddress authority** (e.g., CoreMIDI without true ID): Verify CanonicalHashHex changes (expected behavior)

4. **Multiple Identical Devices**: Connect two identical MIDI devices to different USB ports; verify they get unique FLibremidiDeviceIds (different transport addresses or device IDs)

5. **Cross-Platform**: Test on Windows (MIDI Services), macOS (CoreMIDI), and Linux (ALSA) to confirm backend-specific logic correctly determines authority level

6. **Authority Persistence**: Verify that devices with `DeviceUnique` authority maintain same ID across:
   - USB port changes
   - System reboots
   - Driver updates

## Backend-Specific Grouping Details

### CoreMIDI

**Grouping Key**: `(VendorProduct, USBLocationID, DeviceName)`

**Authority**: `TransportAddress`

**Hash Formula**:
```
hash(backend="CoreMIDI" + 
     auth="transport" + 
     vendor_product + 
     usb_location_id + 
     device_name)
```

**Rationale**:
- `USBLocationID`: Physical USB port location (changes when moved)
- `VendorProduct`: Device model identification (stable across reconnects)
- `DeviceName`: Distinguishes multiple logical devices on same USB hub

**Edge Cases**:
- **Virtual MIDI (IAC Driver)**: VendorProduct and USBLocationID are null → falls back to DeviceName only
- **Multi-port devices**: All ports from same physical device share same grouping key → correctly aggregated into one FLibremidiDevice

---

---

### Windows MIDI Services

**Grouping Key**: `EndpointDeviceId` (string GUID)

**MIDI 2.0 Devices** (Future):
- **Authority**: `DeviceUnique`
- **Hash Formula**: `hash(backend="WindowsMidiServices" + auth="device" + endpoint_device_id)`
- **Rationale**: EndpointDeviceId is derived from MIDI 2.0 Device Identity (device-originated)
- **Persistence**: Maximum stability; survives USB moves, reboots, driver updates

**MIDI 1.0 Devices** (Current):
- **Authority**: `TransportAddress`
- **Hash Formula**: `hash(backend="WindowsMidiServices" + auth="transport" + endpoint_device_id)`
- **Rationale**: EndpointDeviceId is system-generated by Windows MIDI Services (not device-originated)
- **Persistence**: High stability in practice; survives USB moves and reboots, but lacks device-originated guarantee
- **Note**: Unlike other TransportAddress implementations, uses EndpointDeviceId alone without vendor_product or device_name, as it already encodes sufficient uniqueness

**Port Aggregation**:
- All ports with the same EndpointDeviceId are grouped into one `FLibremidiDevice`
- Bidirectional ports are split into separate input and output entries
- Inputs collected in `Inputs` array, outputs in `Outputs` array

**Example - Roland A-88MKII (MIDI 1.0)**:
```
EndpointDeviceId: "\\?\swd#midisrv#midiu_ksa_roland_a88mkii_{GUID}" (system-generated)
Authority: TransportAddress (not device-originated)
  Inputs: [Port 1, Port 2]
  Outputs: [Port 1, Port 2]
→ One FLibremidiDevice with 2 input ports and 2 output ports
```

**Edge Cases**:
- **Diagnostics ports**: Filtered out by implementation
- **Virtual MIDI**: Has software-generated EndpointDeviceId

**Implementation Note**:

**Current Status**: libremidi does not expose properties to distinguish MIDI 2.0 
device-originated IDs from MIDI 1.0 system-generated IDs.

**Interim Solution**: All Windows MIDI Services devices are treated as 
`TransportAddress` authority. This provides equivalent stability to `DeviceUnique` 
in practice, as EndpointDeviceId persists across USB moves and reboots regardless 
of MIDI protocol version.

**Future Enhancement**: When libremidi exposes `NativeDataFormat` or similar 
properties, MIDI 2.0 devices can be upgraded to `DeviceUnique` authority while 
maintaining backward compatibility with existing saved configurations.

---

### Other Backends (TODO)

**ALSA**:
- TODO: Document grouping strategy using card/device numbers
- Expected Authority: `TransportAddress`
- Expected Grouping Key: `(card, device, DeviceName)`

**WinMM** (Legacy):
- TODO: Document grouping strategy using device index
- Expected Authority: `TransportAddress` (unstable)
- Note: Device index may change on system reboot

**Jack**:
- TODO: Document grouping strategy using client/port names
- Expected Authority: `TransportAddress`
- Expected Grouping Key: `(client_name, DeviceName)`

**PipeWire**:
- TODO: Document grouping strategy
- Expected Authority: `TransportAddress`

---

## Files Changed

- **`LibremidiTypes.h`**: Added FLibremidiDeviceIdAuthority enum, FLibremidiDeviceId struct, FLibremidiDevice struct
- **`LibremidiTypes.cpp`**: Implemented FLibremidiDeviceId builders, hash helpers, TODO comment on GetPersistentID()
- **`LibremidiEngineSubsystem.h`**: Added GetAvailableDevices() declaration
- **`LibremidiEngineSubsystem.cpp`**: Implemented GetAvailableDevices() and aggregation helpers
