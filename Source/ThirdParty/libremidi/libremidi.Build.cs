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
		
		// Platform-specific libraries and frameworks
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// WinMM API (MIDI 1.0) - Windows XP+ compatible
			PublicDefinitions.Add("LIBREMIDI_WINMM=1");
			PublicSystemLibraries.Add("winmm.lib");
            PublicSystemIncludePaths.Add(Path.Combine(
				Target.WindowsPlatform.WindowsSdkDir,
				"Include",
				Target.WindowsPlatform.WindowsSdkVersion,
				"cppwinrt"));

			// Windows MIDI Services (MIDI 2.0) - DISABLED
			// The MIDI SDK headers (generated with C++/WinRT v2.0.240405.15) are incompatible
			// with Windows SDK 10.0.22621.0's C++/WinRT headers.
			// To enable MIDI 2.0 support, regenerate MIDI SDK headers using the same 
			// C++/WinRT version as the Windows SDK, or use the MIDI SDK's bundled NuGet package.
			PublicDefinitions.Add("LIBREMIDI_WINMIDI=0");
			
			// MIDI 2.0 API support (UMP - Universal MIDI Packet) is disabled on Windows
			// because WinMIDI backend is required for actual MIDI 2.0 hardware communication
			// Note: We do NOT define LIBREMIDI_ENABLE_MIDI2 here to avoid WinMIDI backend compilation
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// CoreMIDI framework for macOS (MIDI 1.0 and 2.0)
			// MIDI 2.0 requires macOS 11+
			PublicDefinitions.Add("LIBREMIDI_COREMIDI=1");
			PublicDefinitions.Add("LIBREMIDI_ENABLE_MIDI2");
			PublicFrameworks.Add("CoreMIDI");
			PublicFrameworks.Add("CoreAudio");
			PublicFrameworks.Add("CoreFoundation");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			// ALSA library for Linux (MIDI 1.0 and 2.0)
			// MIDI 2.0 requires kernel 6.5+ and latest libasound
			PublicDefinitions.Add("LIBREMIDI_ALSA=1");
			PublicDefinitions.Add("LIBREMIDI_ENABLE_MIDI2");
			PublicSystemLibraries.Add("asound");
			PublicSystemLibraries.Add("pthread");
		}
	}
}