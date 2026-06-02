# Windows MIDI Services for Unreal Engine

This directory contains the Windows MIDI Services (WinRT) integration for the Libremidi4UE plugin.

## Requirements

- Windows 10 (19H1 Build 18362) or later
- Windows 11 recommended for full MIDI 2.0 support
- Visual Studio 2022 with C++/WinRT support

## Directory Structure

```
WindowsMidiServices/
├── Win64/
│   └── include/
│       ├── winmidi/                          (MIDI Services SDK headers)
│       └── winrt/
│           ├── base.h                        (C++/WinRT runtime, v2.0.250303.1)
│           ├── Microsoft.Windows.Devices.Midi2*.h  (MIDI2 projections)
│           ├── Windows.*.h                   (minimal dependency set)
│           └── impl/                         (implementation headers)
├── Licenses/
│   ├── LICENSE.txt
│   └── THIRD_PARTY_NOTICES.txt
└── README.md (this file)
```

**Note**: Only the C++/WinRT headers required by libremidi and the MIDI Services SDK are
included. The full Windows SDK C++/WinRT projection is not bundled — all headers here were
generated with `cppwinrt.exe v2.0.250303.1` and must be kept as a matched set.

**Bundled SDK version**: Windows MIDI Services NuGet `1.0.17-rc.4.25` (rc-4, 2026-04-12).
See `Win64/include/winmidi/init/WindowsMidiServicesVersion.h` for the authoritative version
record. End-user machines must have the matching SDK Runtime installed
(`Windows.MIDI.Services.SDK.Runtime.and.Tools.1.0.17-rc.4.25-x64.exe`) — the runtime DLL is
not bundled.

## Usage

The WindowsMidiServices module is automatically included when building on Windows x64 platforms.

### Include Headers

```cpp
#if WITH_WINDOWS_MIDI_SERVICES
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#endif
```

## Regenerating Headers

If you need to update the MIDI Services SDK headers:

1. Install Windows MIDI Services SDK
2. Run `cppwinrt.exe` against the MIDI2 `.winmd` metadata to generate projection headers
3. Replace the contents of `Win64/include/winrt/` with the generated output
4. **Important**: All headers (including `base.h` and `Windows.*.h` dependencies) must come
   from the same `cppwinrt.exe` invocation to avoid version mismatches

### Extending the projection when bumping libremidi

Because this is a **minimal, hand-trimmed subset** (see the Note above), bumping the
libremidi submodule to a version whose `winmidi` backend uses a Windows MIDI Services
namespace that is not yet bundled will fail to compile with errors like
`'X': namespace does not exist` or `'Type' is not a member of
winrt::Microsoft::Windows::Devices::Midi2` — **even though the bundled SDK is current**.
The SDK does not need bumping; the *projection subset* needs extending.

To extend it (no SDK version change):

1. Find the winrt headers the new libremidi actually includes:
   `grep -rh '#include <winrt/' <submodule>/include/libremidi/backends/winmidi/`
2. Regenerate the MIDI2 namespaces with the **matching** cppwinrt version
   (`v2.0.250303.1`, must equal the `base.h` banner) and the rc-4 winmd:
   `cppwinrt.exe -input "<...>/Microsoft.Windows.Devices.Midi2.winmd" -ref 10.0.22621.0 -output <tmp>`
   (`-ref` references — does not regenerate — the Windows SDK, so keep the existing
   `Windows.*.h` + `base.h` as-is).
3. Copy only the Midi2-namespace files in the `#include` closure of the headers from
   step 1 into `Win64/include/winrt/` (public + matching `impl/*.0/1/2.h`).
4. If the closure pulls a **new** `Windows.*` dependency (e.g. `Windows.Data.Json`,
   required by the JSON-based `ServiceConfig` that `Endpoints.Virtual` depends on),
   generate it too: `cppwinrt.exe -input 10.0.22621.0 -include Windows.Data.Json -output <tmp>`.
5. Verify the bundle is self-contained (no dangling `#include "winrt/..."` paths).

History: the `Endpoints.Virtual`, `ServiceConfig`, and `Windows.Data.Json` headers were
added this way for libremidi's winmidi virtual-port support (#194).

## License

Windows MIDI Services is licensed under the MIT License. See `Licenses/LICENSE.txt` for details.

## References

- [Windows MIDI Services Documentation](https://learn.microsoft.com/en-us/windows/uwp/audio-video-camera/midi)
- [C++/WinRT Documentation](https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/)
