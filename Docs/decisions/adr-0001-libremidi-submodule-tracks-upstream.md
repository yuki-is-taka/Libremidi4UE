---
description: The libremidi submodule tracks celtera/libremidi directly; the fork is retired now that its only patch is merged upstream
type: decision
status: accepted
updated: 2026-07-17
---

# ADR-0001: libremidi submodule tracks upstream directly

## Context and problem statement

The submodule at `Source/ThirdParty/libremidi/libremidi` pointed at a personal fork
(`yuki-is-taka/libremidi`) rather than upstream `celtera/libremidi`. The fork existed for one
reason, recorded in `7ae6bff`: to carry a winmidi input fix that upstream had not yet taken.

Upstream has since merged that fix as `e8270d5` ("winmidi: fix input dropped when GTB Number()
!= group index"), byte-identical to the fork's `e16efb6`. The fork therefore carries no unique
delta, while its `master` sits 15 commits behind upstream. Keeping the submodule on the fork now
means the fork must be synced by hand on every upstream update, or the recorded SHA will not
exist in the URL that `.gitmodules` advertises — a broken clone.

## Considered options

- Sync the fork's `master` from upstream on each update, keeping the fork in the submodule path.
- Repoint `.gitmodules` at `celtera/libremidi` and drop the fork from the submodule path.

## Decision outcome

Chosen: repoint at `celtera/libremidi`, because the fork was a carrier for an unmerged patch and
that patch is now upstream. With no delta left, the fork is a relay point whose purpose has ended;
maintaining it would be maintenance without a reason.

### Consequences

- Good: upstream updates are a plain `git fetch` + SHA bump — no fork-sync step that, if skipped,
  silently publishes an unresolvable submodule SHA.
- Good: `libremidi.Build.cs` no longer needs its manual-patch warning. Upstream's
  `backends/winmidi/helpers.hpp` now has only the `winrt::`-prefixed
  `using namespace winrt::Windows::Devices::Enumeration;`; the duplicate unprefixed line the
  warning told the reader to delete is gone. The comment was retired with this change.
- Rejected fork-sync: it preserves a hop that carries nothing, and makes correctness depend on a
  manual step performed out of band from the update itself.
- Accept: a future patch we need before upstream takes it must re-establish a fork and revert this
  ADR's URL choice. That is a deliberate, visible act, which is the point — the fork should exist
  only while it carries something.

### Confirmation

`.gitmodules` names `celtera/libremidi`, and the recorded submodule SHA is an ancestor of that
repository's `master` (`git merge-base --is-ancestor`). Any future need for a fork arrives as a
superseding ADR, not a silent URL edit.

## Verification status at time of writing

The submodule was advanced to `67e8ccd` (v5.4.2-62) and the Mac `MABOROSHIEditor` target builds
clean. Note that the winmidi backend this ADR's history revolves around is **Windows-only and was
not compiled** for this change; the Windows ship-gate build and an on-device MIDI check remain
outstanding. Upstream's 19 new commits include Windows-affecting changes (winmm port-close, and
`__declspec(dllexport)` widened beyond MSVC).
