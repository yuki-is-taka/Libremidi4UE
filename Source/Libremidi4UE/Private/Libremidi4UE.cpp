// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#include "Libremidi4UE.h"
#include "Libremidi4UELog.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FLibremidi4UEModule"

#if PLATFORM_WINDOWS && WITH_WINDOWS_MIDI_SERVICES
namespace
{
	// Handle of the app-local Windows.Devices.Midi2.dll. Deliberately never released: C++/WinRT caches
	// activation factories that point into the module, and the process owns it until exit.
	void* GWindowsMidiServicesDll = nullptr;

	// The project supplies Windows.Devices.Midi2.dll (+ .pri) under <Project>/ThirdParty/WindowsMidiServices/
	// Win64/bin (same project-relative path in editor and packaged builds). Where the OS does not register
	// the API (before Windows 11 25H2), C++/WinRT activation falls back to loading "Windows.Devices.Midi2.dll"
	// by bare name, which succeeds only if a module with that name is already in the process. So the DLL is
	// loaded here by full path, before any libremidi observer / midi_in / midi_out can touch WinRT. Where the
	// OS registers the API, activation goes to the OS and this preload is harmless.
	void PreloadWindowsMidiServicesRuntime()
	{
		const FString DllPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(), TEXT("ThirdParty/WindowsMidiServices/Win64/bin/Windows.Devices.Midi2.dll")));

		if (!IFileManager::Get().FileExists(*DllPath))
		{
			UE_LOG(LogLibremidi4UE, Log,
				TEXT("Windows MIDI Services runtime not found at '%s'; relying on the API provided by the OS."), *DllPath);
			return;
		}

		GWindowsMidiServicesDll = FPlatformProcess::GetDllHandle(*DllPath);
		if (GWindowsMidiServicesDll)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Preloaded Windows MIDI Services runtime from '%s'."), *DllPath);
		}
		else
		{
			UE_LOG(LogLibremidi4UE, Error,
				TEXT("Failed to load the Windows MIDI Services runtime from '%s'. The Windows MIDI Services backend will work only if the OS provides the API."),
				*DllPath);
		}
	}
}
#endif // PLATFORM_WINDOWS && WITH_WINDOWS_MIDI_SERVICES

void FLibremidi4UEModule::StartupModule()
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE module loaded"));

#if PLATFORM_WINDOWS
#if WITH_WINDOWS_MIDI_SERVICES
	PreloadWindowsMidiServicesRuntime();
#else
	UE_LOG(LogLibremidi4UE, Warning,
		TEXT("Built without Windows MIDI Services support (the project does not provide ThirdParty/WindowsMidiServices/Win64/include); only WinMM (MIDI 1.0) is available on Windows."));
#endif
#endif
}

void FLibremidi4UEModule::ShutdownModule()
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE module unloaded"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLibremidi4UEModule, Libremidi4UE)
