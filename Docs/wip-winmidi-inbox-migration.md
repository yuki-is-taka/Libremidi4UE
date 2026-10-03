---
description: Findings on the Windows MIDI Services in-box API migration (Microsoft.Windows.Devices.Midi2 → Windows.Devices.Midi2), the open port-identity questions, and the 2026-09-23 code review of winmidi defects; read before touching anything Windows-MIDI-identity-related or before the in-box migration / port-identity redesign. The 2026-10 UMP fix batch itself is in design-winmidi-ump-fixes.md
type: wip
status: investigation
updated: 2026-10-03
---

# WinMIDI in-box API migration — findings

> **Status**: Investigation for the in-box migration and the port-identity redesign. Not a design.
> The 2026-10 UMP fix batch is decided and designed elsewhere (§1).
> **Scope**: Libremidi4UE's `Source/ThirdParty/WindowsMidiServices` projection + the `winmidi`
> backend in the `libremidi` submodule (upstream `celtera/libremidi`; carried in the owner's fork
> per [adr-0003](decisions/adr-0003-libremidi-fork-strategy.md)).
> **Last verified**: 2026-09-23 (§2–§3); §1, §4 banner, §5 and §6 updated 2026-10-03.

## 1. Status / plan

**Owner decision (2026-10-03), replacing the 2026-09-23 "do nothing now":** fix the UMP-path
winmidi defects now, in the owner's fork of `libremidi`, under the fork strategy of
[adr-0003](decisions/adr-0003-libremidi-fork-strategy.md) (which supersedes
[adr-0001](decisions/adr-0001-libremidi-submodule-tracks-upstream.md)). The first batch is
checkpoint A of [design-winmidi-ump-fixes.md](design-winmidi-ump-fixes.md): the baseline bump with
the timestamp-correction removal ([adr-0004](decisions/adr-0004-winmidi-timestamp-correction-removed.md)),
upstream PR #264, (q), the per-message input dispatch for (g) and (b)
([adr-0005](decisions/adr-0005-winmidi-input-group-addressing.md)), (p), and the interim output
group stamp for (d). The remaining UMP-path defects are [backlog](backlog.md) rows, each with a
trigger. Upstream PRs are filed by the owner only.

What still waits for the in-box ship, and is what this doc investigates: the in-box API migration
(`Microsoft.Windows.Devices.Midi2` → `Windows.Devices.Midi2`), the port-identity redesign ((a),
(c), (e), the `Ordinal` row, adr-0005's output half) and one connection per endpoint ((h)). These
stay fork-only until then (adr-0003 rule 2); the date fallback lives in the backlog's
port-identity row.

**Triggers to start the in-box work:**
- The in-box API ships (Windows 11 25H2+, end of November 2026 per Microsoft's porting guide).
- RC4 (the currently bundled SDK) actually breaks — no confirmed expiry is on record; see §2.
- Movement on upstream libremidi issue #252 (a migration guide with no maintainer response as of
  2026-09-07).

## 2. Situation (as of 2026-09-23), with sources

- **Installed on the dev machine**: standalone "App SDK Runtime and Tools" RC4, `1.0.17-rc.4.25`
  (release `rc-4`, 2026-04-12,
  https://github.com/microsoft/MIDI/releases/tag/rc-4).
- **Bundled projection**: `Plugins/Libremidi4UE/Source/ThirdParty/WindowsMidiServices/`, generated
  by C++/WinRT `v2.0.250303.1` from the NuGet `1.0.17-rc.4.25` `.winmd`. Verified paths and
  contents:
  - `Source/ThirdParty/WindowsMidiServices/README.md` — states the bundled SDK version, the
    cppwinrt version, and the regeneration procedure.
  - `Source/ThirdParty/WindowsMidiServices/Win64/include/winmidi/init/WindowsMidiServicesVersion.h`
    — the authoritative auto-generated version record:
    `WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_FULL = "1.0.17-rc.4.25"`,
    `_BUILD_SOURCE = "GitHub Preview"`, `_BUILD_DATE = "2026-04-12"`,
    `_BUILD_VERSION_NAME = "SDK Release Candidate 4"`, `_BUILD_IS_PREVIEW = true`.
- Microsoft pivoted to an **in-box API** `Windows.Devices.Midi2` (releases
  `inbox-dev-preview-1`…`-9`; Preview 9 published 2026-09-21,
  https://github.com/microsoft/MIDI/releases/tag/inbox-dev-preview-9; NuGet asset
  `Windows.Devices.Midi2.0.99.83-devpreview.9.nupkg` while the release notes' prose says
  `0.99.79`; Tools `0.99.84-devpreview.9`). Versioning restarted at `0.99.x` — newer despite the
  lower number. All releases remain prerelease; no GA yet.
- Porting guide (https://microsoft.github.io/MIDI/kb/porting-midi-libraries/) quotes: the
  out-of-band `Microsoft.Windows.Devices.Midi2` package "was never supported for redistribution,
  only for testing against the preview, and it is being dropped entirely in November 2026";
  `Windows.Devices.Midi2` "is part of Windows, in box in Windows 11 25H2 and later from the end of
  November 2026, and it is the only supported Windows MIDI Services API target if you intend to
  ship"; "Windows 11 24H2 leaves support in October 2026, and so will not receive the in-box API".
  **Consequence**: production Windows machines must be 25H2+ after migration.
- RC4 carries **no documented expiry/kill switch**. The in-box previews carry no built-in expiry either:
  the Preview 9 release notes only allow distributing the preview WinRT binaries with an
  alpha/beta/preview app that enforces its own expiration no later than 2027-01-15
  ([adr-0006](decisions/adr-0006-date-fallback-rationale-corrected.md)). Preview 1 notes say RC4
  can stay installed side-by-side. **Breakage of RC4 via a
  future in-box midisrv update is INFERRED, not stated** anywhere found.
- Pre-ship in-box previews need the implementation `.dll`/`.pri` side-by-side with the host exe
  (else `REGDB_E_CLASSNOTREG`) — awkward with `UnrealEditor.exe` as host; goes away after ship.
- **Header generation**: Microsoft on #252: "during the preview period you still need a NuGet
  package for the metadata and headers... Once the metadata is in the Windows SDK proper, that
  download can go away entirely. We will comment on this issue when that happens." Preview-1 notes
  say the Windows SDK may lag the OS ship. **Inferred, not confirmed**: once in the Windows SDK,
  `winrt/Windows.Devices.Midi2*.h` appears under `Include/<ver>/cppwinrt` (already on our include
  path) and the bundled `ThirdParty/WindowsMidiServices` projection can be removed — provided UE
  builds against that SDK version.
- Dev machine (build 26300, 26H2 Dev channel) has **no** in-box `Windows.Devices.Midi2` yet.
- **Upstream libremidi state** (as of 2026-09-23):
  - #252 (migration guide by a Windows MIDI Services team member, 2026-09-07) — no maintainer
    response.
  - #254 (same author, 2026-09-07) — no maintainer response.
  - #234 (COM path drops batched UMP) — stalled.
  - `cmake/libremidi.winmidi.cmake` — **verified**: still fetches
    `https://github.com/microsoft/MIDI/releases/download/rc-2/Microsoft.Windows.Devices.Midi2.1.0.15-rc.2.15.nupkg`
    (an older `rc-2` build than the bundled UE projection's `rc-4`).
  - No branches/PRs/forks doing the in-box migration exist upstream.
  - Maintainer is active on other backends (34 commits landed on `master` since the pinned
    submodule commit, none touching the winmidi backend's `midi_in.hpp`/`midi_out.hpp`/
    `detail/conversion.hpp`; two touch `observer.hpp`/`helpers.hpp` — see §4).
  - **Submodule pin**: `67e8ccd97d4bdb6d0de634aec4425280f1ef0423` (`v5.4.2-62-g67e8ccd`).
    `origin/master` (fetched 2026-09-23) is `b9f19f7c8bd647de760cc2d69f7e7d9648b0baeb`, **34 commits
    ahead**.

## 3. API delta rc-4 → in-box (C++/WinRT consumer view)

Source: upstream #252, `inbox-dev-preview-1` release notes. Not verified against our submodule
(the in-box API does not exist in any code we carry yet) — recorded as reported.

- Root namespace: `Microsoft.Windows.Devices.Midi2` → `Windows.Devices.Midi2`.
- `Endpoints.*` → `Transports.*` (e.g. `Endpoints.Virtual` → `Transports.Virtual`).
- Enumeration types move into `.Enumeration` (+ `.Enumeration.Legacy`).
- Several declared-info / loopback-result structs became runtimeclasses that can be null (e.g.
  `GetTransportSuppliedInfo()`, `GetDeclaredEndpointInfo()` return null for plain MIDI 1.0
  devices).
- Bootstrap collapses to `Windows::Devices::Midi2::MidiApi::EnsureServiceAvailable()`.
- Removed: `MidiUniversalSystemExclusiveMessageBuilder`, `MidiClockGenerator`,
  `MidiClockDestination`, runtime version/update utilities, `IMidiEndpointConnectionSettings`
  (merged into `MidiEndpointConnectionSettings`).
- Watcher `Updated`/`Removed` args hand over the device object directly.
- Raw COM callback: `UINT32*` → `UINT32 const*` (surfaces as C2259 at the call site).
- Header names: `winrt/Microsoft.Windows.Devices.Midi2.h` → `winrt/Windows.Devices.Midi2.h`
  (+ `.Enumeration.h`, `.Transports.Virtual.h`).

## 4. Existing winmidi defects (code review of upstream `origin/master`)

> **Superseded for current state (2026-10-03).** This section is the 2026-09-23 review at
> `b9f19f7`, kept as history. Current file:line, severity and fix sketches for every defect, (a)
> to (ac), are in the [defect audit](audit-winmidi-defects-2026-10-03.md) at upstream `5c839a5`;
> hardware evidence in the [probe record](audit-winmidi-probe-2026-10-03.md); each defect's
> disposition (fixed in a checkpoint-A unit, deferred with a trigger, or outside the UMP-only
> scope) in [backlog.md](backlog.md). Statements the audit or the probe overturned are marked
> **Now:** below. Defects (j) to (ac) were found after this review and appear only in the audit
> and the backlog.

Reviewed against upstream `celtera/libremidi` `origin/master` = `b9f19f7c8bd647de760cc2d69f7e7d9648b0baeb`
(fetched 2026-09-23; the submodule pin `67e8ccd` is 34 commits behind — see §2). Paths are under
`include/libremidi/backends/winmidi/` unless noted. **Every location below was re-read directly
from `origin/master` via `git show`** (read-only; the submodule checkout itself was not touched)
and line numbers corrected where they drifted from the original report.

**a. GTB-vs-FB double enumeration + wrong `get_port` resolution (#254).**
`helpers.hpp:67-91` `get_port(device_name, group_terminal_block)` matches only against
`ep.GetGroupTerminalBlocks()` (`gp.Number() == group_terminal_block`, line 83) and returns a GTB
type — confirmed exactly at these lines. It never looks at `GetDeclaredFunctionBlocks()`, so a
function-block (FB)-derived port cannot be opened by this path.
`observer.hpp:140-196` (`get_input_ports`/`get_output_ports`) enumerates **both**
`GetDeclaredFunctionBlocks()` and `GetGroupTerminalBlocks()` for every endpoint — confirmed
exactly at these lines — so a MIDI 2.0 device that declares both FBs and GTBs for the same
group lists twice. The issue's suggested fix: a shared `blocks_for` helper (FBs if present, else GTBs)
used by both enumeration and `get_port` (~40 lines) — but it changes what `port.port` means (see
§5, open decision 1). **Now:** part of the port-identity redesign (backlog).

**b. #217 follow-up: multi-group blocks still filtered to their first group only.**
The original #217 bug (`port.port - 1` used as a raw group index) is already fixed on
`origin/master` — `midi_in.hpp:99-108` now resolves the block via `get_port` and sets
`m_group_filter = gp.FirstGroup().Index();` (line 108), with a comment (lines 103-107) explaining
exactly why `port.port` cannot be used as a group index. But a GTB/FB that **spans several
groups** (`GroupCount() > 1`) is still filtered down to only its first group: the message-receive
paths compare a single value with `==`, not membership in the block's group range —
`midi_in.hpp:195-200` (non-COM `process_message`) and `midi_in.hpp:230-235` (COM raw callback
`process_message`) both do `if (group != m_group_filter) return;`. Confirmed exactly at these
lines. **Now:** fixed by unit U2 with a range verdict
([adr-0005](decisions/adr-0005-winmidi-input-group-addressing.md)).

**c. Block direction suspected INVERTED (confirmed on hardware 2026-10-03, see below).**
Microsoft defines function-block direction from the block's own viewpoint (`BlockInput` =
destination into the device, `BlockOutput` = source out of the device — SDK doc
`docs/sdk-reference/Enumeration/MidiFunctionBlockDirectionEnum.md`, and Microsoft's own
`midi-console` sample, `src/in-box/user-tools/midi-console/endpoint_picker.cpp:160-166`; neither
file lives in our submodule and was not independently re-fetched here). Upstream's
`observer.hpp` excludes `BlockOutput` from the **input** list: `get_input_ports` checks
`gp.Direction() != MidiFunctionBlockDirection::BlockOutput` (line 153) and
`!= MidiGroupTerminalBlockDirection::BlockOutput` (line 160); `get_output_ports` excludes
`BlockInput` symmetrically (lines 182, 189). `add_block`'s hotplug switch (lines 255-263) treats
`BlockInput` → `add_input()`, `BlockOutput` → `add_output()` — the same polarity. If Microsoft's
device-viewpoint definition is right, this is backwards: a device-side `BlockOutput` (the device
sends) should appear as an **input** port to the app, and vice versa. Masked on bidirectional
blocks and on symmetric-cable devices; a send-only MIDI 1.0 device would be missing from inputs
entirely. **Now:** confirmed on hardware (all six Push 3 ports and a second interface,
[probe §3.1](audit-winmidi-probe-2026-10-03.md)); deferred into the port-identity redesign, which
fixes it by construction (backlog).

**d. Output group hard-coded — silent misroute today.**
**Now:** `detail/conversion.hpp:112` is dead code (only the uncalled `ump_from_midi1` uses it); the
real sources are cmidi2's converter context, whose group is never set (`cmidi2.hpp:2630`), and the
verbatim `send_ump` ([audit §3 (d)](audit-winmidi-defects-2026-10-03.md)). Interim fix: unit U3.
`midi_out.hpp:45-47` resolves the block (`get_port(*device_id, port.port)`)
only to validate it exists, then discards it — `gp` is never referenced again in the file,
including in `send_ump` (`midi_out.hpp:174-195`). So writing to "Port 3" of a multi-cable
interface lands on cable 1 (group 0) regardless of which port was opened.

**e. Hotplug update churns every port and ignores block-active/updated flags.**
`observer.hpp:205-214` `on_device_updated` unconditionally removes then re-adds every port of the
updated device (`remove_device` then `add_device`, marked `// OPTIMIZEME` in the source) —
confirmed exactly at these lines. It does not check
`MidiEndpointDeviceInformationUpdatedEventArgs::AreFunctionBlocksUpdated` at all (the property is
never referenced anywhere in `observer.hpp`/`helpers.hpp`), so every metadata update — not just a
block-layout change — churns ports, and IDs assigned by ordinal position can come back
differently numbered. Function-block `IsActive` is likewise never checked when enumerating or
adding blocks (`IsActive` appears exactly once in this backend, at `helpers.hpp:260`, setting it
`true` when constructing a **virtual** device's own block — never read back for a remote device).

**f. Transport filter (`wanted_port`, #968b5fa) not applied on the hotplug path.**
Commit `968b5fa` ("observer: one meaning for the transport filters on every backend") added
`wanted_port<Input>()` and wired it into `get_input_ports`/`get_output_ports` (confirmed: both now
call `wanted_port<...>(ep, gp)` instead of `to_port_info<...>(ep, gp)` directly). But
`add_block`/`add_device` (`observer.hpp:223-281`, the hotplug path invoked from
`on_device_added`/`on_device_updated`) still call `to_port_info<...>(ep, gp)` directly — confirmed
by reading the full function bodies — so a transport this observer was configured to exclude can
still appear via hotplug. That same range carries the double-add (item a) and direction (item c)
issues, since `add_device` (lines 271-281) iterates both `GetDeclaredFunctionBlocks()` and
`GetGroupTerminalBlocks()` exactly like the enumeration path.

**g. COM raw-callback input path: batch-level filtering, no groupless-message exemption.**
**Now:** this *is* #234, and crash-class: a batch over six words overruns a stack object
([probe §3.3](audit-winmidi-probe-2026-10-03.md)). Fixed by unit U2; the groupless question below
is answered by [adr-0005](decisions/adr-0005-winmidi-input-group-addressing.md). The COM raw callback `process_message(sessionId, connectionId,
timestamp, wordCount, ump)` (`midi_in.hpp:213-240`) reads the group from `ump` (the first UMP word
only, via `cmidi2_ump_get_group(ump)`, line 232) and applies the resulting pass/fail verdict to
the **entire** batch of `wordCount` words rather than per-message — confirmed by reading the
function; there is no loop splitting the batch by message. There is also no exemption in either
`process_message` overload for UMP message types that are inherently groupless (Utility/MT 0x0,
Stream/MT 0xF) — group filtering applies uniformly regardless of message type.

**h. One connection per port (optional).**
Both directions open exactly one `MidiEndpointConnection` per port: `midi_in.hpp:113`
`m_endpoint = m_session.CreateEndpointConnection(ep.EndpointDeviceId());` and `midi_out.hpp:51`
same call — confirmed exactly at both lines. The porting guide recommends one ref-counted
connection per **endpoint** (shared across the endpoint's blocks/groups) instead. Not a
correctness bug today (each port already gets its own connection), just diverges from
Microsoft's recommended pattern — lowest priority of the nine. **Now:** deferred until after the
port-identity redesign and the in-box port (backlog).

**i. `to_ns()` returns raw QPC ticks, not nanoseconds, despite the timestamp being documented
(and consumed) as ns.** `midi_in.hpp:208`/`:237` (`process_message`, both the non-COM and COM
raw-callback overloads) — confirmed exactly at these lines, present on `origin/master` as well as
the pinned submodule commit. Every other backend examined does an explicit tick/tick-domain → ns
conversion in its own `to_ns()`; winmidi is the only one that returns the raw device value
unconverted. **Now:** fixed upstream in `5c839a5` (PR #263); the wrapper-side correction is removed
in the same commit as the bump ([adr-0004](decisions/adr-0004-winmidi-timestamp-correction-removed.md)).

## 5. Open decisions for the owner

Items 1, 3 and 4 are open for the port-identity redesign; item 2 is answered.

1. **What a "port" is.** (a) per block — `port.port` changes meaning across a discovery event
   (e.g. GTB 1 today, FB 0 tomorrow, per item a); (b) endpoint + group + direction (the porting
   guide's recommendation; note libremidi's own virtual port,
   `helpers.hpp:263` `block0.GroupCount(16); // All 16 groups`, declares 16 groups and so would
   appear as 16 ports to any other process enumerating it under this model); (c) (b) restricted to
   groups actually in use by a declared block (what Microsoft's `midi-console` sample does, per
   item c's citation — not independently verified here).
2. **Group-override policy for output** — does the fixed backend override the UMP group with the
   opened port's resolved group (closing item d), or respect whatever group the caller already
   encoded in the message? **Answered (2026-10-03):** the backend stamps the port's group,
   following Microsoft's guidance that the group lives in the message. Interim rule until the
   port-identity redesign: range-membership re-stamp with a guard for asymmetric devices (unit U3;
   [adr-0005](decisions/adr-0005-winmidi-input-group-addressing.md) output note). The redesign
   makes every port one group, so the stamp becomes the port's group by construction.
3. **Inactive/mid-discovery blocks** — are function blocks with `IsActive() == false`, or blocks
   whose name is still empty mid-discovery, listed as ports at all?
4. **`port.port` semantics is upstream's call.** Any change to what a port ID means is a decision
   for `celtera/libremidi` maintainers; until they decide, the fork carries ours under
   [adr-0003](decisions/adr-0003-libremidi-fork-strategy.md).

## 6. Downstream impact

**Libremidi4UE.** `Source/Libremidi4UE/Private/LibremidiTypes.cpp:233-235` — confirmed:

```cpp
case WINDOWS_MIDI_SERVICES:
	// MidiGroupTerminalBlock::Number() — 1-based terminal block ordinal
	return static_cast<int32>(Port.port);
```

`FLibremidiPortInfo::ComputeOrdinalFromNative` computes `Ordinal` as the raw GTB number for the
Windows backend. If upstream's port model changes what `port.port`/`Number()` means (open
decision 1), this ordinal — and everything that keys off it — must follow.

**Consumers.** A typical downstream consumer (verified against one, 2026-09-23):
- builds a stable port id from the device identifier and the native port handle — on Windows
  `in:<EndpointDeviceId>:<GTB number>` / `out:<EndpointDeviceId>:<GTB number>`, so the GTB number
  is part of a persisted id;
- reconnects a saved binding without comparing that id: it rebuilds an `FLibremidiPortInfo` from
  the saved `DisplayName`, `DeviceName`, `PortName`, `Manufacturer`, `Serial` and `Ordinal`, then
  calls `FindClosestPort`, a weighted heuristic match (delegating to
  `libremidi::find_closest_port`), with no exact-id attempt first. `Ordinal` (the GTB number, per
  the point above) is one of the fields the heuristic weighs.
- Consequence: a change to what a port number means changes persisted ids and shifts the
  heuristic. Exact-identity reconnect needs a group in the key, which libremidi's port information
  does not carry until (a); it lands with the port-identity redesign (backlog `Ordinal` row).

## 7. Test plan sketch

- Record `midi endpoint properties <id> --verbose` block layout first — GTBs only show with
  `--verbose`.
- **GTB-only**: USB MIDI 1.0 devices with symmetric AND asymmetric jack layouts (asymmetric is
  needed to catch item c's masking). The loopback transport also synthesizes GTBs.
- **FB-only**: a libremidi `open_virtual_port` opened from another process, and Microsoft's sample
  `simple-app-to-app-midi` (function blocks with different groups/directions).
- **FB+GTB together**: needs real MIDI 2.0 USB hardware (or a ProtoZOA / Pico TinyUSB UMP device),
  also useful to reproduce the GTB→FB switch after plug-in.
- Extract block selection / direction / group expansion as pure functions so they can carry unit
  tests independent of a live device.
