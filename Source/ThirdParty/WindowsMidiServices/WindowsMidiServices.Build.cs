// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

using System.IO;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

// Windows MIDI Services (Windows.Devices.Midi2, in-box API preview) is NOT bundled with this plugin.
// The host project supplies the C++/WinRT projection headers and the runtime DLL under
//   <ProjectDir>/ThirdParty/WindowsMidiServices/Win64/{include,bin}
// This module only points at them. See README.md next to this file for the required layout.
public class WindowsMidiServices : ModuleRules
{
	// Project-relative root of the host-supplied files (forward slashes: used in $(ProjectDir)/... staging paths).
	public const string ProjectRelativeRoot = "ThirdParty/WindowsMidiServices/Win64";

	// Files staged next to each other, at the same project-relative path, in packaged builds.
	static readonly string[] RuntimeFileNames = { "Windows.Devices.Midi2.dll", "Windows.Devices.Midi2.pri" };

	// Returns the host project's projection include directory, or null when it is not there (or when
	// the target is not Win64 or has no project, e.g. an engine-only build). Shared with libremidi.Build.cs
	// so that both modules take the same decision. A matched cppwinrt set is required: winrt/base.h must
	// ship together with the Windows.Devices.Midi2 headers, because every generated header asserts the
	// exact C++/WinRT version and UE may build against a Windows SDK whose cppwinrt is older.
	public static string FindIncludeDir(ReadOnlyTargetRules Target)
	{
		if (Target.Platform != UnrealTargetPlatform.Win64 || Target.ProjectFile == null)
		{
			return null;
		}

		string IncludeDir = Path.Combine(
			Target.ProjectFile.Directory.FullName, "ThirdParty", "WindowsMidiServices", "Win64", "include");
		bool bComplete =
			File.Exists(Path.Combine(IncludeDir, "winrt", "Windows.Devices.Midi2.h")) &&
			File.Exists(Path.Combine(IncludeDir, "winrt", "base.h"));
		return bComplete ? IncludeDir : null;
	}

	public WindowsMidiServices(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IncludeDir = FindIncludeDir(Target);
		if (IncludeDir != null)
		{
			PublicSystemIncludePaths.Add(IncludeDir);
			PublicDefinitions.Add("WITH_WINDOWS_MIDI_SERVICES=1");

			// Stage the runtime (loose, next to the project content) at the same project-relative path;
			// the Libremidi4UE module preloads it from FPaths::ProjectDir() by full path.
			string BinDir = Path.Combine(Target.ProjectFile.Directory.FullName, "ThirdParty", "WindowsMidiServices", "Win64", "bin");
			foreach (string FileName in RuntimeFileNames)
			{
				if (File.Exists(Path.Combine(BinDir, FileName)))
				{
					RuntimeDependencies.Add("$(ProjectDir)/" + ProjectRelativeRoot + "/bin/" + FileName);
				}
				else
				{
					Logger.LogWarning("WindowsMidiServices: {File} not found in {Dir}. Windows MIDI Services will work only where the OS " +
						"provides the API in-box; packaged builds will not carry the app-local runtime.", FileName, BinDir);
				}
			}

			Logger.LogInformation("WindowsMidiServices: projection found in {Dir}", IncludeDir);
		}
		else
		{
			PublicDefinitions.Add("WITH_WINDOWS_MIDI_SERVICES=0");

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				Logger.LogWarning("WindowsMidiServices: the Windows.Devices.Midi2 C++/WinRT projection was not found; the Windows MIDI Services " +
					"backend is disabled (WinMM / MIDI 1.0 only). Put a matched cppwinrt projection (winrt/base.h and winrt/Windows.Devices.Midi2*.h) " +
					"in <ProjectDir>/ThirdParty/WindowsMidiServices/Win64/include and Windows.Devices.Midi2.dll/.pri in " +
					"<ProjectDir>/ThirdParty/WindowsMidiServices/Win64/bin. See Plugins/Libremidi4UE/README.md.");
			}
		}
	}
}
