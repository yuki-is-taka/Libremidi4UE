---
description: winmidi's raw QPC-tick timestamps are corrected to true nanoseconds inside the Libremidi4UE wrapper, keyed on API + timestamp_mode
type: decision
status: accepted
updated: 2026-09-27
---

# ADR-0002: winmidi timestamp tick correction

## Context and problem statement

`Source/ThirdParty/libremidi/libremidi/include/libremidi/backends/winmidi/midi_in.hpp:208` and
`:237` both do `auto to_ns = [t = <raw WMS timestamp>] { return t; };` — the winmidi backend
(`WINDOWS_MIDI_SERVICES`) returns the raw QPC (`QueryPerformanceCounter`) tick count unconverted,
despite `to_ns()`'s name and every other examined backend's contract promising true nanoseconds.
`detail/midi_stream_decoder.hpp`'s `input_state_machine_base::timestamp<info>` routes this raw value
through unconverted for the `Absolute`, `Relative`, and `Custom` `timestamp_mode` branches (the
`SystemMonotonic` branch is unaffected — winmidi's `absolute_is_monotonic` is `false`, so it falls
back to `system_ns()`, which is already true ns). Thal requests `Absolute`
(`ThalMidiBackendLibremidi.cpp`), the confirmed-broken branch. `IThalMidiInput::NormalizeTimestamp`
(`ThalMidiPort.h`/`.cpp`) then divides by 1e9 assuming true ns, so the device clock reads roughly
100x slow relative to `FPlatformTime`, and `FThalStrokeTracker::CollectStale`'s 0.25s timeout fires
almost immediately after a stroke begins.

## Considered options

- Fix `to_ns()` inside the submodule.
- Reroute `Absolute` through `Custom`'s `get_timestamp` hook internally.
- Convert in `HandleMessage`/`HandleUmpMessage`, keyed on API + `timestamp_mode`, snapshotted at
  `Initialize`.

## Decision outcome

Chosen: convert in `HandleMessage`/`HandleUmpMessage`, entirely inside the Libremidi4UE wrapper —
no submodule edit, no ADR superseding ADR-0001. `ULibremidiInput` snapshots the API the constructed
`midi_in` actually resolved to (`get_current_api()`) and the `timestamp_mode` baked into its
`Config` at `Initialize` time (not read live from the `TimestampMode` member, since
`SetTimestampMode` does not touch an already-constructed `midi_in`'s `Config`). On Windows, when the
active API is `WINDOWS_MIDI_SERVICES`, `Initialize` also queries `QueryPerformanceFrequency` once
and caches it. `HandleMessage`/`HandleUmpMessage` then convert the message's raw-tick timestamp to
true ns via `Libremidi4UE::ConvertTicksToNs` (`LibremidiTimestampConversion.h`) whenever
`Libremidi4UE::NeedsWinmidiTickCorrection(ActiveApi, ActiveTimestampMode)` is true — i.e. only for
`Absolute`/`Relative`/`Custom` on `WINDOWS_MIDI_SERVICES`; `NoTimestamp`/`AudioFrame` never route
through `to_ns()`, and `SystemMonotonic` is already correct ns and must not be converted a second
time.

- Rejected fixing the submodule: reopens the fork question ADR-0001 just closed, and this is
  arguably an upstream bug worth filing there rather than carrying a local patch.
- Rejected rerouting `Absolute` through `Custom`: collides with the already-exposed
  `ELibremidiTimestampMode::Custom` value the wrapper exposes to callers.

`QueryPerformanceFrequency`/`QueryPerformanceCounter` (plain Win32 calls) were chosen over
`winrt::...::MidiClock::TimestampFrequency()` (numerically equivalent — `MidiClock` is itself
QPC-based at 100ns resolution) to avoid pulling in a first WinRT/COM dependency at this call site, a
possible `hresult_error` on activation if the WMS runtime is absent, RC4's documented November-2026
retirement making a fresh WinRT dependency the wrong time to add, and calling into a function-local
`static` initializer from inside the COM message callback (which has no `try`/`catch` around that
path). The frequency is queried once at `Initialize`, guarded against `0` (QueryPerformanceFrequency
can return 0, and the code defends against it rather than dividing by it).

### Consequences

- Good: Thal and any other consumer sees true ns regardless of backend, matching the public contract
  (`LibremidiMessage.h`, "Timestamp (nanoseconds)").
- Accept: this becomes a double-conversion risk if upstream's `winmidi` backend is ever fixed to
  return true ns itself (a plausible fix — not filed as of this writing). The first-message
  cross-check (`RunTimestampCrossCheckOnce`) is the safety net for that day, not a substitute for
  actively removing the correction once upstream lands it.
- Accept: a submodule SHA bump, or a fork edit that touches `winmidi`'s `midi_in.hpp`, must
  re-check `to_ns()`'s return value and delete this correction in the same change if upstream has
  since started returning true ns; this is not automatic, and nothing currently fails the build if
  it is missed — the cross-check's log line is the only runtime signal.
- Accept: Custom mode's tick correction assumes `Config.get_timestamp` (`LibremidiInput.cpp`, both the
  MIDI1 and MIDI2 configs) stays the identity transform; if that lambda is ever changed to actually
  transform the timestamp, the correction must move ahead of it, or Custom must be dropped from
  `NeedsWinmidiTickCorrection`.

### Confirmation

`LibremidiTimestampTest.cpp` (`Libremidi4UE.Timestamp.WinmidiTickConversion`) covers
`ConvertTicksToNs`/`NeedsWinmidiTickCorrection` as pure functions: a known-frequency case, the
overflow boundary the quotient/remainder split avoids, the `Freq == 0` passthrough, a negative
(`Relative`-mode) delta, `INT64_MIN` with no UB, and the full API/mode matrix for
`NeedsWinmidiTickCorrection`. At runtime, `RunTimestampCrossCheckOnce` logs an `Error` naming this
ADR if the converted timestamp and a fresh QPC-now conversion disagree by more than a few seconds —
the live signal that this correction has gone stale.
