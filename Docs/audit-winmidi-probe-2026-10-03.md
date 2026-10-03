---
description: >
  Hardware evidence for the winmidi defect audit: libremidi (pinned 67e8ccd and upstream HEAD 5c839a5) driven against an Ableton Push 3 over Windows MIDI Services on Windows 11, with WinMM ground truth. Confirms block direction (c), output group (d), batch/SysEx loss and stack overrun (g), WinMM wrap (j), timestamps (i). Read for the exact reproduction method and pass criteria for a fixed libremidi.
type: reference
status: point-in-time record (2026-10-03)
updated: 2026-10-03
---

# libremidi winmidi on Windows 11 + Push 3: hardware probe evidence

Date: 2026-10-03. Companion to the [defect audit](audit-winmidi-defects-2026-10-03.md); this
document is the hardware evidence for it. No repository was modified and nothing was
pushed. Work happened only in a scratch probe directory on a Windows 11 test machine and
on a macOS development machine.

The probe sources, the harness README (rebuild/run steps, pass criteria for a fixed libremidi)
and all merged logs and ASan reports are kept outside this repository. §2 gives the build
recipe and §4 the pass criteria. Harness script names mentioned below (`run.ps1`, `build.bat`,
`stage.sh`) refer to that external harness.

---

## 0. Results

"pinned" = `67e8ccd` (Libremidi4UE submodule pin). "head" = `5c839a5` (upstream `master`, 2026-09-28).

| Defect | pinned | head | Decisive evidence |
|---|---|---|---|
| (c) block direction inverted | **CONFIRMED** | **CONFIRMED** | All six Push ports, and the UR44C, listed against the block's own direction (§3.1). Runtime-masked on Push by symmetric group numbering. |
| (d) output group ignored | **CONFIRMED** | **CONFIRMED** | Sends to "User Port 2" and "External Port 3" land on cable 1; caller-encoded group 1 lands on cable 2 (§3.2). Same through the MIDI 1.0 byte API. |
| (g) batch treated as one message: SysEx loss | **CONFIRMED** | **CONFIRMED** | Per-message path completes 0 of 8 SysEx replies; raw path and WinMM ground truth get 8 of 8 (§3.3). |
| (g) stack overrun for batches > 6 words | **CONFIRMED** | **CONFIRMED** | ASan `stack-buffer-overflow`, `WRITE of size 32`, `midi_stream_decoder.hpp:469`, on the WMS callback thread. Release build: input stalls, then process dies `0xC0000409` (§3.3). |
| (k) `Open()` result ignored | NOT REPRODUCED | NOT REPRODUCED | `Open()` returned `true` in every case tried, including on a connection after `DisconnectEndpointConnection` (§3.4). Code-level defect stands; no runtime trigger found. |
| (j) WinMM UMP wrap drops SysEx | **CONFIRMED** | **CONFIRMED** | UMP-API input on WinMM delivers only the Start packet: 0 of 8 complete, MIDI-1 input on the same port 8 of 8 (§3.5). |
| (i) timestamps ticks vs ns | **CONFIRMED** | FIXED | pinned `ts_lib` equals QPC ticks; head equals ticks x 100 (§3.6). |
| New: input stall after an overrun | OBSERVED | OBSERVED | Release build: after the first 8-word batch, no callback on any Push port for 2.3 to 7.6 s, then `0xC0000409` (§3.3). |
| New: duplicate GTB numbers on loopback endpoints | OBSERVED | OBSERVED | Two GTBs `Number()=1` (one per direction); `get_port` resolves the first (§3.7). |
| WinMM input-close use-after-free (fixed upstream `49d3b8a`) | NOT REPRODUCED | (clean) | ASan runs with WinMM inputs open and closed: no report (one run each). |
| (g) dense CC bursts | INCONCLUSIVE | INCONCLUSIVE | Needs physical input; nobody at the device. |
| (a), (b), (e), (f), (h) | not exercised | not exercised | KSA Push has no FBs and GroupCount 1; no hotplug possible remotely. |

**Bottom line for the Push 3 User Port over WMS:** both symptoms observed in the downstream UE consumer are
reproduced outside UE with libremidi alone, on the pin and on upstream `master`. Sends land
on the Live cable because of (d). SysEx replies never complete because of (g). The overrun
part of (g) is more severe than "writes past a stack object": in an `/O2 /GS` build it stops
all input delivery for the endpoint and later terminates the process.

---

## 1. Method

### 1.1 Setup

| Item | Value |
|---|---|
| Machine | Windows 11 Pro build 26300 test machine, driven remotely |
| WMS | `midisrv` running; SDK runtime `1.0.17-rc.4.25` (probe reads it via `IMidiClientInitializer`) |
| Push 3 | KSA endpoint `<endpoint device id>`, VID 2982 PID 1969 |
| Compiler | MSVC 14.44.35207 (`_MSC_FULL_VER` 194435229) from VS 18 Community; Windows SDK 10.0.26100.0 |
| Ground truth | python-rtmidi 1.5.8 (WinMM), in a scratch virtualenv |
| Ableton Live | not running (checked before every scenario; `run.ps1` refuses otherwise) |
| The downstream application's Push host process | not running, not touched |

Both the probe and the ground truth stamp every log line with `QueryPerformanceCounter`
seconds, so `run.ps1` merges them into one timeline.

### 1.2 How a send's landing cable is identified

Calibration through WinMM only (`calibrate`):

```
GT SUMMARY sent di to C1/Live -> C1/Live:DI-REPLY(+0.5ms)
GT SUMMARY sent di to C2/User -> C2/User:DI-REPLY(+0.4ms)
GT SUMMARY sent di to C3/External -> NONE
GT SUMMARY sent pal0 to C1/Live -> C1/Live:PAL-REPLY idx=0(+0.3ms)
GT SUMMARY sent pal0 to C2/User -> C2/User:PAL-REPLY idx=0(+0.5ms)
GT SUMMARY sent pal0 to C3/External -> NONE
GT SUMMARY sent mode1 to C2/User -> C2/User:MODE-REPORT 1(+0.4ms); C1/Live:MODE-REPORT 1(+0.4ms)
GT SUMMARY sent mode2 to C3/External -> NONE
GT SUMMARY sent mode2 to C1/Live -> C1/Live:MODE-REPORT 2(+0.3ms); C2/User:MODE-REPORT 2(+0.4ms)
```

Device Inquiry and palette get are answered only on the cable they arrived on; cable 3
never answers; mode reports appear on cables 1 and 2. So the cable of a DI or palette
reply is the cable the request landed on.

Reply sizes over WMS (raw callback): mode report 9 B = 4 words, palette reply 17 B = 6
words, DI reply 26 B = 8 words. The Push also sends Active Sensing (`FE`) on cables 1
and 2 every 270 ms, as 1-word batches.

### 1.3 Scenarios

Each scenario ran on `pinned` and `head`; release (`/O2`) and ASan (`/Od /fsanitize=address`)
builds where relevant. Names as in `run.ps1`.

| Scenario | Purpose |
|---|---|
| `list`, `list-all` | (c): libremidi port lists vs SDK block direction and groups |
| `send-wms-user-g0`, `-ext-g0`, `-live-g0`, `-user-bytes`, `-user-g1` | (d): landing cable of WMS sends |
| `send-winmm-user` | control: libremidi WinMM output via the UMP API |
| `recv-wms`, `recv-wms-ordered`, `recv-wms-rawonly` | (g), (i): WMS inputs on all three ports with both callbacks / raw only |
| `recv-winmm-ump` | (j): WinMM input through the UMP API, plus a MIDI-1 input for contrast |
| `open-check` | (k) |
| `calibrate`, `restore` | ground truth only |

---

## 2. Build recipe

Same model as Libremidi4UE (`libremidi.Build.cs` + `WindowsMidiServices.Build.cs`): header-only
libremidi with `LIBREMIDI_WINMM`, `LIBREMIDI_WINMIDI`, `LIBREMIDI_ENABLE_MIDI2`; Libremidi4UE's
trimmed rc-4 C++/WinRT projection copied to `third_party\WindowsMidiServices` and placed ahead of
the Windows SDK `cppwinrt` directory; its `winmidi` directory on the include path, so
`WindowsMidiServicesAppSdkComExtensions.h` is found and the COM raw-callback path is compiled in,
exactly as in UE. The SDK runtime is located the same way libremidi does it in UE:
`CoCreateInstance` of the installed `MidiClientInitializer`, then `EnsureServiceAvailable`.

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
cl /nologo /std:c++20 /EHsc /permissive- /Zc:__cplusplus /utf-8 /bigobj /W3 /MD /Zi /FS ^
   /O2   (or: /Od /fsanitize=address) ^
   /DLIBREMIDI_HEADER_ONLY /DLIBREMIDI_WINMM /DLIBREMIDI_WINMIDI /DLIBREMIDI_ENABLE_MIDI2 ^
   /I libremidi\<label>\include ^
   /I third_party\WindowsMidiServices\Win64\include ^
   /I third_party\WindowsMidiServices\Win64\include\winmidi ^
   src\libremidi_probe.cpp /link /INCREMENTAL:NO winmm.lib WindowsApp.lib ole32.lib
```

`build.bat <label> rel|asan` wraps this and copies the ASan runtime DLL next to the exe.
Sources came from a fresh clone of `celtera/libremidi` on the macOS development machine,
exported with `git archive` for both commits and copied to the test machine as a tarball
(`stage.sh`). All four builds compile; the
only warnings are `/W3` ones inside libremidi's own headers (C4267/C4244/C4101 conversions in
`cmidi2.hpp` and friends, one C4005 `WIN32_LEAN_AND_MEAN` redefinition), none in the probe.

---

## 3. Evidence per defect

### 3.1 (c) Block direction inverted: CONFIRMED (pinned, head)

The probe looks up each libremidi port's block through the SDK and checks Microsoft's
block-viewpoint direction. Head, identical on pinned:

```
LM-WMS IN  display='Ableton Push 3 MIDI Live Port 4' ... port(block#)=4
LM-WMS IN    -> GTB#4 'Ableton Push 3 MIDI Live Port' dir=BlockInput firstGroup=0 groupCount=1 => listed as app INPUT: INVERTED (block RECEIVES from host; app cannot receive from it)
LM-WMS IN    -> GTB#5 'Ableton Push 3 MIDI User Port' dir=BlockInput firstGroup=1 groupCount=1 => listed as app INPUT: INVERTED ...
LM-WMS IN    -> GTB#6 'External Port' dir=BlockInput firstGroup=2 groupCount=1 => listed as app INPUT: INVERTED ...
LM-WMS OUT   -> GTB#1 'Ableton Push 3 MIDI Live Port' dir=BlockOutput firstGroup=0 groupCount=1 => listed as app OUTPUT: INVERTED (block SENDS to host; app cannot send to it)
LM-WMS OUT   -> GTB#2 'Ableton Push 3 MIDI User Port' dir=BlockOutput firstGroup=1 groupCount=1 => listed as app OUTPUT: INVERTED ...
LM-WMS OUT   -> GTB#3 'External Port' dir=BlockOutput firstGroup=2 groupCount=1 => listed as app OUTPUT: INVERTED ...
LM-WMS IN    -> GTB#2 'Steinberg UR44C-1' dir=BlockInput ... => listed as app INPUT: INVERTED ...
LM-WMS OUT   -> GTB#1 'Steinberg UR44C-1' dir=BlockOutput ... => listed as app OUTPUT: INVERTED ...
```

The SDK's own MIDI 1.0 port association ties each group to the WinMM ports by flow:

```
SDK-EP   group 0: WinMM source(app input)='Ableton Push 3 MIDI (#5)' destination(app output)='Ableton Push 3 MIDI (#6)'
SDK-EP   group 1: WinMM source(app input)='MIDIIN2 (Ableton Push 3 MIDI) (#6)' destination(app output)='MIDIOUT2 (Ableton Push 3 MIDI) (#7)'
SDK-EP   group 2: WinMM source(app input)='MIDIIN3 (Ableton Push 3 MIDI) (#7)' destination(app output)='MIDIOUT3 (Ableton Push 3 MIDI) (#8)'
```

At runtime the inversion is masked on the Push: input "User Port 5" (GTB 5, group 1) filters
on group 1, and cable 2's source also emits group 1, so the right data arrives (§3.3 raw logs:
User Port 5 receives the `g1` replies sent to cable 2). An asymmetric device would break.

### 3.2 (d) Output group ignored: CONFIRMED (pinned, head)

Merged probe + ground-truth timelines, landing cable from the DI and palette replies:

| Scenario (what was opened / encoded) | pinned | head |
|---|---|---|
| `send-wms-user-g0`: "User Port 2", UMP group 0 (the downstream consumer's encoding) | DI, PAL -> **C1/Live** | DI, PAL -> **C1/Live** |
| `send-wms-user-bytes`: "User Port 2", MIDI 1.0 bytes `send_message` | -> **C1/Live** | -> **C1/Live** |
| `send-wms-ext-g0`: "External Port 3", group 0 | -> **C1/Live** (should be NONE) | -> **C1/Live** |
| `send-wms-live-g0`: "Live Port 1", group 0 | -> C1/Live (correct) | -> C1/Live |
| `send-wms-user-g1`: "User Port 2", caller encodes group 1 | -> C2/User | -> C2/User |
| `send-winmm-user`: WinMM `MIDIOUT2`, UMP API | -> C2/User | -> C2/User |

```
PROBE send: block: GTB#2 'Ableton Push 3 MIDI User Port' dir=BlockOutput firstGroup=1 groupCount=1 => ...
PROBE send: open_port -> OK is_port_open=1 current_api=4099
PROBE SEND di encoding=ump callerGroup=0 bytes=[F0 7E 7F 06 01 F7] umps=[30047E7F 06010000] -> OK
GT RX C1/Live 26B DI-REPLY [F0 7E 01 06 02 00 21 1D ...]
PROBE SEND pal0 encoding=ump callerGroup=0 bytes=[F0 00 21 1D 01 01 04 00 F7] umps=[30160021 1D010104 30310000 00000000] -> OK
GT RX C1/Live 17B PAL-REPLY idx=0 [...]
---- same port, caller group 1
PROBE SEND di encoding=ump callerGroup=1 bytes=[F0 7E 7F 06 01 F7] umps=[31047E7F 06010000] -> OK
GT RX C2/User 26B DI-REPLY [...]
```

The opened block's group (1) never reaches the wire; the cable is selected purely by the
group nibble the caller put in the UMP, and the MIDI 1.0 byte path always stamps group 0.
Every send call returned OK.

### 3.3 (g) Batch handled as one message: CONFIRMED (pinned, head)

**SysEx loss.** `recv-wms-ordered` sends the small replies first, so the per-message path is
visible before any overrun. Release build, pinned (head identical apart from timestamps):

```
GT TX C1/Live mode1 [F0 00 21 1D 01 01 0A 01 F7]
PROBE [Live Port 4] MSG  words=[30160021 1D01010A 30310100 00000000] MT3 sysex7 g0 START n=6 [00 21 1D 01 01 0A]
PROBE [Live Port 4] RAW  batch words=4 [30160021 1D01010A 30310100 00000000]
GT RX C1/Live 9B MODE-REPORT 1 [F0 00 21 1D 01 01 0A 01 F7]
GT TX C1/Live mode2 [...]
PROBE [Live Port 4] MSG  words=[30160021 1D01010A 30310200 00000000] MT3 sysex7 g0 START n=6 [...] => ABANDONED open sysex (6 bytes so far)
GT TX C1/Live pal0 [...]
PROBE [Live Port 4] MSG  words=[30160021 1D010104 30260000 00000000] MT3 sysex7 g0 START n=6 [...] => ABANDONED open sysex (6 bytes so far)
PROBE [Live Port 4] RAW  batch words=6 [30160021 1D010104 30260000 00000000 30330000 00000000]
```

One `on_message` call per batch, carrying the batch's first words; the End / Continue packets
are never delivered as messages. A per-packet consumer sees Start after Start and
never completes a SysEx. Totals from `recv-wms-rawonly` vs the message path:

```
SUMMARY [Live Port 4] RAW: batches=38 words=56 maxBatch=8 hist={1:34 4:2 6:1 8:1} umps=45 sysexComplete=4
SUMMARY [Live Port 4] RAW-walk sysex 26B DI-REPLY / 17B PAL-REPLY idx=0 / 9B MODE-REPORT 1 / 9B MODE-REPORT 2
```

On the message path, every SysEx that reached `on_message` in any run, on either build, was a
lone `START` (2 + 2 in `recv-wms`, 8 + 8 in `recv-wms-ordered` release, 6 + 6 before the ASan
abort): none completed. The both-callback runs never reach their own SUMMARY lines because
the process dies first (below), so the count comes from the per-line log.

The raw batch walked by UMP size reproduces exactly what the WinMM ground truth received,
so WMS delivers the data intact; the loss is libremidi's.

**Stack overrun.** ASan, both builds, at the first 8-word batch (the DI reply):

```
==27676==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x0012e47ff0b0 ...
WRITE of size 32 at 0x0012e47ff0b0 thread T2
    #0 memmove
    #4 std::copy<std::_Span_iterator<unsigned int const >,unsigned int *>
    #5 libremidi::midi2::input_state_machine::on_bytes_segmented ...\libremidi\pinned\include\libremidi\detail\midi_stream_decoder.hpp:469
    #6 libremidi::midi2::input_state_machine::on_bytes ...\midi_stream_decoder.hpp:380
    #7 libremidi::winmidi::midi_in_impl::process_message ...\backends\winmidi\midi_in.hpp:238     (head: :251)
    #8 libremidi::winmidi::midi_in_impl::raw_callback_type::MessagesReceived ...\midi_in.hpp:66
    #9 DllGetActivationFactory+0x2a32 (...\Desktop App SDK Runtime\Microsoft.Windows.Devices.Midi2.dll)
   #10 (C:\Windows\System32\Midi2.MidiSrvTransport.dll)
  This frame has 4 object(s): [32, 56) 'msg' ...
Thread T2 created by T0 here: ... MidiEndpointConnection::Open <- midi_in_impl::open_port midi_in.hpp:132
```

32 bytes into the 24-byte `libremidi::ump`. 4- and 6-word batches stay inside the object
(the 6-word one overwrites the timestamp field, which is then reassigned), so ASan fires
exactly at the first batch longer than 6 words. The raw-only ASan runs are clean.

**Release-build consequence (new).** With `/O2` (MSVC `/GS` on by default) the overrun did not
fail silently in any of four runs (`recv-wms` and `recv-wms-ordered`, pinned and head):

```
T=521664.121 PROBE [Live Port 4] MSG ... START n=6 [7E 01 06 02 00 21]      <- first 8-word batch
T=521664.970 GT RX C2/User 26B DI-REPLY ...                                  <- probe gets nothing
  ... no callback of any kind (not even 1-word Active Sensing) on either port ...
T=521671.703 PROBE recv: closing
T=521671.705 PROBE [User Port 5] MSG ts=5216649197768 ... START               <- queued since 521664.92
[run] probe exit=0xC0000409                                                  <- STATUS_STACK_BUFFER_OVERRUN
```

After the first overrun, delivery to every Push port of the probe stopped (7.6 s here, 7.4 s on
head, 2.3 to 2.4 s in the ordered runs, measured from the overrun to the next callback), the
queued callbacks were then flushed, and the next overrun terminated the process with `0xC0000409`. The exact outcome is undefined behaviour of this
binary's stack layout; UE's build may differ. One relevant point for the earlier analysis:
"nothing received after re-binding" in the downstream consumer's run is also consistent with this stall,
not only with the Push being in User mode.

### 3.4 (k) `Open()` result ignored: NOT REPRODUCED (pinned, head)

```
[Live Port 1] libremidi midi_out.open_port -> OK is_port_open=1    (all six Push ports: OK)
[shadow-endpoint] SHADOW Open() first=1 second(already open)=1
[shadow-endpoint] SHADOW Open() after DisconnectEndpointConnection=1
[bogus-block-99] libremidi midi_out.open_port -> FAIL(Cannot assign requested address)
[closed-session] SHADOW CreateEndpointConnection -> null
[closed-session] libremidi midi_out.open_port -> FAIL(Device or resource busy) is_port_open=0
[closed-session] libremidi midi_in.open_port -> FAIL(Device or resource busy) is_port_open=0
```

No case made `Open()` return `false`: a closed session fails earlier (null connection, which
libremidi handles), and `Open()` even returns `true` for a connection that was already
disconnected. The unchecked return value is real in the code but could not be triggered on
this machine; also note that checking it would not have caught the disconnected case.

### 3.5 (j) WinMM UMP-over-MIDI-1 wrap drops SysEx: CONFIRMED (pinned, head)

Same WinMM port opened twice in the probe: once through the UMP API (`ump_input_configuration`
on `WINDOWS_MM`, i.e. libremidi's wrap), once as a plain MIDI-1 input.

```
GT TX C2/User di [F0 7E 7F 06 01 F7]
PROBE [MIDIIN2 (Ableton Push 3 MIDI) 1] MSG  words=[30167E01 06020021 30261D69 32030001] MT3 sysex7 g0 START n=6 [7E 01 06 02 00 21]
PROBE [MIDIIN2 (Ableton Push 3 MIDI) 1] M1   26B [F0 7E 01 06 02 00 21 ... 14 04 F7] DI-REPLY
GT RX C2/User 26B DI-REPLY [...]
SUMMARY [MIDIIN2 (Ableton Push 3 MIDI) 1] MSG: calls=4 sysexComplete=0 sysexOpenAtEnd=1 abandoned=3
SUMMARY [MIDIIN2 (Ableton Push 3 MIDI) 1] M1:  calls=4 sysex=4
```

Identical on cable 1 and on both builds, release and ASan. The wrap is not an overrun (it copies
at most 4 words), so ASan is clean. Note also that `on_raw_data` is never called through the
wrap (`RAW: batches=0`). Sending through libremidi WinMM via the UMP API works (§3.2 last row).

### 3.6 (i) Timestamps: CONFIRMED on pinned, FIXED on head

Same DI reply, raw callback, with the probe's own QPC tick count at callback time:

```
pinned  RAW batch words=8 ts_lib=5216396050654    qpc_now=5216396056340     (ticks, 100 ns)
head    RAW batch words=8 ts_lib=521814355359600  qpc_now=5218143558225     (ns = ticks x 100)
```

Bumping past `5c839a5` therefore still requires removing the ADR-0002 wrapper correction in
the same change.

### 3.7 Other observations

- **Duplicate block numbers on loopback endpoints.** "Default App Loopback (A/B)" and "Loopback
  ERAE (A/B)" each expose two GTBs with `Number()=1`, one `BlockInput` and one `BlockOutput`.
  libremidi lists the `BlockInput` one as an input and the `BlockOutput` one as an output (the (c)
  pattern again), but `get_port(device, 1)` returns the first match for both, so `open_port` on
  the "output" resolves the `BlockInput` GTB. Harmless today (both are group 0), but `port.port`
  is not a unique key on these endpoints; relevant to (a)/#254.
- **Active Sensing.** The Push 3 sends `FE` every 270 ms on cables 1 and 2 over WMS. libremidi's
  default `ignore_sensing` drops it on the message path; it shows on the raw path.
- **Per-connection receive thread.** ASan shows the callback thread is created inside
  `MidiEndpointConnection::Open()` (`Midi2.MidiSrvTransport.dll`), yet the release-build stall
  blocked the other port's connection too.
- **WinMM input-close use-after-free** (pinned, fixed upstream in `49d3b8a`): not reproduced in
  the ASan runs; one run per build, so absence is weak evidence.

---

## 4. Consequences for the fix plan

- The minimal pair from the analysis, (d) + (g), is exactly what the hardware shows. Both are
  unfixed on upstream `master`; a submodule bump alone changes neither.
- (g) is a crash, not only a loss. A fix must walk the batch by UMP size and bound each copy;
  the pass criterion is `MSG: sysexComplete` equal to `RAW: sysexComplete` with no ASan report
  and exit 0 in `recv-wms` (README table).
- (c) is visible only in metadata on the Push; its runtime check needs an asymmetric device or
  the loopbacks.
- (k) stays a correctness fix without a hardware reproducer.
- (j) blocks the WinMM fallback for SysEx receive exactly as predicted.

## 5. State left behind

- Push 3 restored to Dual mode via WinMM at the end (`restore`: `MODE-REPORT 2` on cables 1 and 2).
- No probe or ground-truth process left running; Live and the downstream application's Push host untouched.
- Test machine: the probe directory (sources, two libremidi trees, projection copy, four builds,
  193 log files), kept outside this repository.
- macOS development machine: the probe harness (sources, README, `stage.sh`, logs), also kept
  outside this repository.
