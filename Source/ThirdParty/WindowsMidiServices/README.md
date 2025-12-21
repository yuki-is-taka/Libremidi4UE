# Windows MIDI Services for Unreal Engine

This directory contains the Windows MIDI Services (WinRT) integration for the Libremidi4UE plugin.

## Requirements

- Windows 10 (19H1 Build 18362) or later
- Windows 11 recommended for full MIDI 2.0 support
- Visual Studio 2022 with C++/WinRT support

## Directory Structure

```
WindowsMidiServices/
??? Win64/
?   ??? include/winrt/
?   ?   ??? Microsoft.Windows.Devices.Midi2.h
?   ?   ??? Microsoft.Windows.Devices.Midi2.Messages.h
?   ?   ??? base.h
?   ?   ??? winrt.ixx
?   ?   ??? impl/
?   ?       ??? *.h (implementation headers)
?   ??? metadata/
?       ??? Microsoft.Windows.Devices.Midi2.winmd
??? Licenses/
?   ??? LICENSE.txt
?   ??? THIRD_PARTY_NOTICES.txt
??? README.md (this file)
```

## Usage

The WindowsMidiServices module is automatically included when building on Windows x64 platforms. 

### Include Headers

```cpp
#if WITH_WINDOWS_MIDI_SERVICES
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#endif
```

## Setup Instructions

1. Install Windows MIDI Services SDK
2. Generate C++/WinRT projection headers
3. Copy generated headers to `Win64/include/winrt/`
4. Optionally copy metadata files to `Win64/metadata/`

## License

Windows MIDI Services is licensed under the MIT License. See `Licenses/LICENSE.txt` for details.

## References

- [Windows MIDI Services Documentation](https://learn.microsoft.com/en-us/windows/uwp/audio-video-camera/midi)
- [C++/WinRT Documentation](https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/)
