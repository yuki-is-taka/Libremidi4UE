// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

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

			// MIDI 2.0: ENABLE_MIDI2 stays on regardless; the Windows MIDI Services backend itself is
			// compiled in only when the host project supplies the Windows.Devices.Midi2 projection
			// (same detection as the WindowsMidiServices module, which also sets WITH_WINDOWS_MIDI_SERVICES).
			PublicDefinitions.Add("LIBREMIDI_ENABLE_MIDI2");
			PublicDependencyModuleNames.Add("WindowsMidiServices");

			string WindowsMidiServicesPath = WindowsMidiServices.FindIncludeDir(Target);
			if (WindowsMidiServicesPath != null)
			{
				PublicDefinitions.Add("LIBREMIDI_WINMIDI");
				PublicSystemLibraries.Add("WindowsApp.lib");

				// Include order matters: the project's matched C++/WinRT projection must come before the
				// Windows SDK C++/WinRT. The SDK that UE selects can carry an older cppwinrt than the one
				// the projection was generated with, and each generated header asserts the exact version.
				// The projection ships its own winrt/base.h plus the whole dependency closure, so nothing
				// should resolve from the SDK directory; it stays as a fallback behind it.
				PublicSystemIncludePaths.Add(WindowsMidiServicesPath);

				string WindowsSdkCppWinRTPath = Path.Combine(
					Target.WindowsPlatform.WindowsSdkDir,
					"Include",
					Target.WindowsPlatform.WindowsSdkVersion,
					"cppwinrt");
				PublicSystemIncludePaths.Add(WindowsSdkCppWinRTPath);
			}
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