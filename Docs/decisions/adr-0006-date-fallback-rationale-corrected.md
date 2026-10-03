---
description: Corrects the rationale of ADR-0003's date fallback (2027-01-15) — the date is the end of Microsoft's permitted distribution window for preview binaries, not a built-in expiry of the preview; the date stays; all preview components are removed once the in-box API ships
type: decision
status: accepted
updated: 2026-10-03
amends: adr-0003
---

# ADR-0006: Date-fallback rationale corrected (amends ADR-0003)

## Context and problem statement

[ADR-0003](adr-0003-libremidi-fork-strategy.md) Decision outcome rule 2, the "Date fallback" item,
sets 2027-01-15 as the date on which the port-identity redesign starts even if the in-box Windows
MIDI Services API has not shipped. Its rationale says that 2027-01-15 is the date "the in-box
developer Preview 9 self-expires". That is wrong. Microsoft's release notes for in-box developer
Preview 9 (tag `inbox-dev-preview-9` of `microsoft/MIDI`, published 2026-09-21) do not say the
preview binaries expire. Their Customer Preview terms say the WinRT API binaries may be
distributed with an alpha/beta/preview version of an app as long as that app has an enforced
expiration date of no later than 2027-01-15, and that the app clearly indicates it uses
pre-release code. The date is a limit on distribution, not an expiry built into the binaries.

Does the date still belong in the fallback, and on what ground?

## Considered options

- Keep 2027-01-15 and correct its rationale.
- Drop the date and make the redesign wait for the in-box ship however long that takes.
- Move the date to another one.

## Decision outcome

Chosen: keep 2027-01-15 and correct its rationale, because the date is a real boundary in
Microsoft's terms and the corrected rationale supports it as well as the wrong one did.

1. **The fact.** The Preview 9 release notes (`inbox-dev-preview-9`) permit distributing the
   preview WinRT API binaries only with an alpha/beta/preview app that enforces its own expiration
   no later than 2027-01-15. The preview binaries carry no built-in expiry.
2. **The date stays, with a new rationale.** 2027-01-15 is the end of Microsoft's permitted
   distribution window for preview binaries. If the in-box API has not shipped by then, a build
   that depends on the preview binaries can no longer be distributed under those terms, and the
   in-box schedule has slipped materially. The port-identity redesign then starts anyway, on the
   API generation current at that time, as ADR-0003 rule 2 already says.
3. **Policy: remove all preview components once the in-box API ships.** When the official Windows
   MIDI Services ships in Windows, every preview component is removed from the unit: the bundled
   preview projection and headers, and any side-by-side preview `.dll`/`.pri` arrangement. A
   preview is a bridge to the ship, not a supported target
   ([wip-winmidi-inbox-migration.md](../wip-winmidi-inbox-migration.md) §2).
4. **Unchanged.** The rest of ADR-0003 rule 2 stands: the redesign's trigger (the in-box ship), the
   fork-only status of the redesign until then, and the rule that the date is a schedule
   parameter kept in the backlog's port-identity row, where moving it is a backlog edit.

### Consequences

- Good: the fallback rests on a stated term of Microsoft's release notes instead of an inference
  that the notes do not support.
- Good: the removal policy gives the migration a defined end state with no preview leftovers.
- Accept: the corrected rationale is weaker as a signal. A built-in expiry would have forced the
  issue at the date; a distribution limit binds only if we distribute preview binaries, so the
  date is a decision point, not a deadline imposed on the code.
- Rejected dropping the date: an unbounded wait leaves the port-identity redesign, and the
  defects it fixes by construction, with no end.
- Rejected moving the date: 2027-01-15 is the one date Microsoft's own terms name.

### Confirmation

- No doc other than ADR-0003 (immutable, kept as the historical record) claims a preview expiry
  of the binaries themselves: a search of `Docs/` for the wrong wording finds only ADR-0003 and
  this ADR's quotation of it in its context section.
- The backlog's port-identity row cites this ADR for the date's rationale.
