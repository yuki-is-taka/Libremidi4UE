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

## License

Windows MIDI Services is licensed under the MIT License. See `Licenses/LICENSE.txt` for details.

## References

- [Windows MIDI Services Documentation](https://learn.microsoft.com/en-us/windows/uwp/audio-video-camera/midi)
- [C++/WinRT Documentation](https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/)
