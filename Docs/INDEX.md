---
description: Libremidi4UE documentation index; routes to every doc in this unit
type: reference
status: living
updated: 2026-10-03
---

# Libremidi4UE — documentation index

Read this first; it routes to every doc below. The authoritative API spec is the README and the source headers.

## Reference
| Doc | Open when |
|---|---|
| [../README.md](../README.md) | features, identifier fields, platform backends, FindClosestPort, device grouping |

## Decisions (immutable)
| ADR | Status | Decision |
|---|---|---|
| [adr-0001](decisions/adr-0001-libremidi-submodule-tracks-upstream.md) | superseded by adr-0003 | libremidi submodule tracks `celtera/libremidi` directly; fork retired once its winmidi patch merged upstream |
| [adr-0002](decisions/adr-0002-winmidi-timestamp-tick-correction.md) | superseded by adr-0004 | winmidi's raw-tick timestamp is corrected to true ns inside the Libremidi4UE wrapper, keyed on API + mode |
| [adr-0003](decisions/adr-0003-libremidi-fork-strategy.md) | accepted (date-fallback rationale amended by adr-0006) | libremidi is carried in the owner's fork: topic branches rebuilt into the `libremidi4ue` integration branch in a recorded order, tests in their own commits, fork `master` a pure mirror, immutable `libremidi4ue-pin-*` tags pushed before each pin, no UE code in the fork; leaf fixes are upstream PR candidates the owner files; in-box migration and port identity fork-only until the in-box ship (date fallback); drop-and-follow when upstream is equivalent; repoint to upstream when the delta is empty; UMP-only scope plus (q), (p); follow Microsoft's guidance |
| [adr-0004](decisions/adr-0004-winmidi-timestamp-correction-removed.md) | accepted | the ADR-0002 correction is deleted in the same commit as the bump past upstream `5c839a5`; a generic Windows-only WMS timestamp-domain cross-check is kept |
| [adr-0005](decisions/adr-0005-winmidi-input-group-addressing.md) | accepted | winmidi input: per-message verdict over the block's group range `[first, first+count)`; MT 0x0 / 0xF and reserved types dropped on group-filtered ports; virtual ports deliver everything. Output half deferred to the port-identity redesign (interim rule noted) |
| [adr-0006](decisions/adr-0006-date-fallback-rationale-corrected.md) | accepted | amends the ADR-0003 date-fallback rationale only: 2027-01-15 is the end of Microsoft's permitted distribution window for preview binaries (Preview 9 release notes), not a built-in expiry; the date stays; all preview components are removed once the in-box API ships. ADR-0003 date-fallback rationale amended by ADR-0006 |

## Design / WIP
| Doc | Status | Open when |
|---|---|---|
| [design-winmidi-ump-fixes.md](design-winmidi-ump-fixes.md) | current | before executing, reviewing or rebasing the 2026-10 winmidi UMP fix batch (checkpoint A: U0 baseline, #264, (q), U2 input dispatch, (p), U3 output group); the fork's recorded stack order; test strategy, probe verification, deferred units and triggers |
| [wip-winmidi-inbox-migration.md](wip-winmidi-inbox-migration.md) | investigation | before touching Windows MIDI port identity/enumeration; before the in-box API migration or the port-identity redesign (its §1 points to the 2026-10 decision; its §4 is a 2026-09-23 history superseded by the audit) |

## Audits and evidence (point-in-time records)
| Doc | Status | Open when |
|---|---|---|
| [audit-winmidi-defects-2026-10-03.md](audit-winmidi-defects-2026-10-03.md) | point-in-time record (2026-10-03) | before touching any winmidi defect (ids a-ac), planning the fork/upstream fix batch, or bumping the libremidi submodule; file:line, severity, fix sketch, dependency graph and fix order per defect |
| [audit-winmidi-probe-2026-10-03.md](audit-winmidi-probe-2026-10-03.md) | point-in-time record (2026-10-03) | when you need the hardware evidence (Push 3 over Windows MIDI Services, pin vs upstream HEAD) behind defects (c), (d), (g), (j), (i), or the pass criteria for a fixed libremidi |
| [audit-winmidi-probe-cpA-2026-10-03.md](audit-winmidi-probe-cpA-2026-10-03.md) | point-in-time record (2026-10-03) | the checkpoint-A hardware verification (probe label `cpA`): (d), (g), (i), (p) rows passing, (c) still defective by design; the fork's new unit tests under MSVC |
| [audit-winmidi-upstream-2026-10-03.md](audit-winmidi-upstream-2026-10-03.md) | point-in-time record (2026-10-03) | before filing, commenting on or tracking anything on upstream `celtera/libremidi` (issues/PRs sweep, Microsoft's in-box migration and porting-guide asks, upstream-only items N1-N10) |

## Backlog
Open future work + deferrals: [`Docs/backlog.md`](backlog.md) — this unit's single TODO ledger.
