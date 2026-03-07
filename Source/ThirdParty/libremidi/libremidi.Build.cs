// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.
//
// IMPORTANT: libremidi requires manual patching after submodule update.
// File: libremidi/include/libremidi/backends/winmidi/helpers.hpp
// Fix: Remove duplicate "using namespace Windows::Devices::Enumeration;" (without winrt:: prefix)
// The line with "winrt::Windows::Devices::Enumeration" should remain.

using System.IO;
using UnrealBuildTool;

public class libremidi : ModuleRules
{
	public libremidi(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		// libremidi requires C++20
		CppStandard = CppStandardVersion.Cpp20;
		
		// libremidi submodule include path
		string IncludePath = Path.Combine(ModuleDirectory, "libremidi", "include");
		PublicSystemIncludePaths.Add(IncludePath);
		
		// Enable header-only mode (recommended by libremidi documentation)
		PublicDefinitions.Add("LIBREMIDI_HEADER_ONLY");
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// MIDI 1.0: WinMM API
			PublicDefinitions.Add("LIBREMIDI_WINMM");
			PublicSystemLibraries.Add("winmm.lib");

			// MIDI 2.0: Windows MIDI Services
			PublicDefinitions.Add("LIBREMIDI_WINMIDI");
			PublicDefinitions.Add("LIBREMIDI_ENABLE_MIDI2");
			PublicSystemLibraries.Add("WindowsApp.lib");
			
			// Include order matters: MIDI SDK headers must come before Windows SDK C++/WinRT
			string WindowsMidiServicesPath = Path.Combine(ModuleDirectory, "..", "WindowsMidiServices", "Win64", "include");
			PublicSystemIncludePaths.Add(WindowsMidiServicesPath);
			
			string WindowsSdkCppWinRTPath = Path.Combine(
				Target.WindowsPlatform.WindowsSdkDir,
				"Include",
				Target.WindowsPlatform.WindowsSdkVersion,
				"cppwinrt");
			PublicSystemIncludePaths.Add(WindowsSdkCppWinRTPath);
			
			PublicDependencyModuleNames.Add("WindowsMidiServices");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// MIDI 1.0 & 2.0: CoreMIDI (MIDI 2.0 requires macOS 11+)
			PublicDefinitions.Add("LIBREMIDI_COREMIDI");
			PublicDefinitions.Add("LIBREMIDI_ENABLE_MIDI2");
			PublicFrameworks.AddRange(new string[] { "CoreMIDI", "CoreAudio", "CoreFoundation" });
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			// MIDI 1.0 & 2.0: ALSA (MIDI 2.0 requires kernel 6.5+)
			PublicDefinitions.Add("LIBREMIDI_ALSA");
			PublicDefinitions.Add("LIBREMIDI_ENABLE_MIDI2");
			PublicSystemLibraries.AddRange(new string[] { "asound", "pthread" });
		}
	}
}