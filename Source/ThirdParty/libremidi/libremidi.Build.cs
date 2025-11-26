// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class libremidi : ModuleRules
{
	public libremidi(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		// libremidi is a header-only library in the submodule
		string IncludePath = Path.Combine(ModuleDirectory, "libremidi", "include");
		PublicSystemIncludePaths.Add(IncludePath);
	}
}
