---
description: >
  Checkpoint-A hardware verification: the fork's checkpoint-A pin (libremidi4ue-pin-20261003-a, 6a572eb) driven against an Ableton Push 3 over Windows MIDI Services with WinMM ground truth, probe label cpA. The (d), (g), (i) and (p) rows pass, the output restamp warns once per open, the receivable groups are 0-2, (c) is still defective by design; the fork's new WinRT-free unit tests pass under MSVC.
type: reference
status: point-in-time record (2026-10-03)
updated: 2026-10-03
---

# Checkpoint A on Windows 11 + Push 3: probe results (label `cpA`)

Date: 2026-10-03. Companion to the first [probe record](audit-winmidi-probe-2026-10-03.md), whose
method (§1), build recipe (§2) and pass criteria (§4 and the harness README) this run reuses
unchanged. Plan of record: [design-winmidi-ump-fixes.md](design-winmidi-ump-fixes.md), section
"Checkpoint A".

"cpA" = `6a572eb`, the head of the fork's `libremidi4ue` branch, tag
`libremidi4ue-pin-20261003-a`: upstream `master` `5c839a5` plus U1 (upstream pull request 264),
(q), U2, (p) and U3.

## 0. Results

| Row | Scenario(s) | Defective (pinned and head) | cpA | Verdict |
|---|---|---|---|---|
| (d) output group | `send-wms-user-g0`, `send-wms-user-bytes` | DI and palette replies on cable 1 (Live) | replies on cable 2 (User); `send-wms-user-g0` also under ASan, error log empty | **PASS** |
| (d) | `send-wms-ext-g0` | replies on cable 1 | no reply (cable 3 never answers) | **PASS** |
| (d) controls | `send-wms-live-g0`, `send-wms-user-g1` | cable 1, cable 2 | cable 1, cable 2 (unchanged) | **PASS** |
| (d) override (new) | `send-wms-live-g1` | cable 2 | cable 1 (caller's group 1 restamped to the Live port's group 0) | **PASS** |
| (g) batch | `recv-wms`, `recv-wms-ordered` (release) | exit `0xC0000409`; message path completes 0 SysEx | exit 0; message path = raw path on Live and User: 4 = 4 (`recv-wms`), 5 = 5 (`recv-wms-ordered`) | **PASS** |
| (g) | same, ASan | `stack-buffer-overflow` at `midi_stream_decoder.hpp:469` | identical counts; every `.probe.err.log` empty | **PASS** |
| (i) timestamps | `recv-wms` raw callback | pinned: ticks | delivered = QPC ticks x 100.0000 on all 90 raw callbacks; 0.1 to 1.5 ms before the callback's QPC-now | **PASS** (ns) |
| (p) MIDI 1 input SysEx | `recv-wms-midi1` (new), release and ASan | (p): `F0 01 F7` instead of the mode report | MIDI 1 input on Live and User: 5 of 5 SysEx whole (9 B mode reports, 17 B palette reply, 26 B DI reply); ASan clean | **PASS** |
| Restamp warning | `send-wms-user-g0`, `-ext-g0`, `-live-g1` | (no such warning) | exactly one per open: "a message's group is outside the output port's block; it is sent on the block's first group (reported once per open)", for 4 sent messages; none on the in-range sends | **PASS** |
| Receivable groups | `list` + the restamp sends | n/a | 0, 1, 2 (see §2) | **PASS** |
| (c) block direction | `list` | every Push port `INVERTED` | every Push port still `INVERTED`; inputs = GTB 4-6, outputs = GTB 1-3 | defective **by design** (deferred to the port-identity redesign) |

Unit tests (§3): the fork's two new WinRT-free test programs pass under MSVC.

**Bottom line:** at the checkpoint-A pin, sends to a Push 3 port land on that port's cable, SysEx
replies and mode reports arrive whole on the UMP path and on the MIDI 1 path, nothing overruns,
and timestamps are nanoseconds on the QPC timeline. The only defect left in the rows the probe
can exercise is (c), deferred on purpose.

## 1. Setup and runs

Same Windows 11 test machine, Push 3 and ground truth as the first record (§1.1 there). Compiler
MSVC 14.44.35207 (`_MSC_FULL_VER` 194435229), Windows SDK 10.0.26100.0. The `cpA` tree was staged
with `stage.sh` from the fork's tag and built `rel` (`/O2`) and `asan` (`/Od /fsanitize=address`).
Ableton Live was not running.

Two scenarios were added to the harness for this checkpoint:

| Scenario | What it does |
|---|---|
| `send-wms-live-g1` | WMS send to the Live port with the caller encoding group 1 (design U3: demonstrates the override) |
| `recv-wms-midi1` | `recv-wms-ordered`'s triggers, with a MIDI 1 `midi_in` opened next to the UMP one on every port (row (p)) |

Runs, in order, one session: `list`; the six `send-wms-*` scenarios (release); `recv-wms`,
`recv-wms-ordered`, `recv-wms-midi1` (release, then ASan); `send-wms-user-g0` (ASan); `restore`.
Every probe and ground-truth process exited 0. `restore` reported Dual mode on cables 1 and 2.

## 2. Evidence

**(d) output group.** Ground-truth summaries (Device Inquiry and palette-get replies identify
the landing cable; mode reports appear on cables 1 and 2 whatever cable the request used):

```
send-wms-user-g0     C2/User DI-REPLY x1, C2/User PAL-REPLY idx=0 x1          (1 restamp warning)
send-wms-user-bytes  C2/User DI-REPLY x1, C2/User PAL-REPLY idx=0 x1          (no warning: the MIDI 1.0
                                                                               converter stamps group 1)
send-wms-ext-g0      no DI or palette reply on any cable                      (1 restamp warning)
send-wms-live-g0     C1/Live DI-REPLY x1, C1/Live PAL-REPLY idx=0 x1          (no warning)
send-wms-user-g1     C2/User DI-REPLY x1, C2/User PAL-REPLY idx=0 x1          (no warning)
send-wms-live-g1     C1/Live DI-REPLY x1, C1/Live PAL-REPLY idx=0 x1          (1 restamp warning)
```

The probe prints the caller's words before the send (for example `30047E7F 06010000`, group 0,
on the User port); the restamp happens inside libremidi.

**Receivable groups.** U3 restamps only when a host-to-device block of the endpoint covers the
port's first group; the set of such groups is computed from the endpoint's blocks (function blocks
if declared, else group terminal blocks; device-viewpoint direction `BlockInput` or
`Bidirectional`). The Push 3 declares no function blocks; its host-to-device group terminal blocks
are GTB 4, 5 and 6 (`dir=BlockInput`, first groups 0, 1 and 2, one group each), so the receivable
groups are 0, 1 and 2. Behaviour agrees: the three out-of-range cases targeted groups 1 (User),
2 (External) and 0 (Live) and all took the restamp branch; the "sent unchanged" warning never
appeared.

**(g) batch.** `recv-wms` (release), summaries per port:

```
Live Port 4   RAW: batches=45 words=56 maxBatch=2 hist={1:34 2:11} umps=45 truncated=0 sysexComplete=4
Live Port 4   MSG: calls=11 sysexComplete=4 sysexOpenAtEnd=0 abandoned=0 orphans=0
User Port 5   RAW: batches=45 words=56 maxBatch=2 hist={1:34 2:11} umps=45 truncated=0 sysexComplete=4
User Port 5   MSG: calls=11 sysexComplete=4 sysexOpenAtEnd=0 abandoned=0 orphans=0
External 6    RAW: batches=0 ... MSG: calls=0
```

`on_raw_data` now fires once per delivered message, so the raw path's histogram has collapsed to
1- and 2-word spans (the first record saw 4-, 6- and 8-word batches); the count equality is the
criterion. A 26-byte Device Inquiry reply arrives as Start, Continue, Continue, End on the message
path, each with the batch's timestamp. ASan runs of `recv-wms`, `recv-wms-ordered` and
`recv-wms-midi1`: identical counts, empty error logs.

**(i) timestamps.** For every raw callback of `recv-wms` (90), `ts_lib / qpc_now` = 100.0000 and
`qpc_now x 100 - ts_lib` lies between 0.1 and 1.5 ms: libremidi delivers nanoseconds on the QPC
timeline.

**(p) MIDI 1 input.** `recv-wms-midi1` (release and ASan), MIDI 1 input on each Push port:

```
Live Port 4   M1: calls=5 sysex=5  (MODE-REPORT 1 9B, MODE-REPORT 2 9B, PAL-REPLY 17B, DI-REPLY 26B, MODE-REPORT 2 9B)
User Port 5   M1: calls=5 sysex=5  (same five)
```

**(c) block direction.** `list`: inputs "Live Port 4", "User Port 5", "External Port 6" are GTB
4-6 with `dir=BlockInput`; outputs "Live Port 1", "User Port 2", "External Port 3" are GTB 1-3
with `dir=BlockOutput`; the probe marks all six `INVERTED`, as before.

## 3. Fork unit tests under MSVC

The checkpoint-A tree was configured with libremidi's own CMake on the test machine: Visual
Studio 18's CMake and Ninja, MSVC 19.44.35229, Release, `LIBREMIDI_HEADER_ONLY=ON`,
`LIBREMIDI_TESTS=ON`, Catch2 v3.16.0 from a local copy (`FETCHCONTENT_SOURCE_DIR_CATCH2`,
`FETCHCONTENT_FULLY_DISCONNECTED=ON`; the test machine cannot fetch from GitHub
non-interactively). The Windows MIDI Services and UWP backends were disabled in CMake
(`LIBREMIDI_NO_WINMIDI`, `LIBREMIDI_NO_WINUWP`, no C++/WinRT download): the two new programs test
the WinRT-free helpers, which do not need the backend.

| Test program | Result |
|---|---|
| `winmidi_ump_batch_test` | passed: 8 test cases, 223 assertions |
| `winmidi_output_group_test` | passed: 5 test cases, 90 assertions |
| `conversion_test`, `midi_stream_decoder_test`, `rawio_test` (files the checkpoint changed) | passed |

## 4. State left behind

- Push 3 restored to Dual mode (`restore`).
- No probe or ground-truth process left running; Ableton Live not touched.
- Test machine: the `cpA` tree and its two builds, the logs of this run, and the CMake build of
  the unit tests, all outside this repository, next to the first record's material.
