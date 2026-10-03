---
description: The ADR-0002 winmidi tick-to-ns correction is deleted in the same commit as the submodule bump past upstream 5c839a5 (which converts upstream); a generic Windows-only WMS timestamp-domain cross-check is kept, without ADR-0002 references
type: decision
status: accepted
updated: 2026-10-03
supersedes: adr-0002
---

# ADR-0004: Remove the winmidi timestamp correction with the upstream bump

## Context and problem statement

[ADR-0002](adr-0002-winmidi-timestamp-tick-correction.md) converts winmidi input timestamps from
raw QPC ticks to nanoseconds inside the wrapper, because libremidi's winmidi `to_ns()` returned
the raw ticks (defect (i)). Upstream fixed (i) in `5c839a5` (PR #263, merged 2026-09-28):
`to_ns()` now returns true nanoseconds. The probe confirms it: on the pin `67e8ccd` the delivered
timestamp equals the QPC tick count; on `5c839a5` it equals ticks × 100
([probe §3.6](../audit-winmidi-probe-2026-10-03.md)).

The baseline step of the fix batch ([ADR-0003](adr-0003-libremidi-fork-strategy.md);
[design](../design-winmidi-ump-fixes.md) U0) moves the pin past `5c839a5`. With the correction
still in place, the wrapper would scale already-converted nanoseconds by 1e9 / QPC frequency once
more — 100× too large at the usual 10 MHz — and every consumer that measures time between messages
(timeouts, tempo, gesture tracking) would misbehave.

## Considered options

- Remove the correction in the bump commit, and delete the runtime cross-check with it.
- Remove the correction in the bump commit, and keep the cross-check, generalised to a
  timestamp-domain check.
- Keep the correction and make it detect whether a value is already in nanoseconds.
- Remove the correction in a follow-up commit.

## Decision outcome

Chosen: remove the correction in the bump commit and keep the cross-check, generalised, because
the correction's only purpose ended upstream, while the check's purpose — noticing at runtime
that Windows MIDI Services timestamps are not in the domain the wrapper's public contract
promises — outlives this one defect.

Removed, in the same Libremidi4UE commit as the bump:
- the tick-to-ns conversion in `ULibremidiInput::HandleMessage` and `HandleUmpMessage`, and the
  API/mode predicate that selected it (`NeedsWinmidiTickCorrection`) with its matrix test;
- every source comment, log text and test name that cites ADR-0002.

Kept, as a generic Windows-only "WMS timestamp domain" cross-check:
- on the first message of an input whose resolved API is Windows MIDI Services and whose
  timestamp mode is `Absolute` (or `Custom`, whose `get_timestamp` is the identity), compare the
  delivered timestamp with QPC-now expressed in nanoseconds; if they differ by more than a few
  seconds, log one `Error` saying the WMS timestamps are not in the expected domain;
- the QPC frequency snapshot taken at `Initialize`, and the overflow-safe tick-to-ns helper the
  check needs to express QPC-now in nanoseconds, with that helper's pure-function test.

`Relative` mode stays unchecked: its values are deltas, not instants.

### High-consequence-change gate

Deleting the correction removes something a prior ADR deliberately established, so the gate
applies.

- **Purpose.** The correction is a compensating shim for one upstream defect, not a capability.
  The capability it served — true-nanosecond timestamps, the contract in `LibremidiMessage.h`
  ("Timestamp (nanoseconds)") — is now provided by upstream and is preserved. The cross-check
  serves a different and durable purpose, detecting a timestamp-domain mismatch on any machine at
  runtime, so it is kept.
- **Requirement.** ADR-0002 requires this removal itself: a bump past an upstream fix "must
  re-check `to_ns()`'s return value and delete this correction in the same change". No document
  requires the correction beyond that point, and no deferred increment depends on it.
- **Refute.** Two adversarial reviews of the fix plan (2026-10-03) argued the case for keeping.
  The plan had proposed deleting the cross-check too, on the ground that every upstream sync
  re-runs the probe's timestamp row. The reviews held that the probe runs only at pin bumps, on
  one machine, while the check runs in every run on every machine and would catch a domain change
  the probe cannot see (a different SDK runtime, the in-box API, a regression upstream). The check
  is kept for that reason. No argument survived for keeping the correction itself: with upstream
  converting, it can only double-convert.

### Consequences

- Good: timestamps are correct nanoseconds after the bump without wrapper arithmetic, and
  ADR-0002's assumption that `Custom` mode's `get_timestamp` stays the identity no longer matters
  for correctness.
- Good: a later timestamp regression or domain change on Windows MIDI Services surfaces as one
  loud log line in any run.
- Rejected deleting the check: it would leave only the periodic, single-machine probe row.
- Rejected a self-detecting correction: guessing ticks versus nanoseconds from magnitude is the
  cross-check's comparison turned into silent repair, which hides the very signal the check
  exists to raise, and keeps wrapper arithmetic for a defect that no longer exists.
- Rejected a follow-up commit: every commit between the bump and the removal would ship
  timestamps 100× too large.
- Accept: the check is a log line, not a build failure. The probe's timestamp row on every pin
  bump stays the hard gate.

### Confirmation

- Probe timestamp row on every pin bump: the delivered timestamp ≈ QPC-now × 100, i.e. nanoseconds
  ([probe §3.6](../audit-winmidi-probe-2026-10-03.md)).
- The WMS timestamp-domain check stays silent in the checkpoint-A run on Windows.
- After the bump commit, no file under `Source/` mentions ADR-0002.
