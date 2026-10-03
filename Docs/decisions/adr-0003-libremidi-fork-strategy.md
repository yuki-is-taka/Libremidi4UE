---
description: libremidi is carried in the owner's fork — per-unit topic branches rebuilt into an integration branch, immutable tagged pins, leaf fixes offered upstream by the owner only, drop-and-follow when upstream is equivalent, repoint to upstream when the carried delta is empty
type: decision
status: accepted
updated: 2026-10-03
supersedes: adr-0001
---

# ADR-0003: libremidi fork strategy

## Context and problem statement

[ADR-0001](adr-0001-libremidi-submodule-tracks-upstream.md) pointed the submodule at
`celtera/libremidi` because the fork then carried no delta, and recorded that a patch needed
before upstream takes it "must re-establish a fork" through a superseding ADR. That case has
arrived.

The 2026-10-03 [defect audit](../audit-winmidi-defects-2026-10-03.md) and
[hardware probe](../audit-winmidi-probe-2026-10-03.md) show that libremidi's Windows MIDI
Services (winmidi) backend, on the current pin and on upstream `master` `5c839a5`:

- sends every output to the first cable of a multi-cable device (defect (d));
- loses multi-packet SysEx and message bursts, and overruns a stack object on any receive batch
  longer than six words, which stalls input and then kills the process in an `/O2 /GS` build
  (defect (g), upstream #234).

Neither is fixed upstream. The reserved-size fix that removes the overrun is an open contributor
PR (#264). Microsoft's in-box `Windows.Devices.Midi2` API replaces the bundled out-of-band SDK at
the end of November 2026, and some defects (port identity, one connection per endpoint) should be
fixed once, against that API, not twice.

The questions: where do fixes live while upstream does not have them, how do they stay separable
for upstream, and when does the fork end?

## Considered options

- Wait for upstream and carry nothing (ADR-0001's state).
- Patch the submodule checkout, or compensate in the wrapper.
- A fork with one long-lived branch that merges upstream periodically, fixes committed on top.
- A fork whose fixes live on per-unit topic branches, with an integration branch rebuilt from
  them and immutable tagged pins.

## Decision outcome

Chosen: per-unit topic branches in the owner's fork, rebuilt into an integration branch, because
it is the only option that both ships the fixes now and keeps every fix separable — as an
upstream PR candidate, and as a unit that can be dropped the day upstream has an equivalent.

### Rules

1. **Where fixes live.** Fixes to libremidi are made in the owner's fork, in a clone separate from
   the submodule checkout. Never in the submodule checkout, and never as wrapper-side compensation
   for a libremidi defect (ADR-0002 was the last of those; [ADR-0004](adr-0004-winmidi-timestamp-correction-removed.md)
   retires it).
2. **Upstream.** Small leaf fixes are upstream PR candidates, kept PR-ready on their topic
   branches. The owner files every upstream PR, issue and comment personally; agent tooling never
   opens them, and a PR text is drafted only on request. Large parts stay fork-only until Windows
   ships Windows MIDI Services in-box (expected end of November 2026): the in-box API migration
   (upstream #252), the port-identity redesign (defect (a), upstream #254, together with (c)
   block direction and the output half of the group policy, [ADR-0005](adr-0005-winmidi-input-group-addressing.md)),
   and one connection per endpoint (defect (h)). The interim output group stamp for defect (d) is
   fork-only as well, because it is a port-model policy; only its one-line converter-group fix is
   a leaf candidate.
   - **Date fallback.** If the in-box API has not shipped by 2027-01-15, the port-identity
     redesign starts anyway, on the API generation current then. 2027-01-15 is the date the
     in-box developer Preview 9 self-expires, so a slip past it means Microsoft's schedule has
     moved materially. The date is a schedule parameter kept in the backlog's port-identity row;
     moving it is a backlog edit, not a new ADR.
3. **Drop-and-follow.** When upstream lands a fix for a defect we carry, we drop our fix and
   follow upstream's if it is *equivalent*: our unit's tests and the unit's probe rows (the pass
   criteria in the probe record) pass on upstream's code. Tests may be adapted in how they reach
   the code under test, never in what they assert. The tests survive the drop: the unit's tests
   commit stays carried, rebased onto upstream, until upstream has equivalent coverage. If
   upstream's fix is not equivalent, our unit stays, rebased onto upstream's fix, carrying only
   the difference.
4. **Exit, and acceptance of permanence.** When the carried delta is empty (every unit dropped or
   merged upstream, its tests included), the submodule is repointed at `celtera/libremidi` by a
   new ADR, as ADR-0001 did. The fork may also turn out to be permanent — a fix upstream declines,
   a port model upstream does not adopt — and that is accepted. Re-evaluate the fork when:
   - upstream declines a PR candidate, or leaves a filed PR without a maintainer response for
     three months;
   - an upstream sync makes a carried unit conflict so badly that it must be re-derived rather
     than rebased;
   - upstream lands its own in-box migration or port model;
   - Microsoft's guidance changes on a point a carried unit implements;
   - Libremidi4UE stops using the winmidi backend.
5. **Branch model.**
   - Fork `master` is a pure mirror of upstream `master`: fast-forward only, never committed to.
   - Topic branches are the source of truth. Each fix unit is one topic branch, based on upstream
     `master` or on another topic it declares as its prerequisite.
   - Each unit's tests are in their own commit, separate from the fix, so that the fix can be
     dropped while the tests stay.
   - The integration branch `libremidi4ue` is rebuilt from the topics in a recorded order — the
     stack table of the governing design doc, today
     [design-winmidi-ump-fixes.md](../design-winmidi-ump-fixes.md) §2. A fix is never written
     first on the integration branch.
   - An upstream sync: fast-forward `master`; rebase each topic; drop the units upstream has made
     equivalent (`git cherry` / `git range-diff`); rebuild `libremidi4ue`; compare it with the
     previous pin using `git range-diff`.
   - `.gitmodules` names the fork with no `branch =` line. The pinned SHA is all a clone needs,
     and a branch line invites `git submodule update --remote` to move the pin silently.
6. **Pin hygiene.** Rebuilding the integration branch orphans earlier SHAs — ADR-0001's failure
   mode (a recorded SHA the advertised URL cannot serve). Therefore:
   - every SHA Libremidi4UE pins gets an immutable tag `libremidi4ue-pin-YYYYMMDD[-n]` on the fork,
     pushed BEFORE the Libremidi4UE commit that pins it;
   - tags are never moved or deleted;
   - no fork branch is deleted while a pinned SHA is reachable only from it, unless that SHA is
     tagged first.

   The historical pin `e16efb6` (Libremidi4UE `7ae6bff`, `2346402`, `d1dfca4`) had become
   unreachable on the fork; the retroactive tag `libremidi4ue-pin-20260602` was pushed for it, so
   those Libremidi4UE commits clone again.
7. **No UE-specific code in the fork.** The fork is plain libremidi: everything in it builds and
   tests with libremidi's own CMake and could be offered upstream. Unreal glue (the wrapper, the
   `*.Build.cs` files) stays in Libremidi4UE.
8. **Scope.** The fork carries UMP-path fixes: the winmidi COM receive path, winmidi `send_ump`,
   the winmidi observer, and the shared UMP decoder and segmenter. MIDI 1 byte-path and WinMM
   defects are recorded in the [backlog](../backlog.md), not fixed, with two exceptions included
   by owner decision: (q), the bounds of cmidi2's UMP → MIDI 1 SysEx stack buffer, and (p), SysEx
   reassembly for MIDI 1 inputs on winmidi. Both are small leaf fixes, and (p) keeps the
   per-message input fix from turning "nothing delivered" into "corrupt SysEx delivered" for
   MIDI 1 inputs.
9. **Follow Microsoft's guidance.** Wherever Microsoft's Windows MIDI Services guidance (the
   porting guide and the SDK documentation) covers a point, the fork follows it. ADR-0005 applies
   this to group addressing. It also governs the deferred work: one connection per endpoint and
   one session named after the host application, 100 ns absolute timestamps, and the endpoint
   watcher instead of polling.

### Consequences

- Good: the defects that block multi-cable sends and SysEx on Windows get fixed now, on a stack
  that can be rebuilt after any upstream sync.
- Good: every unit stays separable. An upstream PR is the topic branch rebased onto upstream
  `master`; a drop is removing the topic from the stack order.
- Good: every Libremidi4UE commit keeps cloning, because its pin is tagged.
- Rejected waiting for upstream: the overrun is a process kill on ordinary device traffic, and
  #234 is stalled.
- Rejected local patches: wrapper-side compensation cannot fix an overrun inside libremidi, and a
  modified submodule checkout is a pin no clone can reproduce.
- Rejected one merge-based branch: fixes intermix with upstream merges, so no unit can be offered
  upstream or dropped on its own.
- Accept: every upstream sync costs a rebase of each topic plus a test and probe re-run.
- Accept: the fork may be permanent (rule 4).
- Accept: Libremidi4UE depends on a fork the owner hosts; its pins stay fetchable only while the
  fork and its tags exist.

### Confirmation

- `.gitmodules` names the fork and has no `branch =` line (from the baseline commit, U0, on).
- Before every Libremidi4UE commit that changes the pin, `git ls-remote --tags` on the fork lists
  a `libremidi4ue-pin-*` tag pointing at the new SHA.
- Fork `master` is an ancestor of upstream `master` (`git merge-base --is-ancestor`).
- `git diff <upstream master>..libremidi4ue` touches no Unreal-specific code.
- Every upstream sync re-runs each carried unit's tests and the probe rows of the governing design
  doc. A unit is dropped only on rule 3's evidence, recorded in its backlog row.

## State at the time of writing (2026-10-03)

The submodule still pins upstream `67e8ccd` through `celtera/libremidi`; the switch to the fork
lands with the baseline commit U0 of [design-winmidi-ump-fixes.md](../design-winmidi-ump-fixes.md).
The retroactive tag `libremidi4ue-pin-20260602` exists on the fork. The integration branch does
not exist yet; fork `master` carries no delta and is behind upstream. The stale fork branches
whose patches upstream has merged are to be deleted (owner decision), subject to rule 6.
