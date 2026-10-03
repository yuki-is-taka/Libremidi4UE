---
description: winmidi input group addressing (input half) — every UMP of a COM receive batch gets its own verdict; a group-filtered port accepts the opened block's group range [first, first+count); groupless MT 0x0 / 0xF and reserved types are dropped on group-filtered ports; virtual ports deliver everything. The output half waits for the port-identity redesign
type: decision
status: accepted
updated: 2026-10-03
---

# ADR-0005: winmidi input group addressing

## Context and problem statement

On Windows MIDI Services, a libremidi input port is a block (a group terminal block or a function
block) of an endpoint, and it must deliver only the messages that belong to that block's groups.
The COM receive path, which Libremidi4UE's builds use, receives buffers that can hold several
UMPs; Microsoft's porting guide says a group of messages that arrived together stays together in
one callback.

Today libremidi reads the group from the first word of the buffer only and hands the whole buffer
to the decoder as one message (defect (g), upstream #234), and it compares a single group instead
of the block's range (defect (b)). Message types without a group field — MT 0x0 (Utility) and
MT 0xF (UMP Stream) — and the reserved types have no group in bits 27..24, so the current filter
passes or drops them arbitrarily. Evidence: [audit §3 (b), (g)](../audit-winmidi-defects-2026-10-03.md),
[probe §3.3](../audit-winmidi-probe-2026-10-03.md).

Which messages may a group-filtered input port deliver, decided the same way on both receive
paths?

## Considered options

- A verdict per batch, from the first word (today's behaviour).
- A verdict per message, with groupless and reserved types passed to every port.
- A verdict per message, with groupless and reserved types dropped on group-filtered ports and
  delivered on unfiltered ones.

## Decision outcome

Chosen: a verdict per message, dropping groupless and reserved types on group-filtered ports,
because it follows Microsoft's guidance ([ADR-0003](adr-0003-libremidi-fork-strategy.md) rule 9)
and the UMP specification, and it is the only option in which what a port delivers depends only
on messages that belong to that port.

1. **Per-message verdict.** The COM receive batch is walked by UMP size, using the UMP 1.1 sizes
   including the reserved types (upstream PR #264). Each UMP gets its own verdict and, if
   accepted, is delivered to the decoder as its own call. A truncated tail ends the walk. A size
   of 0 also ends the walk (the `n == 0` guard), so no change to the size table can make the
   callback loop forever. The service timestamp is converted once per batch and carried by every
   message of that batch. The WinRT receive path, already one message per event, applies the same
   verdict.
2. **Group range.** A group-filtered port accepts a group-bearing UMP (MT 0x1, 0x2, 0x3, 0x4, 0x5,
   0xD) when its group `g` satisfies `first <= g < first + count`, where `first` and `count` are
   the opened block's first group and group count.
3. **Groupless and reserved types are dropped on a group-filtered port.** Each has its own ground:
   - MT 0xF (UMP Stream): Microsoft's guidance. The porting guide says messages without a group
     must not be routed to a port, because they describe the whole endpoint.
   - MT 0x0 (Utility): the UMP specification. Since UMP 1.1, Utility messages carry no group.
   - Reserved types (0x6–0xC, 0xE): our forward-compatibility policy. Their bits 27..24 have no
     defined meaning, so a port cannot attribute them to a group. A future type that does carry a
     group gets a verdict when the specification defines it.
4. **Virtual ports deliver everything.** A port without a group filter (today: a virtual port)
   delivers every message, groupless and reserved types included.

### Output: interim rule, not decided here

The output half — which group a sent UMP carries — waits for the port-identity redesign
(fork-only until the in-box ship, ADR-0003 rule 2; tracked in the [backlog](../backlog.md)). There
a port becomes endpoint + group + direction, as Microsoft's model has it, and the outgoing group
is the port's group by construction.

Until then the fix batch applies an interim rule ([design](../design-winmidi-ump-fixes.md) U3):
- a group-bearing UMP whose group lies outside the opened block's range is re-stamped to the
  block's first group, and a group inside the range is kept (range membership; on a single-group
  block, which every MIDI 1.0 cable is, every message carries the port's group);
- MT 0x0, MT 0xF and reserved types are never re-stamped, and virtual ports are untouched;
- because block direction (defect (c)) is still inverted, the stamp is applied only if a
  host-to-device block of the endpoint covers the group; otherwise the message goes out unchanged
  and a warning is raised (guard for asymmetric devices).

That rule is interim: the redesign's output ADR replaces it rather than extending it.

### Consequences

- Good: every UMP of a batch is delivered, so SysEx and message bursts complete, and the decoder
  never receives more than one message per call, which removes the overrun at its cause (with
  #264's bounds as the second line).
- Good: one WinRT-free verdict function serves both receive paths and can be unit-tested anywhere.
- Accept: no libremidi consumer sees endpoint-discovery or function-block messages (MT 0xF) on a
  group-filtered port. An endpoint-level path is part of the port-identity redesign.
- Accept: `on_raw_data` fires once per delivered message instead of once per batch, so raw
  consumers see filtered data. Libremidi4UE's raw handler only logs.
- Accept: in `Relative` timestamp mode, the messages of one batch after the first get a delta of
  zero.
- Accept: jitter-reduction timestamps (MT 0x0) never reach a group-filtered port; libremidi's
  default `ignore_timing` dropped them anyway.
- Rejected the per-batch verdict: data loss and a stack overrun, both reproduced on hardware.
- Rejected passing groupless types: contradicts the porting guide, and an endpoint-wide message
  would appear on every port of the endpoint.

### Confirmation

- The WinRT-free dispatch tests in the fork ([design §4](../design-winmidi-ump-fixes.md)),
  including the mutation check: reverting the walk, the range compare or the groupless drop each
  turns a test red.
- Probe `recv-wms` and `recv-wms-ordered`: on every port, the message path's completed SysEx count
  equals the raw path's, the probe exits 0, and the ASan build reports nothing
  ([probe §4](../audit-winmidi-probe-2026-10-03.md)).
