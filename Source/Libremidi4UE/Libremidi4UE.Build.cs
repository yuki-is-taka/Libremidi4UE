// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

using UnrealBuildTool;

public class Libremidi4UE : ModuleRules
{
	public Libremidi4UE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// libremidi is header-only and compiles inside this module's translation units; its Windows
		// MIDI Services backend and C++/WinRT throw and catch C++ exceptions by design. UE enables
		// exceptions only for targets compiled against the editor, so without this a game target would
		// build that code with no unwinding (no /EH on MSVC). Engine precedent: AudioCaptureRtAudio,
		// ImageWrapper.
		bEnableExceptions = true;
		// AutoRTFM cannot be used together with exceptions.
		bDisableAutoRTFMInstrumentation = true;

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
