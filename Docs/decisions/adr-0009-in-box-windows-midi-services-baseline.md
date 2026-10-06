---
description: The in-box Windows MIDI Services port (Windows.Devices.Midi2, Preview 10, files supplied by the host project) is the baseline on main and the out-of-band RC4 path is retired, because Microsoft withdrew the RC4 runtime on 2026-10-01; main takes preview/winmidi-inbox by fast-forward, libremidi has one pin (libremidi4ue-pin-inbox-20261004), and the fork's integration branch libremidi4ue is fast-forwarded to it
type: decision
status: accepted
updated: 2026-10-06
amends: adr-0003, adr-0008
---

# ADR-0009: The in-box Windows MIDI Services port is the baseline; the RC4 path is retired

## Context and problem statement

[ADR-0008](adr-0008-windows-midi-services-preview10-host-supplied.md) carried the in-box port on
`preview/winmidi-inbox` only and kept `main` on the RC4 code until Windows ships the API in-box. The
RC4 runtime (`Microsoft.Windows.Devices.Midi2`, App SDK `1.0.17-rc.4.25`) was withdrawn by Microsoft
on 2026-10-01, so `main`'s only Windows MIDI 2.0 path cannot be installed any more: observed on
2026-10-06, `main` on a Windows machine without the RC4 runtime silently comes up as a Dummy backend
with 0 ports. The in-box port, by contrast, ran the hosted-controller walks on Windows 11 on
2026-10-06. The owner decided on 2026-10-06 that the preview branch is the baseline. Does the in-box
port stay a branch until the in-box ship, or become the baseline on `main` now?

## Considered options

- Keep `main` on RC4 and the port on the branch until the in-box ship (ADR-0008 decision 1).
- Make the in-box port the baseline now: fast-forward `main` to `preview/winmidi-inbox`.
- Keep both paths on `main` behind a build switch.

## Decision outcome

Chosen: fast-forward `main` to `preview/winmidi-inbox`, because the RC4 runtime it targets no longer
exists, so `main`'s Windows MIDI 2.0 path is unusable, and the stability reason for keeping the port on
a branch (ADR-0008, context) no longer holds.

1. **Baseline.** The in-box port (`Windows.Devices.Midi2`, Preview 10) is the Windows MIDI Services
   backend on `main`. The RC4 path is retired and not kept behind a switch. This amends ADR-0008
   decision 1 (branch only; "`main` stays on the RC4 code until Windows ships the API in-box") and
   its Removal plan step 4 (the merge now happens, not at the in-box ship). The branch
   `preview/winmidi-inbox` is retired once `main` contains it.
2. **ADR-0008 decisions 2 to 4 stand on `main`**: no Microsoft binaries or projection headers in the
   repository (the host project supplies `<Project>/ThirdParty/WindowsMidiServices/`), the full-path
   DLL preload in `StartupModule`, and the exception boundary. Without the host files the Windows
   MIDI Services backend is compiled out and the build log says why.
3. **One libremidi pin; the integration branch carries it.** `main` pins `0496dcd`, the commit the fork
   tag `libremidi4ue-pin-inbox-20261004` points to; ADR-0008's two pins end. In the fork, the
   integration branch `libremidi4ue` is fast-forwarded from `6a572eb` to `0496dcd` (two commits: the
   topic commit `488bb98` of `fix/winmidi-inbox-api` and its merge), so nothing is orphaned and every
   earlier pin stays reachable ([ADR-0003](adr-0003-libremidi-fork-strategy.md) rule 6). The recorded
   order of ADR-0003 rule 5 (the stack table of
   [design-winmidi-ump-fixes.md](../design-winmidi-ump-fixes.md) section 2, rows 1 to 5) gains
   `fix/winmidi-inbox-api` as its last unit. The fork branch `libremidi4ue-inbox` is redundant after
   that and is not deleted (rule 6). ADR-0003 rule 2's timing is amended for the in-box API
   migration: it is carried on `main` now, still fork-only (any upstream PR is filed by the owner);
   the port-identity redesign still waits for the in-box ship, unchanged.
4. **Minimum platform.** The Windows MIDI Services backend needs Windows 11 25H2 or later; WinMM
   (MIDI 1.0) covers earlier Windows. macOS and Linux are unaffected.
5. **ADR-0008's distribution obligations and Removal plan steps 1 to 3 stay** (drop the app-local
   files from the host project within 90 days of the in-box release, regenerate the projection from
   the in-box metadata or switch to the SDK's own, drop the preload). Step 4 and the second pin are
   done by this ADR.

### Consequences

- Good: `main` works on a machine without the withdrawn runtime; the README no longer asks for a
  runtime that cannot be obtained; one code path, one pin.
- Good: no Microsoft files enter the repository or its history.
- Accept: every Windows host project must supply the Preview 10 files itself; a clone alone builds
  without the Windows MIDI Services backend.
- Accept: Windows MIDI 2.0 on Windows versions before 11 25H2 is lost with the RC4 path.
- Rejected a build switch that keeps both paths: the RC4 runtime cannot be installed, so the second
  path could never be exercised.

### Confirmation

- `git merge-base --is-ancestor 84cabde main` holds, and
  `git ls-tree main Source/ThirdParty/libremidi/libremidi` shows `0496dcd`.
- In the fork, `libremidi4ue` equals `0496dcd`.
- No `.dll`, `.pri`, `.winmd` or `winrt/` header closure under `Source/ThirdParty/WindowsMidiServices`.
- The README states no requirement for the RC4 runtime, and a build with the host files present
  reports the projection found and the module log reports the DLL preloaded.
