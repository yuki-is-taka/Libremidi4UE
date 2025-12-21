// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class WindowsMidiServices : ModuleRules
{
	public WindowsMidiServices(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Windows MIDI Services SDK headers (regenerated with cppwinrt.exe)
			string IncludePath = Path.Combine(ModuleDirectory, "Win64", "include");
			PublicSystemIncludePaths.Add(IncludePath);
			
			PublicDefinitions.Add("WITH_WINDOWS_MIDI_SERVICES=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_WINDOWS_MIDI_SERVICES=0");
		}
	}
}
