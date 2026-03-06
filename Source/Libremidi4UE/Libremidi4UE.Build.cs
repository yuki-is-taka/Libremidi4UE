// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Libremidi4UE : ModuleRules
{
	public Libremidi4UE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
				"libremidi",
				"Projects"
			}
			);
	}
}
