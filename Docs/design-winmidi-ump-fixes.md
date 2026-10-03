---
description: Plan of record for the 2026-10 libremidi winmidi UMP fix batch, checkpoint A — baseline bump with the timestamp-correction removal, upstream PR #264, (q), per-message input dispatch (g, b), (p), interim output group stamp (d); fork stack order, test strategy, hardware verification, deferred units and their triggers. Read before executing, reviewing or bumping past any of these units
type: design
status: current
updated: 2026-10-03
---

# libremidi winmidi UMP fixes — checkpoint A

Decisions this plan executes: [ADR-0003](decisions/adr-0003-libremidi-fork-strategy.md) (fork
strategy), [ADR-0004](decisions/adr-0004-winmidi-timestamp-correction-removed.md) (timestamp
correction removed), [ADR-0005](decisions/adr-0005-winmidi-input-group-addressing.md) (input group
addressing). Evidence: [defect audit](audit-winmidi-defects-2026-10-03.md) (ids (a)–(ac)),
[hardware probe](audit-winmidi-probe-2026-10-03.md), [upstream sweep](audit-winmidi-upstream-2026-10-03.md).
Open items and their live state: [backlog.md](backlog.md).

Path conventions: `W/` = `include/libremidi/backends/winmidi/`, `L/` = `include/libremidi/` in
libremidi. Line numbers are at upstream `5c839a5` unless marked "pin" (`67e8ccd`).

## 1. Scope

Checkpoint A fixes what makes Windows MIDI Services unusable for a UMP consumer today: sends that
land on the wrong cable, SysEx and bursts that never complete, and a process kill on ordinary
receive traffic (§6). It consists of:

| Unit | Ids | What |
|---|---|---|
| U0 | (i) | Baseline: submodule onto the fork at upstream `master`, ADR-0002 correction removed; then the checkpoint-A pin |
| U1 | (r) | Carry upstream PR #264 (reserved UMP sizes, truncated-tail guards) |
| (q) | (q) | Bound cmidi2's UMP → MIDI 1 SysEx stack buffer |
| U2 | (g), (b) | Per-message input dispatch with a per-message group verdict over the block's range |
| (p) | (p) | Reassemble SysEx7 across packets for MIDI 1 inputs on winmidi |
| U3 | (d) | Stamp the output group (interim rule) |

Out of this checkpoint: units U4–U11 and the port-identity redesign, each a backlog row with a
trigger (§7); the in-box API migration; everything outside the UMP-only scope (ADR-0003 rule 8).

State at planning time (2026-10-03): upstream `master` is `5c839a5`; PR #264 is open as a single
commit `f139d62` touching `L/cmidi2.hpp`, `L/detail/midi_stream_decoder.hpp`,
`L/detail/ump_stream.hpp` and `tests/unit/midi_stream_decoder.cpp`; #234 is open; the fork's
`master` carries no delta; the pin `67e8ccd` is 36 commits behind upstream.

## 2. Fork mechanics and stack order

Normative rules are in ADR-0003. In short: fork `master` is fast-forwarded to upstream `master`
and never committed to; each unit is a topic branch; the integration branch `libremidi4ue` is
rebuilt from the topics in the order below; every pinned SHA gets a `libremidi4ue-pin-*` tag
pushed before the Libremidi4UE commit that pins it. The fork work happens in a clone separate from
the submodule checkout. GitHub Actions stay disabled on the fork: upstream's workflows target
self-hosted runners.

This table is the recorded stack order for checkpoint A:

| # | Unit | Topic branch | Based on | Upstream disposition |
|---|---|---|---|---|
| 1 | U1 | `fix/ump-reserved-sizes` | upstream `master` | not ours: follow #264, drop when merged |
| 2 | (q) | `fix/cmidi2-sysex7-bounds` | upstream `master` | PR candidate (leaf) |
| 3 | U2 | `fix/winmidi-input-dispatch` | U1 (sizes) | PR candidate (#234) |
| 4 | (p) | `fix/midi1-input-sysex-reassembly` | upstream `master` | PR candidate (leaf) |
| 5 | U3 | `fix/winmidi-output-group` | U2 (shared helpers) | fork-only; the `converter.context.group` line is a leaf PR candidate |

Within a topic, the fix commits come first and the tests commit last, so every commit on the
integration branch builds and passes. (p) sits right after U2 so that U2 never degrades MIDI 1
inputs from "nothing" to "corrupt" at any point of the stack; (q) sits right after U1.

## 3. Units

### U0: baseline (two Libremidi4UE commits)

Fork side: fast-forward `master` to upstream `master` (at least `5c839a5`); create
`libremidi4ue` at that commit; tag it.

Libremidi4UE commit 1, "bump":
- `.gitmodules` names the fork, without a `branch =` line; every clone runs
  `git submodule sync --recursive` once.
- The pin moves to upstream `master` as mirrored on the fork, tagged.
- The ADR-0002 correction is removed in this same commit, and the generic WMS timestamp-domain
  cross-check is kept (ADR-0004). Splitting the two would ship timestamps 100× too large.
- README: replace "wraps libremidi (v5.4.3)" (already wrong; the pin is `v5.4.2-62`) with the fork
  wording: upstream `master` plus carried fixes, ADR-0003.
- Backlog row (i) flips to `done`.

Libremidi4UE commit 2, "fix pin": the pin moves to the checkpoint-A head of `libremidi4ue`,
tagged. The checkpoint-A rows flip to `done`.

What arrives with the bump (`67e8ccd..5c839a5`, 36 commits):

| Area | Commits | Effect on Libremidi4UE |
|---|---|---|
| winmidi | `5c839a5` (i), `968b5fa` (observer transport filter; brings (f)), `c5f20ea` (no `NOMINMAX` / `WIN32_LEAN_AND_MEAN` redefinition) | (i) forces the correction removal. (f) is invisible with Libremidi4UE's observer flags (hardware, virtual and network all admitted). `c5f20ea` changes Windows include order; only a compile shows its effect. |
| Observer filter, all backends | `968b5fa`; `a9fa70b` and its revert `4d39cfc` (net: `track_network` on by default) | CoreMIDI now types the IAC bus as software and network sessions as network; both are still admitted. Verify the Mac port lists are unchanged. |
| WinMM | `49d3b8a` (input teardown use-after-free fix), `4326a47` (Start/Continue/Stop/Tune Request/Reset no longer dropped), `7b66533` (config shadowing) | Out of scope, but it arrives and improves WinMM users. |
| Other backends, CI | ALSA, PipeWire, JACK, WebMIDI, network, KDMAPI, WinUWP; Catch2 3.15.3 | Not compiled by Libremidi4UE on its shipping platforms. |

Risks:
- The winmidi backend was not compiled for the July bump (ADR-0001's verification note), so this
  is its first Windows compile since then: a Win64 Editor and a Win64 Game build are mandatory.
- Rebasing the topics over later upstream syncs: a unit whose upstream equivalent has landed
  drops out (ADR-0003 rule 3); record that in its backlog row.

Verification: Mac editor build; Libremidi4UE automation tests; CoreMIDI port lists compared with a
pre-bump dump. Windows: Win64 Editor and Win64 Game builds; probe label `base`: the timestamp row
reads QPC-now × 100 (nanoseconds); the (c), (d) and (g) rows are still defective, identical to
upstream `master`. No UE-level run on Windows at this pin: §6 applies until U1.

### U1: carry PR #264 (defect r)

- **Purpose:** remove the out-of-bounds paths — reserved UMP sizes, truncated tails, the unbounded
  copy into `ump::data[4]`. This alone removes (g)'s process kill.
- **Files:** `L/cmidi2.hpp:350-366` (sizes for MT 0x6–0xC and 0xE);
  `L/detail/midi_stream_decoder.hpp:405-409` (tail guard) and `468-471` (copy clamp);
  `L/detail/ump_stream.hpp:35-55` (segmenter tail guard); `tests/unit/midi_stream_decoder.cpp`.
- **Approach:** `git cherry-pick -x f139d62`, unchanged and keeping its original authorship, so
  that once upstream merges it our copy is patch-equivalent and drops out on rebase. If upstream
  merges a variant, take upstream's and re-run U1's checks.
- **Tests:** the PR's own. In our own tests commit, add the audit's hardware-free probes `264` (a
  reserved MT through `on_bytes_multi`) and `p` (a 1-word MT4 tail through `segment_ump_stream`)
  if the PR does not cover them, and an 8-word SysEx7 span through `on_bytes` that is clean under
  ASan (it asserts safety only; delivery is U2's job).
- **Probe:** the crash half of the (g) rows. `recv-wms` and `recv-wms-ordered` (release): exit 0,
  no input stall; ASan builds: an empty error log. SysEx on the message path is still expected to
  stay incomplete.
- **Size and risk:** about 25 carried lines plus tests; low. It is shared code, so CoreMIDI UMP on
  Mac runs through it too: run the Mac tests.

### (q): bound the cmidi2 UMP → MIDI 1 SysEx buffer

- **Purpose:** a UMP send of more than 1 KB of SysEx on a MIDI 1 backend (the WinMM fallback, for
  instance) overruns a stack buffer.
- **Files:** `L/cmidi2.hpp:3195` (`uint8_t sysex7_buffer[1024]`), appended without bounds at
  `3146-3153` and copied into `dst` without a `dLen` check at `3230-3241`; reached from
  `midi1::out_api::send_ump` (`L/detail/midi_out.hpp:51-57`).
- **Approach:** bound `sysex7_buffer_index + n` against the buffer and return `OUT_OF_SPACE`, or
  write the bytes straight into `dst` under a `dLen` check.
- **Tests:** ASan unit tests at 500 bytes (passes today), at the 1024-byte boundary, and at 1100
  bytes (the audit's probe `q1100`, an overflow today).
- **Size and risk:** 8–15 lines; low; no dependency.

### U2: per-message input dispatch (defects g, b)

- **Purpose:** deliver every UMP of a COM receive batch, give each its own group verdict over the
  opened block's range, and apply the groupless policy (ADR-0005). This fixes "0 SysEx messages
  complete" (#234).
- **Files:**
  - `W/midi_in.hpp`: the COM `process_message` (`240-252`) and the WinRT `process_message`
    (`207-213`); `open_port` (`112`) stores the block's first group and group count instead of a
    single filter value; `close_port` resets them.
  - WinRT-free helpers in `L/detail/ump_stream.hpp` (which already holds the output segmenter) or
    a new `L/detail/ump_batch.hpp`: `ump_has_group(w0)` (MT 0x1, 0x2, 0x3, 0x4, 0x5, 0xD);
    `ump_group_in_range(w0, first, count)`; and the batch dispatcher that walks a span by U1's
    sizes and delivers each accepted UMP.
- **Commit series:**
  1. The helpers and the dispatcher.
  2. COM path: the callback body becomes a call into the dispatcher, nothing else. Each accepted
     UMP is delivered as `on_bytes({ump + i, ump + i + n}, ts)`, so the decoder sees exactly one
     message per call. The service timestamp is converted once per batch. The walk stops at a
     truncated tail and when the size function yields 0 (the `n == 0` guard).
  3. The groupless policy on both paths, as its own commit so an upstream PR can leave it out:
     on a group-filtered port, drop MT 0x0, MT 0xF and the reserved types; on an unfiltered
     (virtual) port, deliver everything.
  4. (b): the verdict becomes `first <= g < first + count`.
  5. Tests.
- **Behaviour changes:** `on_raw_data` fires per delivered message, not once per batch (required
  so raw consumers see filtered data; flag it in any PR); in `Relative` timestamp mode, messages
  after the first in a batch get a zero delta.
- **Tests (WinRT-free, run on the dispatcher the callback calls):**
  - SysEx7 batches of 4, 6, 8 and 10 words yield 2, 3, 4 and 5 packets, in order (the audit's
    probes `g4`/`g6`/`g8`, plus a 10-word case, the size of a 29-byte SysEx in one batch);
  - 8 MT2 words yield 8 messages;
  - a mixed-group batch delivers only the in-range messages;
  - MT 0xF, MT 0x0 and a reserved type inside a filtered batch are dropped, inside an unfiltered
    batch delivered;
  - a truncated tail is dropped; NOOP padding is handled;
  - a multi-group range (first 2, count 3) works;
  - `Relative`-mode deltas inside a batch;
  - clean under ASan and UBSan.
- **Probe:** the (g) rows in full. `recv-wms` and `recv-wms-ordered` (release): exit 0, and on each
  port the message path's completed SysEx count equals the raw path's (4 on the Live and User
  ports); ASan: an empty error log. The raw path's batch histogram collapses to per-message spans;
  the count equality is the criterion. (b) cannot be exercised on the Push 3 (group count 1 per
  block) and is covered by the unit tests. Optional: a `loop-batch` scenario (8 CCs sent in one
  call over the service's app loopback arrive as 8 messages) to cover bursts without a person at
  the device.
- **Size and risk:** about 30–40 lines plus about 120 test lines; low.
- **Upstream:** PR candidate for #234. If upstream lands a one-line `on_bytes` → `on_bytes_multi`
  fix first, rebase and keep only the per-message verdict, the groupless policy and (b).
- **Consequences:** device mode reports and other multi-packet SysEx complete; 10-word SysEx
  batches arrive; MT2 bursts (encoders, MPE) arrive complete. A UMP consumer that reassembles
  SysEx7 per packet works unchanged. No identity change. Libremidi4UE `Midi1` inputs on Windows
  MIDI Services now receive every packet and would hit (p) — hence (p) next.

### (p): SysEx7 reassembly for MIDI 1 inputs on winmidi

- **Purpose:** a MIDI 1 input on Windows MIDI Services delivers a multi-packet SysEx as a corrupt
  `F0 <last packet> F7` once U2 delivers every packet (today it delivers nothing).
- **Files:** `L/midi_in.cpp:13-33` (`convert_midi1_to_midi2_input_configuration`), which converts
  one UMP at a time while cmidi2 keeps its SysEx state local to one call (`L/cmidi2.hpp:3195`,
  `3247`).
- **Approach:** a persistent byte buffer per input in the lambda; append the SysEx7 payload for
  status 0 (complete), 1 (start), 2 (continue) and 3 (end); emit `F0 … F7` on complete or end;
  reset on an unexpected start; cap the size (for example 1 MiB); respect `ignore_sysex`; pass
  other message types through the existing converter unchanged.
- **Tests:** through `rawio_ump` and the input configuration (the audit's probes `m1`/`m2`): the
  9-byte Push 3 mode report `F0 00 21 1D 01 01 0A 01 F7` arrives whole; a reset on an unexpected
  start; the size cap.
- **Size and risk:** 30–45 lines; low. Depends on U2 for winmidi to deliver every packet.

### U3: stamp the output group (defect d), interim

- **Purpose:** a send to port *n* of a multi-cable device lands on cable *n*. Windows MIDI
  Services has no destination parameter; the group lives in the message.
- **Files:** `W/midi_out.hpp`: `open_port` (`39-65`) keeps the resolved block, stores its first
  group and group count, and sets `this->converter.context.group` to the first group (MIDI 1 bytes
  reach `send_ump` through the converter, `L/detail/midi_out.hpp:72-80`, so both APIs are
  covered); `send_ump` (`174-195`) re-stamps inside the segment lambda; `close_port` resets the
  stored group. A helper `ump_restamp_group(...)` lives next to U2's helpers.
- **Rule (interim, ADR-0005 output note):**
  - range membership: a group-bearing UMP (MT 0x1, 0x2, 0x3, 0x4, 0x5, 0xD) whose group lies
    outside the opened block's range `[first, first + count)` is re-stamped (bits 27..24) to
    `first`; a group inside the range is kept. On a single-group block, which every MIDI 1.0
    cable is, every message carries the port's group;
  - MT 0x0, MT 0xF and reserved types are never touched; virtual ports (no block) are untouched;
  - guard: while block direction (c) is still inverted, the block that `open_port` resolves for an
    output can be the device-to-host block. Stamp only if a host-to-device block of the endpoint
    (device-viewpoint `BlockInput`, or bidirectional) covers the group; otherwise send the message
    unchanged and raise a warning once. On the Push 3 the guard passes: KSA numbers source and
    destination groups independently from 0, so the wrong-direction block carries the right group.
- **Tests:** the re-stamp helper (MT 0x1/0x2/0x3/0x4/0x5/0xD outside the range re-stamped, inside
  kept; MT 0x0/0xF and reserved untouched); `midi1_to_midi2` with `context.group = 1` turns
  `91 3C 40` into group nibble 1 (0 today); the guard predicate as a pure function over a list of
  blocks (direction, first group, count).
- **Probe:** the (d) rows. `send-wms-user-g0` and `send-wms-user-bytes`: the Device Inquiry and
  palette replies arrive on cable 2 (User); `send-wms-ext-g0`: no reply. Unchanged controls:
  `send-wms-live-g0` lands on cable 1, `send-wms-user-g1` on cable 2. New `send-wms-live-g1`: lands
  on cable 1 (Live), demonstrating the override; today it lands on cable 2.
- **Size and risk:** 20–30 lines plus the guard and tests; low for single-group ports. Behaviour
  change: a caller that encoded an out-of-range group on a single-group port is overridden (today
  that message reaches another cable).
- **Upstream:** fork-only. The re-stamp is a port-model policy, and Microsoft's maintainer-facing
  advice on #254 warns against re-addressing UMPs in general; it is settled together with "port =
  endpoint + group + direction" in the port-identity redesign, which replaces this rule.

### Checkpoint A

Libremidi4UE commit 2 of U0 pins the checkpoint-A head. Probe label `cpA`: the (d), (g) and
timestamp rows pass; the (c) row is still defective by design. A UE-level run on the Windows test
machine with a UMP consumer bound to the Push 3 User port: the device's mode reports arrive and
LED control in User mode works. The design doc goes `implemented` when checkpoint A is pinned and
verified.

## 4. Test strategy

- **Where:** Catch2 in the fork, through libremidi's own CMake (`-DLIBREMIDI_TESTS=ON`,
  `tests/unit/*`). They run on macOS (clang, `-fsanitize=address,undefined`) and on Windows (MSVC).
- **WinRT-free seams that the backend really calls.** The winmidi headers compile only on Windows
  with the WinRT projection, so testable logic lives in WinRT-free headers, and the backend calls
  that logic directly: the COM callback body is a call into the dispatcher and nothing else. The
  tests therefore exercise the code the callback runs, not a copy of it.
- **Mutation check.** For every unit, the tests commit applied to the unit's base without the fix
  must fail. For U2, in addition, reverting each piece on its own — the walk back to the
  whole-batch call, the range compare back to `==`, the groupless drop — must turn at least one
  test red. Record the result with the unit (commit message or checkpoint record).
- **Tests in their own commit** (ADR-0003 rule 5), so they survive a drop-and-follow and serve as
  the equivalence check against upstream's code (rule 3).
- **Libremidi4UE side:** the plugin's automation tests on the Mac editor, with the timestamp tests
  as amended by ADR-0004.
- **Build matrix:** Mac editor; Win64 Editor; Win64 Game (packaged flags), because editor and game
  targets compile with different flags (§5).

## 5. Verification

- **Probe harness.** A standalone Windows console program that builds libremidi exactly the way
  Libremidi4UE does (header-only, WinMM + winmidi + MIDI 2, the bundled rc-4 projection ahead of
  the Windows SDK's, the COM extension header on the include path, so the COM receive path is
  compiled in), drives a Push 3 over Windows MIDI Services, and uses WinMM as ground truth. Release
  and ASan builds; staging, build and run scripts; logs merged on one QPC timeline. Scenarios and
  pass criteria: [probe record](audit-winmidi-probe-2026-10-03.md) §1.3 and §4. It is kept outside
  this repository today and is to be versioned, scrubbed, under `Tools/WinmidiProbe` in a later
  phase (backlog row). Labels used by this plan: `base` (U0), `cpA` (checkpoint A).
- **Rows per unit:** timestamp row (U0), (g) crash half (U1), (g) full (U2), (d) rows plus
  `send-wms-live-g1` (U3), (c) row expected still defective.
- **Win64 Game build check.** Built in addition to the editor target at U0 and at checkpoint A.
- **Exceptions-flag spike** (backlog row). As far as known, UE enables C++ exceptions for every
  module in editor targets but, in game targets, only for modules that opt in. libremidi is
  header-only and compiles inside Libremidi4UE's translation units, and the winmidi backend
  relies on catching `winrt::hresult_error` and other exceptions. The spike builds Win64 Game,
  reads the effective `/EH` flag for those units, and forces one WinRT failure path to see whether
  it is caught. The outcome decides whether Libremidi4UE's module rules opt in to exceptions. An
  access violation is not a C++ exception (`catch (...)` catches it only under `/EHa`), and the
  backend must not rely on that.
- **Physical input** needs a person at the devices: CC and encoder bursts, and a device that sends
  a 29-byte SysEx stream per touch (whether Windows MIDI Services delivers it as one 10-word batch
  is unverified). These go on the manual verification list for checkpoint A.

## 6. Crash exposure before checkpoint A

On the current pin, on Windows with the Windows MIDI Services API: any receive callback batch
longer than six words makes the decoder copy the whole batch into a 24-byte `libremidi::ump`
(pin `L/detail/midi_stream_decoder.hpp:469`). In the probe's `/O2 /GS` build the first such batch
stalled all input on the endpoint for 2.3–7.6 s and the next one ended the process with
`0xC0000409`. A `/GS` failure is a fast-fail, so UE's crash reporter does not run; in production it
looks like the process vanishing. Verified with the Push 3 (its 8-word Device Inquiry reply);
plausible for any device that sends multi-packet SysEx or MIDI 2.0 bursts in one batch.

Libremidi4UE is on that path for every MIDI 2 input: it sets `on_message` unconditionally
(`LibremidiInput.cpp`), so the copy runs whether or not a UE consumer is bound, and `Midi1` inputs
on Windows MIDI Services are wrapped onto the same decoder (`L/midi_in.cpp:13-33`). An input that
does not ignore SysEx lets a SysEx-first batch reach the copy. With
`midi1_channel_events_to_midi2` set, an MT2-first batch takes the one-word upgrade branch instead
(silent loss of the rest of the batch, no overflow). U1 removes the overflow and U2 the loss,
which is why they head the stack.

## 7. Deferred units and their triggers

The live state of each item is its [backlog](backlog.md) row; this table is the plan-time
snapshot. "In-box port" means: fold into the in-box API migration, where the code is rewritten
anyway.

| Unit | Ids | Trigger |
|---|---|---|
| U4 open/close results | (k), (ac) | In-box port ((k)'s callback-registration semantics change there); earlier on an observed silent dead port or an access violation on send or reopen |
| U5 service-down safety | (w) | In-box port (bootstrap becomes `MidiApi::EnsureServiceAvailable()`); earlier on an observed crash when opening while the MIDI service is stopped, restarting or in Legacy mode |
| U6 observer filter, transport codes, null guard | (f), (x), (s) | In-box port (new transport codes, watcher arguments carry the device); earlier on an observed watcher-thread crash or a wrong port type that a filter depends on |
| U7 COM receive firewall and teardown guard | (v) | In-box port (callback locking differs: deadlock instead of race); earlier on an observed crash or terminate at close or shutdown while a device streams |
| U8 observer lifetime and initial snapshot | (t), (u) | In-box port (watcher event arguments change); earlier on an observed crash at observer teardown or duplicate port-added events a consumer cannot absorb |
| U9 send robustness and batching | (y) | Observed send failures under load or a measurable cost of large SysEx sends; otherwise the in-box port |
| U10 MIDI 1 → MIDI 2 input upgrade semantics | (aa) | A maintainer conversation, which the owner handles (shared semantics on every backend); earlier on an observed held note in a consumer applying MIDI 2.0 semantics |
| U11 monotonic flag | (ab) | A consumer that needs service timestamps in `SystemMonotonic` mode on Windows MIDI Services |
| Port-identity redesign | (a), (c), (e), `Ordinal`, ADR-0005 output half | In-box ship (late November 2026); date fallback in the backlog row (ADR-0003 rule 2) |
| One connection per endpoint, one session | (h) | With or after the port-identity redesign and the in-box port |
| Availability probe | (z) | In-box port |
