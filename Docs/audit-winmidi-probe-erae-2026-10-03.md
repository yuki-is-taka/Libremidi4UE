---
description: >
  Hardware check of a SysEx-heavy USB controller (Erae 2, API mode) over Windows MIDI Services with the checkpoint-A libremidi pin (6a572eb) plus a probe-local raw-callback hook. Every service callback carried exactly one packet (a 29-byte finger message arrives as 11 callbacks, never as one 10-word batch), so the batch-size defects (reserved-size overflow, batch loss) are not exposed to this device on this path; the handshake and 9,059 finger messages came through the fixed libremidi intact and in order.
type: reference
status: point-in-time record (2026-10-03)
updated: 2026-10-03
---

# Erae 2 on Windows MIDI Services: delivery shape and integrity (probe label `erae`)

Date: 2026-10-03. Companion to the [checkpoint-A record](audit-winmidi-probe-cpA-2026-10-03.md);
the harness, staging and build recipe are the ones of the [first probe record](audit-winmidi-probe-2026-10-03.md)
(§1, §2), with one addition: a probe-local hook that sees the raw words of each service callback
before libremidi does. The design doc left this physical-input check open ([design §5](design-winmidi-ump-fixes.md),
"Physical input": whether a 29-byte SysEx per touch arrives as one 10-word batch was unverified).

## 0. Results

| Question | Result | Verdict |
|---|---|---|
| Handshake (version request, then a 128-zone boundary scan) | version reply intact (5 data bytes) in about 2-4 ms; 128 zones asked, 128 replies intact (7 data bytes each), none missing, late or duplicated; scan complete; RTT min/avg/max 1.9 / 3.4 / 5.1 ms. Run 1 (no touches) gave the same handshake result | **PASS** |
| Q1: delivery shape of a finger message | every raw callback carried exactly one packet; the largest callback was 2 words; 105,637 callbacks in 60 s. A 29-byte finger message arrived as 11 SysEx7 packets (Start, 9 Continue, End) in 11 separate callbacks, 1-8 us apart. No finger message ever arrived as one 10-word batch | **the 10-word-batch assumption does not hold** for this device on this path |
| Q2: integrity through the fixed libremidi | 9,059 finger messages at about 149/s, all 29 bytes, checksum ok 9,059 / bad 0, zero reassembly anomalies; the probe's reassembly of the raw callback words and libremidi's `on_message` output agree in count and order (9,188 SysEx each: the 9,059 finger messages, 128 zone replies, 1 version reply) | **PASS** |
| Lifecycle (Begin / Slide / Release) | Begin 207, Slide 8,646, Release 206; 2 fingers still down at the end. One Slide without a preceding Begin (finger 3), see §2 | PASS with the artifact excluded |

**Bottom line:** with this device (Erae 2, USB, one firmware), on this path (native MIDI 1.0
bytes, translated to UMP by Windows MIDI Services, input port group 0, one service version), the
service delivered this device's SysEx as one UMP packet per callback, so the dispatcher never saw
a batch longer than 2 words. The defects that need a larger single batch (the reserved-size overflow behind (g)'s
process kill, fixed by U1; the loss of all but the first message of a batch, fixed by U2) are
therefore not triggered by this device's input as delivered here. That is consistent with no
failure ever having been observed with this device on Windows before the fixes.

## 1. Setup

Erae 2 over USB, in its API mode (SysEx finger stream). The device sends native MIDI 1.0 bytes;
Windows MIDI Services translates them to UMP, and the endpoint's input port is group 0. The probe
opened that input through the checkpoint-A pin (`6a572eb`, tag `libremidi4ue-pin-20261003-a`, release
build) and also recorded the raw words of every service callback from a probe-local hook. It sent
a version request, an API-mode disable/enable, and 128 zone-boundary requests; a person touched
the surface for the 60 s of the measurement. Two runs: run 1 without touches (handshake only), run 2 as described.
Run on the Windows test machine; no other device, firmware or service configuration was tried.

## 2. Evidence

**Delivery shape (Q1).** Over the 60 s of run 2 the hook saw 105,637 raw callbacks. Each carried
one packet; the largest was 2 words. One finger message (29 data bytes, 31 on the wire with the
framing bytes) arrived as 11 SysEx7 packets, Start + 9 Continue + End, with at most 3 data bytes per
packet (about 2.6 on average), each in its own callback, consecutive callbacks 1-8 us apart. The
packet sizes mirror the 3-byte event packets of USB-MIDI 1.0. The earlier
assumption (29 bytes = 5 SysEx7 packets of up to 6 bytes = 10 words in one batch) came from the
size of the message, not from a measurement, and it does not describe this path.

**Integrity (Q2).** Both reassemblies (the probe's own, from raw callback words; libremidi's, from
`on_message`) produced 9,188 SysEx in the same order. Finger messages: 9,059, every one 29 data
bytes with a valid checksum (bad 0), zero reassembly anomalies. Actions: Begin 207, Slide 8,646, Release 206.

**The one lifecycle exception.** The single Slide without a Begin (finger 3) was the very first
finger message, 9 ms after API-mode enable: a finger was already resting on the surface when API
mode was switched on (the procedure had a hand resting on it). It is a test-procedure artifact,
not a loss. With it excluded the lifecycle check passes.

**Load (a fact for any future input-cost work).** One continuously touched Erae produced about
1,760 callbacks per second, one cross-process callback per packet. Nothing in this run was
affected by that rate.

**Side observation.** The device also sent single-word system packets resembling MIDI clock, about 48
per second, on the same port. Harmless.

## 3. Scope and limits

- One device, one firmware, one Windows MIDI Services version, input only. Another firmware, another
  service version, or another device that lets the service coalesce packets can deliver larger batches;
  the fixed libremidi handles those (U1, U2, unit-tested at 4, 6, 8 and 10 words and in the
  hardware record of checkpoint A), but this record does not prove it for such a device.
- The pre-fix pin was not run against this device. Its non-exposure follows from the measured
  callback shape and the defects' trigger conditions (a batch longer than 6 words; more than one
  message per batch), and from the absence of any failure with this device before the fixes.
- Not tested: the output direction under large payloads (for example an image-drawing SysEx sent to
  the device). The probe's own outbound traffic (version request, API-mode disable/enable, 128
  zone requests) all worked.

## 4. State left behind

- No probe process left running.
- The run's logs and the probe tree stay on the test machine, outside this repository, next to the
  earlier records' material.
