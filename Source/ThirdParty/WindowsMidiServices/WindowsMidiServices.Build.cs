// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

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
			PublicSystemIncludePaths.Add(Path.Combine(IncludePath, "winmidi"));
			
			PublicDefinitions.Add("WITH_WINDOWS_MIDI_SERVICES=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_WINDOWS_MIDI_SERVICES=0");
		}
	}
}
