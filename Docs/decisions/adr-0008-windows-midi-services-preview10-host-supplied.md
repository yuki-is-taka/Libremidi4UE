---
description: The Windows MIDI Services in-box port (Windows.Devices.Midi2, Preview 10) lives on branch preview/winmidi-inbox only, with no Microsoft binaries or projection headers in this repo (the host project supplies them under ThirdParty/WindowsMidiServices), a full-path DLL preload, an exception boundary, and libremidi pinned to a fork tag separate from main's pin; main stays on RC4 code until the API ships in-box
type: decision
status: accepted
updated: 2026-10-04
amends: adr-0003
---

# ADR-0008: Windows MIDI Services Preview 10, host-supplied, on a preview branch

## Context and problem statement

The out-of-band Windows MIDI Services SDK runtime (RC4, `Microsoft.Windows.Devices.Midi2`) was
withdrawn on 2026-10-01, and Microsoft drops that API in November 2026. The replacement is the
in-box API `Windows.Devices.Midi2`. Until Windows ships it in-box (expected late November 2026,
Windows 11 25H2 and later), the only way to run against it is the in-box Preview 10 build
(`inbox-preview-10`, nupkg `0.99.88-preview.10`). Preview 10 has a go-live license, but Microsoft
leans against redistribution of its binaries by reusable libraries, and this repository is public.

[ADR-0003](adr-0003-libremidi-fork-strategy.md) rule 2 keeps the in-box migration fork-only and
waits for the in-box ship ([wip-winmidi-inbox-migration.md](../wip-winmidi-inbox-migration.md)
carried the status "do nothing now"). Main's RC4 runtime is gone, so waiting leaves Windows MIDI
Services unusable in the meantime. How do we run on Preview 10 now without shipping Microsoft
binaries from a public repo or destabilizing `main`?

## Considered options

- Port on `main` and bundle the Preview 10 projection headers and runtime in the plugin.
- Port on a separate branch, with the host project supplying the Microsoft files.
- Wait for the in-box ship and leave Windows MIDI Services broken until then.

## Decision outcome

Chosen: port on the branch `preview/winmidi-inbox` with host-supplied Microsoft files, because it
restores function now, keeps Microsoft binaries and headers out of this public repository, and
leaves `main` untouched.

1. **Branch.** The port lives on `preview/winmidi-inbox` only. `main` stays on the RC4 code until
   Windows ships the API in-box; then the branch merges into `main`.
2. **No Microsoft files in the repo.** The repository bundles no Microsoft binaries and no
   projection headers (the RC4 projection under `Source/ThirdParty/WindowsMidiServices` is
   deleted on the branch). The host project supplies them under
   `<Project>/ThirdParty/WindowsMidiServices/`:
   - `Win64/include`: the C++/WinRT projection closure (headers generated from the metadata).
   - `Win64/bin`: `Windows.Devices.Midi2.dll` and `Windows.Devices.Midi2.pri`.
   - the metadata `.winmd`.

   `WindowsMidiServices.Build.cs` detects these files, sets `WITH_WINDOWS_MIDI_SERVICES`, and
   stages the `.dll` and `.pri` through `RuntimeDependencies`; `libremidi.Build.cs` uses the same
   detection to define `LIBREMIDI_WINMIDI`. Without the files the Windows MIDI Services backend is
   compiled out (WinMM / MIDI 1.0 only) and the build log says why.
3. **DLL preload.** `StartupModule` loads the DLL by full path from the project directory, so
   C++/WinRT's fallback activation (which looks for an already-loaded module) finds it in both
   the editor and packaged builds.
4. **Exception boundary.** The wrapper catches `winrt::hresult_error` and `std::exception` at its
   entry points (engine subsystem, input, output). Together with the fork's containment of
   failures inside libremidi, a WinRT failure disables MIDI with a logged error instead of
   crashing the process. This builds on the `/EHsc` setting of
   [ADR-0007](adr-0007-enable-cpp-exceptions-in-packaged-builds.md).
5. **libremidi pin.** The submodule is pinned to the fork tag `libremidi4ue-pin-inbox-20261004` on
   the fork branch `libremidi4ue-inbox` (topic branch `fix/winmidi-inbox-api`). This pin is
   separate from the pin on `main`. For this branch only, it supersedes the timing of
   [ADR-0003](adr-0003-libremidi-fork-strategy.md) rule 2 (in-box migration stays fork-only until
   the in-box ship): the in-box port is built now, on the branch, in the fork. The rest of ADR-0003
   stands, including the tag and branch rules and the date fallback as corrected by
   [ADR-0006](adr-0006-date-fallback-rationale-corrected.md).

### Consequences

- Good: Windows MIDI Services works again on the in-box Preview 10 API; `main` and its RC4 code
  are unaffected; the public repo carries no Microsoft binaries.
- Good: port names are unchanged from RC4 (verified), so existing port selections keep matching.
- Accept: the branch runs on Windows 11 25H2 and later only.
- Accept: every host that uses the branch must obtain the Preview 10 files itself and place them
  as in decision 2; a clone alone builds without the Windows MIDI Services backend.
- Accept: obligations for anyone who distributes a build that includes the preview files
  (go-live terms):
  - ship `Windows.Devices.Midi2.dll` and `.pri` together, and never `.pdb` files or installers;
  - disclose that the build uses a preview API;
  - remove the app-local binaries within 90 days of the in-box release.
- Accept: two libremidi pins (`main` and this branch) until the branch merges.
- Rejected bundling the files on `main` or on the branch: it redistributes Microsoft binaries from
  a public repository against Microsoft's stated preference, and puts preview binaries into
  `main`'s history.
- Rejected waiting for the in-box ship: Windows MIDI Services stays unusable on `main` until then.

### Removal plan (at the in-box ship)

1. Remove the `.dll` and `.pri` from the host project.
2. Regenerate the projection headers from the in-box metadata (or switch to the SDK's own).
3. Remove the preload in `StartupModule`.
4. Merge `preview/winmidi-inbox` into `main` and retire the separate libremidi pin.

### Confirmation

- A clone of the branch contains no Microsoft binaries or projection headers: no `.dll`, `.pri`,
  `.winmd` or `winrt/` header closure under `Source/ThirdParty/WindowsMidiServices`.
- With the host files present, the build log reports the projection found and the module log
  reports the DLL preloaded; with them absent, the build succeeds with the backend compiled out and
  says so.
- The submodule SHA on the branch is the commit that `libremidi4ue-pin-inbox-20261004` points to.
