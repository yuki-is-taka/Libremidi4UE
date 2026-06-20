# Libremidi4UE

Cross-platform MIDI 1.0 / MIDI 2.0 (UMP) for Unreal Engine, wrapping libremidi. Used by MABOROSHIFramework's MFMidi as the MIDI backend.

Git-managed (independent repo). Use `git`, not `p4`. Contains a third-party submodule at `Source/ThirdParty/libremidi/libremidi` (upstream `celtera/libremidi`) — do NOT modify files inside it. Never commit without explicit user approval.

## Notes
- Settings use `ULibremidiSettings` (UDeveloperSettings) — this plugin does not follow MABOROSHIFramework's subsystem-as-config pattern.
- The non-obvious core is port identity / reconnection: `ContainerId`/`DeviceId` grouping and `FLibremidiPortInfo::FindClosestPort` (weighted re-match across hotplug). See README.

## Documentation
Read [`Docs/INDEX.md`](Docs/INDEX.md) before non-trivial work. Decisions (immutable): `Docs/decisions/`. This repo follows the project doc-system convention.
