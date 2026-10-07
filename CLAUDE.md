# Libremidi4UE

Cross-platform MIDI 1.0 / MIDI 2.0 (UMP) for Unreal Engine, wrapping libremidi. Used by Thal's ThalMidi as the MIDI backend.

Git-managed (independent repo). Use `git`, not `p4`. Contains the libremidi submodule at `Source/ThirdParty/libremidi/libremidi`, pinned from the owner's fork named in `.gitmodules` (upstream `celtera/libremidi` plus carried fixes; branch `libremidi4ue`, immutable `libremidi4ue-pin-*` tags; ADR-0003). Never modify the submodule checkout: libremidi fixes go on topic branches in a separate fork clone, and every pinned SHA gets its pin tag pushed before the pin commit. Never open upstream PRs/issues — the owner files them. Local commits on your own feature branch are allowed. Committing to or merging into `main`, and every push, need explicit user approval each time (same policy as p4 submit). Landing procedure: root `CLAUDE.md`, "Landing a change".

## Notes
- Settings use `ULibremidiSettings` (UDeveloperSettings) — this plugin does not follow Thal's subsystem-as-config pattern.
- The non-obvious core is port identity / reconnection: `ContainerId`/`DeviceId` grouping and `FLibremidiPortInfo::FindClosestPort` (weighted re-match across hotplug). See README.

## Documentation
Read [`Docs/INDEX.md`](Docs/INDEX.md) before non-trivial work. Decisions (immutable): `Docs/decisions/`. This repo follows the project doc-system convention.
