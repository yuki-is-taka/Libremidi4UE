---
description: Libremidi4UE's runtime module sets bEnableExceptions and bDisableAutoRTFMInstrumentation, so Win64 Game (Development and Shipping) builds compile libremidi's throwing Windows MIDI Services and C++/WinRT code with /EHsc like the Editor; keeping libremidi out of public headers and an exception-to-UE-error boundary are deferred with triggers
type: decision
status: accepted
updated: 2026-10-03
---

# ADR-0007: Enable C++ exceptions in packaged builds

## Context and problem statement

Before 2026-10-03, Win64 Game builds (Development and Shipping) compiled Libremidi4UE with no `/EH`
switch and `_HAS_EXCEPTIONS=0`, while Win64 Editor builds compiled it with `/EHsc`. Unreal Build
Tool (UBT) turns exceptions on globally only for targets compiled against the editor, and a module
can only add them, never remove them (evidence below). Mac always compiles with `-fexceptions`. So
the exception-free configuration existed only in Windows ship builds, and no development surface
ever exercised it.

libremidi's Windows MIDI Services (winmidi) backend and C++/WinRT are written for exceptions: they
`throw`, `try` and `catch` by design, and every projected WinRT call raises
`winrt::hresult_error` when its ABI call fails. Under MSVC's default model (no `/EH`) that code
still compiles, and it compiled warning-free: C4530 ("C++ exception handler used, but unwind
semantics are not enabled") would have flagged it, but libremidi and C++/WinRT come in through
`/external:I`, which UE compiles at `/external:W0`. The same `winrt/base.h` compiled outside
`/external:I` emits C4530 at `base.h(5052)` (UE promotes C4530 to an error).

A probe program compiled with the packaged-build exception flags and the same bundled C++/WinRT
(2.0.250303.1) showed what that model does:

- `catch (winrt::hresult_error const&)` catches the failure, but the destructors of the locals in
  the frames in between do not run, so RAII state leaks (COM references, `hstring`).
- `catch (...)` around an access violation swallows it and the process continues. With `/EHsc` the
  same access violation kills the process.
- A WinRT exception crossing a `noexcept` function passes through to the caller. With `/EHsc` it
  calls `std::terminate`.
- `winrt::hresult_error` does not derive from `std::exception`, so libremidi's
  `catch (const std::exception&)` guards never see WinRT failures. This holds in both models.

Should the module that compiles the throwing dependency declare the exception model it needs,
instead of inheriting whatever the target happens to provide?

### Evidence (UE 5.8 Launcher install, `Engine/Source/Programs/UnrealBuildTool`)

- `Configuration/UEBuildTarget.cs:6181`:
  `bEnableExceptions = bForceEnableExceptions || (bCompileAgainstEditor && !bUseAutoRTFMCompiler)`.
  Exceptions are on for targets compiled against the editor unless the AutoRTFM compiler is in use;
  `bForceEnableExceptions` is a target-level override (`TestTargetRules.cs:314` sets it for test
  targets).
- `Configuration/UEBuildModuleCPP.cs:2672`: `Result.bEnableExceptions |= Rules.bEnableExceptions`.
  The module setting is OR-ed in, so a module can turn exceptions on but never off.
- `Configuration/UEBuildModuleCPP.cs:1822` and `:1932-1941`: a module whose exception setting
  differs from the shared PCH cannot reuse it, so UBT builds a `.Exceptions` shared-PCH variant.
- `Platform/Windows/VCToolChain.cs:436-451`: with exceptions on, `/EHsc` and
  `/DPLATFORM_EXCEPTIONS_DISABLED=0`; otherwise `_HAS_EXCEPTIONS=0` and
  `/DPLATFORM_EXCEPTIONS_DISABLED=1`, with no `/EH` at all.
- `Platform/Mac/MacToolChain.cs:149-156`: Mac always adds `-fexceptions`.
- `Core/Public/Windows/WindowsPlatformCompilerSetup.h:33` makes C4530 an error. UE's
  `THIRD_PARTY_INCLUDES_START` (`Microsoft/MSVCPlatformCompilerPreSetup.h:255-288`) does not touch
  4530; the silence comes from `/external:W0`.
- `HAL/Platform.h:210-211` defines `PLATFORM_EXCEPTIONS_DISABLED` and nothing in engine headers
  tests it, so flipping one module creates no UE-header ODR divergence. Game builds already ship
  with mixed modules.
- Engine precedent for a module that wraps a throwing third-party library:
  `Runtime/AudioCaptureImplementations/AudioCaptureRtAudio/AudioCaptureRtAudio.Build.cs`
  (`bEnableExceptions = true;`, then `bDisableAutoRTFMInstrumentation = true;` because AutoRTFM
  cannot be used with exceptions) and `Runtime/ImageWrapper/ImageWrapper.Build.cs:87-89` (the same
  pair). 32 engine `Build.cs` files set `bEnableExceptions = true`.

## Considered options

- Set `bEnableExceptions` in Libremidi4UE's runtime `Build.cs`, the module that owns the throwing
  dependency.
- Set it in every consumer module that includes Libremidi4UE's public header.
- Leave it as it was.

## Decision outcome

Chosen: set `bEnableExceptions = true` and `bDisableAutoRTFMInstrumentation = true` in
`Source/Libremidi4UE/Libremidi4UE.Build.cs`, because whether code needs exception semantics is a
property of the code, not of whether the target is an editor, and the module that compiles a
throwing dependency is the one that should declare that. This follows the engine's
`AudioCaptureRtAudio` / `ImageWrapper` pattern. It landed in commit `3db8a6f` as part of the
checkpoint-A U0 bump.

Verified on a Win64 Game build: `Libremidi4UE.Shared.rsp` carries `/EHsc` and
`/DPLATFORM_EXCEPTIONS_DISABLED=0`, `_HAS_EXCEPTIONS=0` is gone, and the module uses the
`.Exceptions` shared-PCH variant.

Deferred, each with its trigger and already a row in the [backlog](../backlog.md#follow-ups-from-checkpoint-a):

1. **Keep libremidi's implementation out of Libremidi4UE's public headers** (backlog row "Keep
   libremidi's implementation out of Libremidi4UE's public headers (exceptions follow-up 2)").
   `Public/LibremidiEngineSubsystem.h` includes `<libremidi/libremidi.hpp>`, and `libremidi.Build.cs`
   exports `LIBREMIDI_HEADER_ONLY`, so any module that includes the header compiles all backends,
   winmidi and C++/WinRT included, in its own translation units, without `/EH` unless that module
   also opts in. Trigger: the consumer-side hosting-point and private-dependency work that owns the
   consumer's Libremidi4UE include closure. Interim fact: the consumer's objects define 0
   functions in `libremidi::winmidi` today (only data symbols such as WinRT factory-cache entries
   and a few value-type helpers), so no mixed exception models are linked. That is incidental, not
   enforced: it holds only because the consumer does not odr-use backend code, and nothing at link
   time detects a mismatch (the linker keeps one COMDAT copy of an inline function arbitrarily).
2. **An exception-to-UE-error boundary around libremidi calls** (backlog row "Exception boundary:
   libremidi exceptions become UE errors (exceptions follow-up 3)"). With exceptions on,
   Libremidi4UE can catch `winrt::hresult_error` and `std::exception` around observer creation,
   enumeration, open and close, and turn each into a loud error plus a degraded state. Trigger: the
   U5 / U7 robustness units ([design §7](../design-winmidi-ump-fixes.md)). Failures that terminate
   inside libremidi's own `noexcept` functions can only be fixed in the fork.

### Consequences

- Good: Win64 ship builds of this module match the Editor. A WinRT failure unwinds properly,
  access violations crash loudly instead of being swallowed by `catch (...)`, and a `noexcept`
  violation terminates deterministically (before, only in the Editor).
- Good: the planned fork fixes U5 (catch `winrt::hresult_error` around session creation) and U7 (a
  `catch (...)` firewall in `MessagesReceived`) assume standard semantics. They now behave in
  shipped builds the way they behave in Editor testing.
- Good: Libremidi4UE's own `Private/*.cpp` can now contain `try`/`catch`; before, C4530 made that an
  error in Game builds.
- Accept: one extra shared-PCH variant (`.Exceptions`) per configuration for this module; unwind
  tables grow in this module only, with no cost on the non-throwing path (x64 table-based EH);
  AutoRTFM instrumentation is disabled for this module.
- Accept: no change on the Editor (already `/EHsc`) or on Mac (always `-fexceptions`); no UE-header
  ODR risk.
- Accept: this removes the Editor/ship divergence but does not make WinRT failures survivable.
  Surviving them needs the boundary (deferred 2) and the U5 fork fix; WinRT failures still bypass
  libremidi's `std::exception` guards in both models.
- Accept: until deferred 1 lands, a module that includes the public header still compiles libremidi
  and C++/WinRT without `/EH`. Setting the flag on Libremidi4UE alone is correct for this module
  and safe in the consumer today only for the incidental reason in deferred 1.
- Rejected exceptions in every consumer module: it spreads the exception model, gives each
  consumer its own `.Exceptions` PCH variant, depends on every future includer remembering the
  flag, and there is no link-time check when one forgets. It is the fallback if deferred 1 is
  postponed past the point where a consumer starts to emit backend code.
- Rejected leaving it as it was: ship behaviour then diverges silently from everything that is
  tested (leaked RAII state, access violations hidden as `io_error`, `noexcept` not enforced), and
  the one diagnostic that would say so (C4530) is suppressed.

### Confirmation

- A Win64 Game build's `Libremidi4UE.Shared.rsp` (Development and Shipping) contains `/EHsc` and
  `/DPLATFORM_EXCEPTIONS_DISABLED=0` and no `_HAS_EXCEPTIONS=0`. The `.rsp` files are under
  `Intermediate/Build/Win64/x64/UnrealGame/<configuration>/Libremidi4UE/`.
- `Libremidi4UE.Build.cs` keeps both settings, with the comment explaining why. Removing either
  needs a superseding ADR.
- Review dimension for any change to the module's public headers: does it add libremidi or
  C++/WinRT includes that reach modules without the exception setting (deferred 1)?
