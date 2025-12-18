// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class libremidi : ModuleRules
{
	public libremidi(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		// libremidi submodule
		string IncludePath = Path.Combine(ModuleDirectory, "libremidi", "include");
		PublicSystemIncludePaths.Add(IncludePath);

        // libremidi requires C++20
        CppStandard = CppStandardVersion.Cpp20;
		
		// Enable header-only mode (recommended by libremidi documentation)
		PublicDefinitions.Add("LIBREMIDI_HEADER_ONLY");
		
		// Enable MIDI 2.0 support (UMP - Universal MIDI Packet)
		// Note: API is enabled, but MIDI 2.0 backend requires platform support
		PublicDefinitions.Add("LIBREMIDI_ENABLE_MIDI2");
		
		// Platform-specific libraries and frameworks
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// WinMM API (MIDI 1.0) - Windows XP+ compatible
			// Currently the only enabled backend on Windows
			PublicDefinitions.Add("LIBREMIDI_WINMM=1");
			PublicSystemLibraries.Add("winmm.lib");
            PublicSystemIncludePaths.Add(Path.Combine(
				Target.WindowsPlatform.WindowsSdkDir,
				"Include",
				Target.WindowsPlatform.WindowsSdkVersion,
				"cppwinrt"));

			// Windows MIDI Services (MIDI 2.0) - DISABLED
			// Reason: Microsoft.Windows.Devices.Midi2.h header not found
			// This requires Windows MIDI Services Developer Preview SDK
			// Download from: https://github.com/microsoft/MIDI/releases/
			// 
			// To enable after SDK installation:
			// 1. Uncomment the line below
			// 2. Add C++/WinRT include paths if needed
			// PublicDefinitions.Add("LIBREMIDI_WINMIDI=1");
			
			// Note: Application uses libremidi::API::WINDOWS_MM by default
			// MIDI 2.0 API (send_ump) is available but will work through MIDI 1.0 translation
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// CoreMIDI framework for macOS (MIDI 1.0 and 2.0)
			// MIDI 2.0 requires macOS 11+
			PublicDefinitions.Add("LIBREMIDI_COREMIDI=1");
			PublicFrameworks.Add("CoreMIDI");
			PublicFrameworks.Add("CoreAudio");
			PublicFrameworks.Add("CoreFoundation");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			// ALSA library for Linux (MIDI 1.0 and 2.0)
			// MIDI 2.0 requires kernel 6.5+ and latest libasound
			PublicDefinitions.Add("LIBREMIDI_ALSA=1");
			PublicSystemLibraries.Add("asound");
			PublicSystemLibraries.Add("pthread");
		}
	}
}