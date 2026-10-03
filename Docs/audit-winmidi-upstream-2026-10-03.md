---
description: >
  Read-only sweep of upstream celtera/libremidi issues and PRs touching the Windows MIDI Services (winmidi) backend (258 items enumerated, as of 2026-10-03): per-item state and fix status, Microsoft's in-box migration and porting-guide asks, mapping to our defect ids (a)-(k) and the new upstream-only items N1-N10. Read before filing, commenting on, or tracking anything upstream.
type: reference
status: point-in-time record (2026-10-03)
updated: 2026-10-03
---

# libremidi upstream sweep: Windows MIDI Services ("winmidi") issues and PRs

Read-only sweep of `celtera/libremidi`, done 2026-10-03. Nothing on GitHub was commented on, opened or modified, and no local repo was touched. Upstream was cloned (blobless) into a scratch directory only.

Companion to the [defect audit](audit-winmidi-defects-2026-10-03.md) (our defect analysis) and [backlog.md](backlog.md) (the defect rows). The letters (a)–(k) below refer to those two documents; the N-ids (N1–N10) are defined in §4 here. Hardware evidence: [probe](audit-winmidi-probe-2026-10-03.md).

## 0. Baseline and coverage

| Item | Value |
|---|---|
| Upstream `master` | `5c839a5` (2026-09-28), "winmidi: convert input timestamps from MidiClock ticks to nanoseconds" (= PR #263) |
| Latest upstream release | **v5.4.3, 2026-01-18**. Every winmidi change merged after that date (virtual ports, FB enumeration, #217, #263) is **unreleased** and exists only on `master`. |
| Our submodule pin | `67e8ccd` (2026-07-11). It contains #195 and #217, and does **not** contain `968b5fa` (transport filter) or `5c839a5` (#263). |
| Upstream winmidi SDK pin | `cmake/libremidi.winmidi.cmake:8` downloads `Microsoft.Windows.Devices.Midi2.1.0.15-rc.2.15.nupkg` (rc-2; bumped from rc-1 by `477f3c1`, 2026-02-22). Namespace everywhere is still `winrt::Microsoft::Windows::Devices::Midi2`. |
| Items enumerated | All 258 issues and PRs, all 720 issue comments, all 39 PR review comments, the 6 GitHub Discussions, and `git log` of `include/libremidi/backends/winmidi/` and `cmake/libremidi.winmidi.cmake`. A keyword sweep (~40 terms, including every term in the brief) was run through the search API on top of that. |
| Labels | None of the winmidi-relevant items carry any label. |

Line numbers in this document refer to upstream `master` `5c839a5` unless marked otherwise.

---

## 1. Table of relevant items

"At master" means that the claimed defect or request is still present in the code at `5c839a5`.

### 1a. Core winmidi items

| # | Kind | Title (short) | Author | State | Opened → closed | Fix: commit / PR / first release | At master? | Overlap with our list |
|---|---|---|---|---|---|---|---|---|
| **#254** | Issue | `get_port()` searches only GTBs; FB ports unopenable; MIDI 2.0 devices listed twice | WMS team member | open | 2026-09-07 → — | none; no maintainer reply as of 2026-10-03 | **Yes** | **(a)** exactly; also (b), (e), the Ordinal/"what is a port" decision |
| **#252** | Issue | Heads-up + migration guide: `Microsoft.Windows.Devices.Midi2` → in-box `Windows.Devices.Midi2` | WMS team member | open | 2026-09-07 → — | none; 0 comments as of 2026-10-03 | **Yes** (still rc-2, `Microsoft.*`) | in-box migration row; projection-removal row; (e); **new**: N1–N5, N8 |
| #234 | Issue | WMS COM path: `on_bytes` drops batched UMP packets; should use `on_bytes_multi` | contributor | open | 2026-07-15 → — | none. The maintainer acknowledged it on 2026-08-13 and asked an automated assistant to prepare a PR; that run errored and no PR exists | **Yes** (`midi_in.hpp:250-252`) | **(g)** = #234 |
| #264 | PR | ump: size the reserved message types | contributor | open | 2026-10-02 → — | not merged | **Yes** (decoder overrun) | (g) prerequisite (bounds for `on_bytes_multi`) |
| #200 | Issue | winmidi: observer returns only USB group terminal blocks | contributor | **open** (closed 06-01 by the #195 merge, reopened the same day) | 2026-03-18 → — | partial: `38a0ebd` added FB enumeration (in #195, unreleased, in our pin) but left `get_port` GTB-only | **Yes** (unopenable FB ports) | (a); (c) evidence; (e) |
| #194 | Issue | winmidi `open_virtual_port()` is not implemented | contributor | closed (completed) | 2026-01-26 → 2026-06-01 | PR #195 (`b13655a`, `38a0ebd`, `9ec5995`, `cccc943`), unreleased, in our pin | Fixed; residues N6, N7, N8 remain | **new**: virtual-only residues N6–N8 |
| #195 | PR | feature/winmidi virtual | maintainer | merged | 2026-02-22 → 2026-06-01 | `cccc943` (rebased, 6 commits) | — | as #194/#200 |
| #217 | PR | winmidi: fix input dropped when a GTB's `Number()` isn't its group index | owner | merged | 2026-06-02 → 2026-07-10 | `e8270d5`, unreleased, in our pin | Fixed; its stated leftovers ((b), (a)) remain | origin of (b); (a) |
| #263 | PR | winmidi: convert input timestamps from MidiClock ticks to ns | owner | merged | 2026-09-28 → 2026-09-28 | `5c839a5` = current `master` HEAD, unreleased, **not in our pin** | Fixed | **(i)**; our backlog row (i) still says "open" and is stale |
| #191 | PR | WIP ump endpoint (native bidirectional endpoint API) | maintainer | open (WIP) | 2026-01-17 → — | not merged; winmidi gets only `void` stubs (`midi_endpoint = void`) | n/a | related to (h) and the "what is a port" decision; no winmidi code |
| #233 | Issue | MIDI 2.0 UMP support (status of #191?) | contributor | closed by author | 2026-07-13 → 2026-07-24 | n/a. Maintainer: no time to work on it | n/a | context: #191 is stalled |
| #181 | PR | winmidi: implement the raw API, try to fix SDK issues | maintainer | merged | 2025-12-31 → 2026-01-01 | `5324d01`, `08c829e`, `caf04f1`, **v5.4.0** | — | introduced the COM fast path in which (g)/#234 lives |
| #172 | Issue | Update windows midi services ("new update broke compat") | maintainer | closed | 2025-08-29 → 2026-01-01 | via #181, v5.4.0 | Fixed | none |
| #178 | Issue | Duplicate namespace declaration in winmidi `helpers.hpp` | owner | closed | 2025-12-25 → 2025-12-28 | `ad280e7`, v5.4.0 | Fixed | none |
| #141 | PR | Feature/winmidi update (observer, MIDI1→2 conversion, context sharing, group filtering) | maintainer | merged | 2025-03-03 → 2025-03-03 | `565717c`, `bf0c5fd`, **v5.0.0** | — | origin of the per-port session/connection model (h) and the old `port.port - 1` filter that #217 replaced |
| #143 | PR | win32: make cppwinrt more reliable, enable winuwp on msys | maintainer | merged | 2025-03-12 → 2025-03-12 | `e290891`, `ee81007`, v5.0.0 | — | build only |

### 1b. Microsoft-filed WinMM items (not winmidi, but from the same reviewer and relevant to any WinMM fallback)

| # | Kind | Title (short) | Author | State | Opened | Fix | At master? | Overlap |
|---|---|---|---|---|---|---|---|---|
| **#251** | Issue | [Perf] WinMM default observer calls `midi*GetDevCaps` for every device every 100 ms, even with no callbacks | WMS team member | open | 2026-09-07 | none (design discussion only) | **Yes** (`winmm/config.hpp:27` `poll_period{100}`) | **new**: N9 (WinMM fallback) |
| **#253** | Issue | WinMM: `midi_out_winmm::outHandle` never initialized; destructor may `midiOutClose` an indeterminate handle | WMS team member | open | 2026-09-07 | none (the maintainer had invited this filing on #251) | **Yes** (`winmm/midi_out.hpp:152`) | **new**: N10 |

### 1c. Tangential (build, packaging, bindings); listed for completeness

| # | Kind | Title (short) | State | Note |
|---|---|---|---|---|
| #148 / #149 | Issue / PR | Missing link to `windowsapp.lib` (vcpkg, cppwinrt) | closed / merged | `515c0ab`, tag v5.2.0. A vcpkg maintainer objected that this pulls the UWP lib into win32 desktop builds; no follow-up. `libremidi.winmidi.cmake:77` still links `RuntimeObject windowsapp`. |
| #163 | Issue | x86_64-w64-mingw32 ld errors (`RoGetActivationFactory`) | closed | `8720356`, `64e861a`, `3b41e6a`, v5.3.1 (link RuntimeObject/combase publicly) |
| #196 | PR | Initialize `m2` to silence MSVC C4701 (`cmidi2.hpp`) | merged | owner PR, `3f93260`, unreleased, in our pin |
| #107 | Issue | Expose macOS LocationID / Windows ContainerID | closed | Maintainer noted that WMS exposes the container ID directly; winmidi sets `.container = ContainerId()` (`0f3fe10`, v5.0.0; `observer.hpp:109`) |
| #50, #134, #137, #188, #190 | Issues | vcpkg port; pylibremidi packaging and Python-binding crashes on Windows | mixed | Not winmidi-backend defects. #190 (open) is a nanobind/Python callback crash on Windows 11. #50 records that the maintainer had held the vcpkg port back because of breaking changes in the WinMidi SDK previews. |
| #182, #243 | PRs | KDMAPI backend; WinMM teardown fixes | merged | #243 (`7b66533`, 2026-08-08, after our pin) is the WinMM input rewrite that Microsoft praised in #252 |
| Discussion #185 | Release notes v5.4.0 | — | "Windows MIDI Services: support updated to the RC1 release headers" and "add support for the newly introduced COM fast-path" |
| Discussion #219 | Unified MIDI 1/2 interface question | — | The maintainer says the OS (incl. WMS) does the MIDI 1↔2 translation; no defect |

### 1d. Cross-repo items cited in the threads (`microsoft/MIDI`)

| microsoft/MIDI # | Title (short) | State | Relevance |
|---|---|---|---|
| #869 | Simple MIDI 1.0 loopback ports (loopMIDI-style) | closed 2026-02-28 | A WMS team member announced it on #194 as a preview feature, "several months" from Windows |
| #933 | `CreateVirtualDevice()` fails if `ProductInstanceId` contains whitespace (filed by a contributor from #194) | closed 2026-07-19 | The WMS team member: fixed in the in-box SDK preview. libremidi still passes the port name as the product instance id (N7). |
| #1047 | Virtual endpoints unusable / service hang on virtual-device close (build 26100/26200.8875) | closed 2026-09-14 | Referenced in #252. Fix merged; rolls out with the end-of-November 2026 Windows CFR on 25H2+, enabled within roughly 30–45 days |

---

## 2. Per-item notes

### #254: `get_port()` GTB-only; FB ports unopenable; double listing (Microsoft)
- **Claim:** the observer emits a port for every declared function block **and** every group terminal block. `get_port()` (`helpers.hpp:67-91`, GTB loop at 81) searches only GTBs, so every FB-derived port fails `open_port` with `address_not_available`, and an endpoint with both block kinds is listed twice.
  - Worked example: a Waldorf Iridium shows "Synth 0" (FB, fails to open) and "Synth 1" (GTB, works).
  - The issue identifies this as the unresolved half of #200: `38a0ebd` added FB enumeration without updating `get_port`.
- **Rule given:** if an endpoint declares FBs, use only those; otherwise fall back to GTBs. Never merge the two. One helper should serve both enumeration and resolution. FBs arrive seconds after the endpoint, so the list must be rebuilt on `Updated` when `AreFunctionBlocksUpdated` is set.
- **Second step:** `port.port` should carry the **group index**, not the block number. The message address is endpoint + group. That would also handle multi-group FBs.
- **Maintainer response:** none. The only comment is the author's own link to the porting guide (2026-09-08).
- **Fixed:** no.
- **Code areas:** `helpers.hpp:67-91`; `observer.hpp:140-196` (FB loops at 151-156 and 180-185, GTB loops at 158-163 and 187-192); `observer.hpp:271-281` (`add_device`, the same double-add on hotplug); `midi_in.hpp:103-112`; `midi_out.hpp:45-47`.
- **At master:** yes.
- **Ours:** **(a)**, exactly. The group-index proposal is the Ordinal row / open decision 1 in the wip doc. Multi-group FBs are **(b)**. Rebuilding on `AreFunctionBlocksUpdated` is **(e)**.

### #252: in-box migration guide (Microsoft)
- **Claim / request:** a friendly heads-up with a complete before/after delta against libremidi's code (`cac4d84`).
  - The SDK moves from the redistributable `Microsoft.Windows.Devices.Midi2` to in-box `Windows.Devices.Midi2`, with type changes and a smaller bootstrap.
  - The one runtime hazard is that four getters can now return **null** and crash on the first MIDI 1.0 device. Everything else is compile-time.
  - Two optional performance items (send batching, release-build validation). The full list of asks is in §3.
- **Maintainer response:** none (0 comments).
- **Fixed:** no.
- **Code areas:** all winmidi files plus `cmake/libremidi.winmidi.cmake`; `observer.hpp:101-119` (null hazard at 104 and 112-118; second `GetTransportSuppliedInfo()` call at 118); `observer.hpp:205-221` (watcher args, `// OPTIMIZEME` round trip); `midi_in.hpp:59-64` (`UINT32*` callback); `helpers.hpp:31-40, 93-211` (hand-written IIDs, `IMidiClientInitializer` bootstrap); `helpers.hpp:227-269` (virtual config); `midi_out.hpp:115-146` (`write_raw`).
- **At master:** yes, entirely. Still rc-2 and `Microsoft.*`.
- **Ours:**
  - the in-box migration row;
  - the projection-removal row: its trigger, a Microsoft comment on #252 saying the metadata is in the Windows SDK, has **not** happened;
  - (e), the `Updated` flag gating;
  - **new** sub-items N1–N5 and N8 (§4).

### #234: COM path drops batched UMP packets
- **Claim:**
  - The COM `process_message` passes the whole callback buffer (`wordCount` words) to `on_bytes`, which treats it as one message.
  - With wordCount > 4, only the first UMP survives. Multi-packet SysEx7 loses its Continue and End packets.
  - Reproduced on Windows 11 25H2 with a USB MIDI 1.0 device sending long SysEx. The reporter's patch: `on_bytes` → `on_bytes_multi`.
  - The reporter also credits the #217 group-filter fix. They cite it as being in `67e8ccd`, which is the master commit they pinned, the same commit as our pin.
- **Maintainer response:** acknowledged on 2026-08-13 and asked an automated assistant to prepare a PR from the patch. That run errored part-way and no PR followed.
- **Fixed:** no.
- **Code areas:** `midi_in.hpp:227-253`. The group verdict comes from the batch's first word (243-248) and `on_bytes` gets the whole batch (251-252). `detail/midi_stream_decoder.hpp` (`on_bytes_multi` bounds) is the subject of #264.
- **At master:** yes.
- **Ours:** **(g)** is #234. The one-line patch fixes SysEx but keeps the batch-level group verdict and the groupless-message filtering; the audit's (g) fix sketch covers both. Note that `on_bytes_multi` is unsafe on reserved message types until #264 lands.

### #264: size the reserved UMP message types
- **Claim:** `cmidi2_ump_get_num_bytes` returns `0xFF` for reserved MTs (0x6–0xC, 0xE). One such packet makes UMP input read past its buffer and crash. The PR sizes them per UMP 1.1 §2.1.4 and stops splitters at a truncated packet, with new tests in `tests/unit/midi_stream_decoder.cpp` that crash without the fix.
- **Maintainer response:** none yet (opened 2026-10-02).
- **Fixed:** pending.
- **Code areas:** `cmidi2.hpp`, `detail/midi_stream_decoder.hpp`, `detail/ump_stream.hpp`.
- **At master:** the defect is present.
- **Ours:** the bounds prerequisite for (g). Coordinate rather than duplicate.

### #200: observer returns only USB group terminal blocks
- **Claim (contributor):** virtual (non-USB) endpoints have no GTBs, so the observer listed nothing for them. The reporter suggested `GetDeclaredFunctionBlocks()` instead.
- **Maintainer:** on hardware the maintainer saw MIDI 1.0 devices with GTBs only, so a replacement looked wrong, and shipped `38a0ebd` (enumerate both) on `feature/winmidi_virtual`.
- **WMS team member (2026-05-02):**
  - GTBs are static, come from USB descriptors, and WMS synthesizes them for every MIDI 1.0 device.
  - FBs are MIDI 2.0-only, discovered in-protocol (seconds), may overlap groups, may move at runtime, and **should be used instead of GTBs when present**.
  - There is a GTB→FB conversion helper.
  - "the address for any message is not a GTB or FB", but endpoint + group index.
- **Maintainer follow-up question:** "What should be used as differentiator?" It went unanswered until #254.
- **Later:** the reporter confirmed that enumeration works, but opening the virtual ports fails with "Cannot assign requested address". #254 explains this as the GTB-only `get_port`.
  - The thread also shows a virtual *output* listed among *outputs* in another process. That is the direction-semantics evidence for our (c).
- **Fixed:** partially (`38a0ebd`, in #195; unreleased; in our pin).
- **At master:** yes (the open half).
- **Ours:** (a); (c) evidence; (e).

### #194 / #195: virtual ports
- **Request:** implement `open_virtual_port()` on winmidi (WMS supports virtual devices; WinMM does not).
- **Outcome:** PR #195 merged 2026-06-01, unreleased, in our pin:
  - `b13655a` implements virtual ports through `MidiVirtualDeviceManager`;
  - `9ec5995` keeps virtual ports on the WinRT path, because the app side of a virtual device is incompatible with the COM raw callback;
  - `cccc943` adds `AddMessageProcessingPlugin` on outputs too.
- **The WMS team member's 10 comments (paraphrased):**
  - two virtual options: a transient bidirectional loopback pair (A/B, MIDI 1 + 2) or a full MIDI 2.0 virtual device. The team member asked whether libremidi could live with a bidirectional pair instead of unidirectional MIDI 1.0-style ports;
  - announced simple MIDI 1.0 loopback ports (microsoft/MIDI#869);
  - older MIDI 1.0 APIs see WMS endpoints only when the WMS feature is enabled;
  - pointed to the `simple-app-to-app-midi` sample and the rc3 SDK;
  - the public side of a MIDI 2.0 virtual device is not published until the app side has opened its connection, by design;
  - allow time for FB processing: 16 groups can mean up to 32 MIDI 1.0 ports, plus a discovery timeout of 5–10 s if identity info is missing;
  - **the raw COM callback bypasses message-processing plugins, so declared FBs never get through on the app side**;
  - only the app side of a virtual device is incompatible with COM extensions; its public endpoint is a normal endpoint.
- **Residues still at master:**
  - **N6:** the reporter found that on MSVC `stdx::error` compares unequal to `stdx::error{}` despite code 0 (different `m_domain`). That makes `open_virtual_port` report failure. The maintainer considered folding a domain check into `is_set()`. **No commit** to `system_error2.hpp` since; the edge API still compares `err != stdx::error{}` (`midi_in.cpp:334-343`).
  - **N7:** the virtual device's `ProductInstanceId` is the port name (`midi_in.hpp:153`, `midi_out.hpp:74` → `helpers.hpp:239`). Names with whitespace or punctuation fail `CreateVirtualDevice` on the preview SDK. That bug (microsoft/MIDI#933) is fixed only in the in-box preview.
  - **N8:** `DeclaredFunctionBlockCount` is never set while `HasStaticFunctionBlocks = true` (`helpers.hpp:236-265`). The WMS team member (#252) calls this a plausible cause of the "FBs not registered" symptom.
- **Ours:** none of (a)–(k) cover virtual devices. These are new, and matter only if we ever call `open_virtual_port`.

### #217: group filter by `FirstGroup()` (owner PR)
- Replaced the `port.port - 1` filter with `gp.FirstGroup().Index()` (`midi_in.hpp:107-112`).
- The PR body explicitly left two items open: multi-group blocks pass only their first group, which is **(b)**, and FB/GTB `get_port` mismatch, which is **(a)**/#254.
- Merged 2026-07-10, 38 days after opening. `e8270d5`, unreleased, in our pin.

### #263: timestamps ticks → ns (owner PR)
- Both input paths now convert MidiClock ticks to ns via `TimestampFrequency()` (`midi_in.hpp:81-83, 188-195, 221, 250`). Merged within about 14 h. `5c839a5`, unreleased, not in our pin.
- **Ours:** **(i)**. Done upstream. The Libremidi4UE backlog row still says "open since 2026-09-28" and should be updated. Bumping the submodule past `5c839a5` must remove the ADR-0002 wrapper correction in the same change.
- This matches the porting guide: timestamps are "absolute, from system boot, in 100 ns units".

### #191 / #233: native UMP endpoint API (WIP)
- The new endpoint-centric API (bidirectional, MIDI 2.0-native) is implemented for ALSA only. winmidi gets only `void` typedef stubs.
- The maintainer said in July 2026 that there is no time for it. A contributor (#233) is building similar functionality and may open-source it.
- **Ours:** long-term relevance to (h) and to the "port = endpoint + group" question. Not actionable.

### #181 / #172: COM fast path (v5.4.0)
- Introduced `IMidiEndpointConnectionRaw` send/receive and the hand-written IIDs / `IMidiClientInitializer` bootstrap that #252 asks to delete. It also fixed the SDK-compat break of #172.
- **Ours:** the code that (g) lives in.

### #141 / #143: winmidi update and cppwinrt (v5.0.0)
- `bf0c5fd` "context sharing and group filtering" created `winmidi::input/output_configuration::context` (an optional shared `MidiSession*`, `config.hpp:19-29`). Otherwise each `midi_in`/`midi_out` creates its own session (`midi_in.hpp:75-77`, `midi_out.hpp:25-27`) and its own connection.
- **Ours:** (h) and N5 (§4).

### #251: WinMM observer polling (Microsoft)
- **Claim:** `make<observer_winmm>` defaults to the threaded poller. It wakes every 100 ms and calls `midiIn/OutGetNumDevs` plus `GetDevCaps` per device on both flows, **even when no callbacks are registered** (e.g. `midiprobe`).
  - Measured: 62 calls / 0.77 ms per pass at 28 in / 32 out, about 620 calls/s per process.
  - Under WMS these calls go through `wdmaud2.drv`, which takes a per-process lock shared with the **send** path. During plug/unplug, enumeration stalls by tens of ms and delays `midiOutShortMsg` on other threads in the same process.
  - Each `open_port` also re-enumerates synchronously, often on the UI thread.
- **Asks:**
  1. don't start the poller without callbacks;
  2. skip the `GetDevCaps` sweep unless the device count changed;
  3. raise the default `poll_period` to 0.5–1 s;
  4. use `CM_Register_Notification` on `DEVINTERFACE_MIDI_INPUT/OUTPUT` with a re-arming debounce, plus a slow safety poll;
  5. cache the name→index map so `open_port` does not re-enumerate.
- **Side notes:** display-name matching makes identical devices renumber on removal (WMS team member: no good WinMM solution; the new API has durable ids). Also the `outHandle` bug, filed as #253.
- **Maintainer:**
  - asked about fast unplug/replug races and XP support;
  - WMS team member: the interface GUIDs date from Windows 10; suggested a count gate plus a full poll every ~2 s; the long-term answer is the WMS watcher.
  - The maintainer said the library was born from exactly that many-devices case, and welcomed an official answer from the Windows MIDI Services team.
- **Fixed:** no.
- **At master:** yes.
- **Ours:** **new (N9)**. Relevant only if the WinMM fallback is used, or if a WinMM observer coexists with WMS sends.

### #253: WinMM `outHandle` uninitialized (Microsoft)
- **Claim:** `HMIDIOUT outHandle;` has no initializer, and `close_port()` from the destructor calls `midiOutClose` on it. This is reachable by constructing and destroying a `midi_out` without opening, by a failed name lookup, or by a failed `midiOutOpen`.
  - Reading the uninitialized value is UB.
  - In debug builds the handle is `0xCDCD…`.
  - In release builds a recycled allocation can close a *live* handle belonging to another port.
- **Fix asked:** `HMIDIOUT outHandle{};` and reset it to `nullptr` on open failure, mirroring `midi_in_winmm`.
- **Maintainer:** no reply on #253 itself; the filing had been invited on #251.
- **Fixed:** no.
- **At master:** yes (`winmm/midi_out.hpp:152`).
- **Ours:** **new (N10)**, WinMM only.

---

## 3. The Microsoft team's items and exact asks

All of these come from a WMS team member, in issues #251–#254. Parts of those filings are marked as AI-assisted drafts reviewed by the author, with the author's own notes interleaved. Quotes are kept to short fragments; everything else is paraphrase.

### 3.1 In-box migration (#252)

**Target:** `Windows.Devices.Midi2` (in Windows) instead of `Microsoft.Windows.Devices.Midi2` (out-of-band, libremidi pins `1.0.15-rc.2.15`). The latest dev preview cited is `0.99.66-devpreview.7`, with docs under `microsoft.github.io/MIDI/sdk-reference/`.

| # | Change | Where in libremidi (master) | Compiler catches it? |
|---|---|---|---|
| 1 | `GetTransportSuppliedInfo()`, `GetDeclaredEndpointInfo()`, `GetDeclaredDeviceIdentity()`, `GetDeclaredStreamConfiguration()` become runtimeclasses that **can be null**; properties become method calls | `observer.hpp:104, 112-118` | **No: runtime crash.** Another library hit `0xC0000005` on the first device. Null-check, hoist the single call. |
| 2 | Root namespace → `Windows.Devices.Midi2` | all files, `config.hpp:7-28` forward decls | yes |
| 3 | Enumeration types move to `.Enumeration`: device information, watcher and its args, `MidiFunctionBlock`, `MidiGroupTerminalBlock`, declared-info types, `MidiProtocol`. Session, connection, messages, clock, group, channel and `MidiApi` stay in the root. | `helpers.hpp`, `observer.hpp`, `midi_in/out.hpp` | yes |
| 4 | `Endpoints.Virtual` → `Transports.Virtual` | `helpers.hpp:21-23, 227-234`, virtual paths | yes |
| 5 | Declared-info: field assignment → setter calls (or the new parameterized constructor) | `helpers.hpp:236-245` | yes |
| 6 | Watcher `Updated`/`Removed` args drop `EndpointDeviceId()` and hand over the device (`UpdatedDevice()`, `RemovedDevice()`). This removes the `CreateFromEndpointDeviceId` round trip. | `observer.hpp:205-221` | yes |
| 7 | COM callback `UINT32*` → `UINT32 const*`. A missed change shows up as **C2259 "cannot instantiate abstract class"**, not as a signature error. The ABI is unchanged. | `midi_in.hpp:59-64` | yes (as C2259) |
| 8 | Bootstrap collapses to `MidiApi::EnsureServiceAvailable()` (noexcept bool). Drop the three `CoCreateInstance` calls, the hand-declared `IMidiClientInitializer` and the hard-coded IIDs (the issue author verified that the GUIDs are currently correct). Use `__uuidof(IMidiEndpointConnectionRaw)`. | `helpers.hpp:31-40, 93-211`; `.as(...)` at `midi_in.hpp:129`, `midi_out.hpp:55` | n/a (deletion) |

**Additional asks in #252, beyond the mechanical delta:**
- **Set `DeclaredFunctionBlockCount`** to the number of appended blocks when `HasStaticFunctionBlocks` is true. It is 0 today, and the issue suggests this as a plausible contributor to #194's "function blocks don't get registered" (`helpers.hpp:236-265`). → N8.
- **Gate `Updated` handling on the change flags**: `AreFunctionBlocksUpdated`, `AreGroupTerminalBlocksUpdated`, `IsNameUpdated`, …. Mute toggles and renames currently cause remove+add churn. → (e).
- **Batch raw sends:** query `GetSupportedMaxMidiWordsPerTransmission()` once per connection and send the largest whole-UMP prefix per call. Today `write_raw` makes one COM call per UMP, so a 64 KB SysEx7 transfer is thousands of round trips (`midi_out.hpp:115-146, 174-195`). → N2.
- **`ValidateBufferHasOnlyCompleteUmps` runs only inside `assert`**, so release builds never validate. Call it unconditionally or document the precondition (`midi_out.hpp:122-134`). → N3.
- **Use `MidiClock::TimestampConstantSendImmediately()`** instead of the literal `0` (`midi_out.hpp:123-135`). → N4.
- Optional: `Windows.Devices.Midi2.Enumeration.Legacy` maps a WMS endpoint to the WinMM port numbers it produces, which would let callers correlate libremidi's `winmm` and `winmidi` views of the same device.
- Optional: `MidiApi::GetCurrentlySelectedApiMode()` tells the user why WMS is unavailable. In Legacy API mode WinMM is the correct backend.
- **Don't over-migrate:** session and connection creation, `Open`, `SendSingleMessagePacket`, `AddMessageProcessingPlugin`, the `MidiMessage32..128` constructors, the received-args accessors, `MidiGroup`/`MidiChannel`, the FB/GTB members and the virtual-device types are unchanged apart from the namespace.
- **Keep virtual devices on the WinRT path.** The app side of a virtual device is incompatible with the raw COM callback; libremidi's current `if (m_virtual)` branch "is exactly right".
- **Packaging:** during the preview a NuGet package is still needed for metadata and headers, so `libremidi.winmidi.cmake` keeps its job pointed at the new package. Microsoft "will comment on this issue" once the metadata is in the Windows SDK proper. **No such comment yet.**
- **Timing:** the in-box API is still moving in dev preview. Either keep winmidi gated as preview, or wait for Microsoft's "no more breaking changes" note and port in one pass. The issue author offered to open a PR with the mechanical changes (#2–#8) against a branch of the maintainer's choosing; no reply as of 2026-10-03.
- **Known service bug:** closing a virtual device hangs the service (microsoft/MIDI#1047). The fix rolls out with the end-of-November 2026 Windows update.
- **Praise:** the WinMM input backend (after #243) is described as the reference corrected version of the RtMidi code.

### 3.2 Function blocks vs group terminal blocks, and what a "port" is (#200, #254)
- When an endpoint declares function blocks, use those and ignore GTBs; with no FBs, fall back to GTBs. **Never merge** the two: they are two descriptions of one endpoint at different levels of authority.
- GTBs come from USB descriptors (driver ioctl). WMS synthesizes them 1:1 with cables for every USB and BLE MIDI 1.0 device. They are static and never renamed. Customers have asked for GTB rename, which is likely after the November release.
- FBs are discovered in-protocol from MIDI 2.0 devices only, optional per the spec, authoritative when present. They can overlap groups and can move or change at runtime unless declared static.
- Discovery takes a few seconds (up to about 10 s). During that window an endpoint has GTBs but no FBs.
  - There is **no client-visible "discovery complete" flag**; this was raised internally on the Microsoft side.
  - Use instead: the `Updated` event with `AreFunctionBlocksUpdated`; `DeclaredFunctionBlocksLastUpdateTime`; or `GetDeclaredEndpointInfo().DeclaredFunctionBlockCount()` vs `GetDeclaredFunctionBlocks().Size()`.
  - Delaying endpoint availability until discovery completes is being considered but is **not** in the November release.
- Normalize early with `MidiGroupTerminalBlock::AsEquivalentFunctionBlock()`. Use **one helper for both enumeration and `get_port()`**.
- "The address of a MIDI message is not a block": it is endpoint + group index. Proposed second step: carry the group index in `port.port`, which removes `get_port()`'s block search and handles multi-group FBs.
- The WMS team member describes "port" as meaningless in an addressed-UMP system and notes that Apple's early approach of re-addressing UMPs to the port's group confused users and was walked back in later CoreMIDI. This bears on the re-address policy in our (d) fix sketch: single-group blocks only.
- Tooling caveat: `midi endpoint properties <id>` hides GTBs when FBs exist unless `--verbose` is given.

### 3.3 Virtual devices and loopbacks (#194)
- There are two mechanisms:
  - a loopback endpoint pair (A↔B, MIDI 1.0 and 2.0 compatible, two in/out pairs with different names);
  - a MIDI 2.0 virtual device (participates in discovery, has FBs; MIDI 1.0-compatible ports are created for client connections only; the host-app connection is MIDI 2.0-only).
- Simple loopMIDI-style MIDI 1.0 loopbacks are coming (microsoft/MIDI#869).
- The public side of a virtual device is published only after the app side has opened its connection. The app side is hidden from default enumeration.
- The raw COM callback bypasses message-processing plugins, so it cannot be used on the app side of a virtual device. The public endpoint can use COM like any endpoint.
- Without built-in discovery declarations, the owning app must send the discovery replies itself.

### 3.4 WinMM asks (#251, #253)
These are listed in §2. In short:
- no poller without callbacks;
- count-gated `GetDevCaps`;
- `poll_period` of 0.5–1 s;
- `CM_Register_Notification` on the MIDI device-interface class GUIDs with a re-arming debounce, avoiding the `DBT_DEVNODES_CHANGED` mistake of JUCE#1726;
- a cached name→index map for `open_port`;
- `HMIDIOUT outHandle{}` plus a reset on open failure.

The WMS team member's position on legacy support: the watcher approach covers Windows 10/11; letting go of XP-era compatibility is suggested, but left to the maintainer.

### 3.5 The porting guide (linked on #254, 2026-09-08)
`https://microsoft.github.io/MIDI/kb/porting-midi-libraries/`, "Porting a MIDI Library or Framework to Windows MIDI Services". The page carries no date. The summary below is a paraphrase; the key sentences were checked verbatim.

- **Target and dates:**
  - target the in-box API only;
  - the out-of-band preview package "was never supported for redistribution, only for testing" and is dropped in November 2026;
  - in-box "in Windows 11 25H2 and later from the end of November 2026";
  - 24H2 leaves support in October 2026 and will not receive the in-box API, so a library's documented version floor must reflect that boundary;
  - never mix the two namespaces in one build: the type names are identical.
- **Detection:**
  - two steps: resolve `MidiApi` (try/catch), then `EnsureServiceAvailable()`;
  - Legacy API mode is a valid customer choice, so fall back to WinMM; Hybrid mode leaves MIDI 1.0 devices on the old APIs;
  - own an MTA thread for MIDI work; apartment-init failures are non-fatal.
- **COM vs WinRT:**
  - choose per connection; they are mutually exclusive;
  - COM suits libraries with their own UMP parsing (buffer-oriented, multiple messages per callback);
  - WinRT is required for listeners and virtual devices.
- **Ports:**
  - a port is endpoint device ID + group index + direction, never a block number;
  - persist ID + group, not names or indexes.
- **Connections and sessions:**
  - "Open exactly one MidiEndpointConnection per endpoint, no matter how many" port objects, ref-counted, closed with the last port (each connection costs a cross-process buffer);
  - one session per library, created lazily, named after the host app.
- **Enumeration:**
  - use `MidiEndpointDeviceWatcher`, never polling;
  - wire handlers before `Start()`;
  - FBs take precedence, never merge them with GTBs, use the same helper for enumeration and resolution;
  - rebuild on `Updated` with `AreFunctionBlocksUpdated`, continuously;
  - blank FB names mean "not yet received";
  - filter with `AllStandardEndpoints` and hide the diagnostic loopbacks from users.
- **Receiving:**
  - register listeners or the COM callback **before** `Open()`;
  - WinRT: one `MidiGroupEndpointListener` per input port with a single included group;
  - COM: filter on the group nibble yourself;
  - groupless (endpoint-scoped) messages must not be routed to group ports.
- **Sending:**
  - the group lives in the message; convert MIDI 1.0 bytes with `MidiMessageConverter` and the target `MidiGroup`;
  - query `GetSupportedMaxMidiWordsPerTransmission` per connection, never hard-code it;
  - split inside the library on message boundaries and stop at the first failed transmission;
  - send every SysEx part through the long-message path.
- **Timestamps:** "absolute, from system boot, in 100 ns units, and do not wrap".
- **WinMM habits that are now defects:**
  - set up buffers before open;
  - unprepare before free and honor `MIDIERR_STILLPLAYING`;
  - initialize handle members;
  - call `midiInStop` before `midiInReset`;
  - bound retries on `MIDIERR_NOTREADY`;
  - don't poll device counts while WMS is active.
- **Testing:** skip, rather than fail, when `EnsureServiceAvailable()` is false; use the diagnostic loopbacks and virtual devices; test on Arm64.
- **Common pitfalls named:** per-port connections ("most common mistake"), polling enumeration, block number as address, late callback setup, hard-coded transmission limit, waiting for properties to "settle", index-based persistence, mixed namespaces.

---

## 4. Mapping to our defect list

| Ours | Upstream items | Status at `5c839a5` |
|---|---|---|
| (a) GTB-only `get_port` + FB/GTB double listing | **#254 (Microsoft)**, #200 (open), #217 body | unfixed; no maintainer reply on #254 as of 2026-10-03 |
| (b) multi-group block filtered to first group | #217 body ("left as-is"); #254 (FBs spanning groups; group-index proposal) | unfixed |
| (c) block direction inverted | not raised by anyone upstream. #200's 2026-05-03 listing is evidence. The porting guide defines a port as ID + group + **direction**. | unfixed |
| (d) output group ignored / 0 | not raised upstream. Supported by the porting guide ("group lives in the message"; `MidiMessageConverter` with a target `MidiGroup`) and by #254's address = endpoint + group | unfixed |
| (e) hotplug `Updated` churn; `IsActive` unchecked | **#252** (gate on `Are*Updated` flags; use `UpdatedDevice()`), **#254** (rebuild on `AreFunctionBlocksUpdated`), porting guide | unfixed |
| (f) `wanted_port` not applied on hotplug | not raised upstream (`968b5fa` postdates the Microsoft filings) | unfixed (`observer.hpp:228, 241` use `to_port_info` directly) |
| (g) COM batch handled as one message | **#234** (+ #264 prerequisite); porting guide (COM: filter group nibble per message; route groupless separately) | unfixed; no PR |
| (h) one connection (and session) per port | porting guide: one connection per endpoint, ref-counted, and one session per library. Not raised as a libremidi issue. | unfixed |
| (i) ticks vs ns | **#263** (owner) | **fixed** `5c839a5`, unreleased, not in our pin |
| (j) UMP-over-MIDI-1 wrap drops all but first UMP (`midi_in.cpp:39-51`) | not raised upstream | unfixed |
| (k) `Open()` result ignored (`midi_in.hpp:136, 177`; `midi_out.hpp:57, 88`) | not raised upstream | unfixed |
| in-box migration row | **#252 (Microsoft)**, porting guide | not started upstream (no branch, no reply) |
| projection-removal row | #252's "we will comment when the metadata is in the Windows SDK" | trigger not yet fired (0 comments on #252) |
| Ordinal row (`LibremidiTypes.cpp:233-235`) | #254 group-index proposal; porting guide ("never use block number as address") | open decision unchanged |

### New: not in our list
- **N1:** null-returning `GetTransportSuppliedInfo()`/`GetDeclared*()` after the in-box migration (`observer.hpp:104-118`). This is a runtime crash, not a compile error, and must be part of the migration batch. Our wip doc §3 already mentions it, but there is no defect row.
- **N2:** one COM call per UMP on send; no use of `GetSupportedMaxMidiWordsPerTransmission` (`midi_out.hpp:115-146, 174-195`). Performance only today; it affects large SysEx sends such as Push firmware or patch dumps.
- **N3:** `ValidateBufferHasOnlyCompleteUmps` exists only inside `assert` (`midi_out.hpp:122-134`).
- **N4:** literal `0` instead of `TimestampConstantSendImmediately()` (cosmetic).
- **N5:** one `MidiSession` per `midi_in`/`midi_out` unless the caller supplies `context` (`config.hpp:19-29`, `midi_in.hpp:75-77`, `midi_out.hpp:25-27`). The session is named "libremidi input/output", not after the host app. This extends (h).
- **N6:** `stdx::error` equality fails on MSVC (domain mismatch), so `open_virtual_port` reports failure on success (#194, #200). Unresolved upstream; the same comparison is used in the edge API (`midi_in.cpp:334-343`). Whether it bites our non-virtual MSVC build is **unverified**.
- **N7:** the virtual device's `ProductInstanceId` = port name; names with whitespace or punctuation fail on the preview SDK (microsoft/MIDI#933, fixed only in the in-box preview). Virtual-only.
- **N8:** `DeclaredFunctionBlockCount` not set for the virtual device's static FB (`helpers.hpp:236-265`). Virtual-only.
- **N9:** WinMM observer 100 ms `GetDevCaps` polling, also with no callbacks (#251). WinMM fallback only.
- **N10:** WinMM `outHandle` uninitialized (#253). WinMM fallback only.
- Already compliant, no action: listeners and the COM callback are registered before `Open()` (`midi_in.hpp:121-136`); virtual ports use the WinRT path (`9ec5995`).
