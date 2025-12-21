// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WindowsMidiServices : ModuleRules
{
	public WindowsMidiServices(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Windows MIDI Services (MIDI 2.0) is DISABLED.
			// 
			// Reason: C++/WinRT version mismatch between MIDI SDK headers (v2.0.240405.15)
			// and Windows SDK 10.0.22621.0's C++/WinRT headers.
			//
			// Do NOT add any include paths here - libremidi's winmidi backend is disabled,
			// and adding incompatible MIDI SDK headers would cause compilation errors.
			//
			// To enable MIDI 2.0 support in the future:
			// 1. Update Windows SDK to a version with matching C++/WinRT
			// 2. Or regenerate MIDI SDK headers with matching C++/WinRT version
			// 3. Then uncomment the include paths below and set WITH_WINDOWS_MIDI_SERVICES=1
			
			// string Win64Path = Path.Combine(ModuleDirectory, "Win64");
			// string LocalIncludePath = Path.Combine(Win64Path, "include");
			// PublicSystemIncludePaths.Add(LocalIncludePath);
			
			// string WindowsSdkIncludePath = Path.Combine(
			// 	Target.WindowsPlatform.WindowsSdkDir,
			// 	"Include",
			// 	Target.WindowsPlatform.WindowsSdkVersion,
			// 	"cppwinrt");
			// PublicSystemIncludePaths.Add(WindowsSdkIncludePath);
			
			PublicDefinitions.Add("WITH_WINDOWS_MIDI_SERVICES=0");
		}
		else
		{
			PublicDefinitions.Add("WITH_WINDOWS_MIDI_SERVICES=0");
		}
	}
}
