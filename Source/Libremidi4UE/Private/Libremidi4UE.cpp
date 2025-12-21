// Copyright Epic Games, Inc. All Rights Reserved.

#include "Libremidi4UE.h"
#include "Libremidi4UELog.h"
#include "Modules/ModuleManager.h"

// libremidi include test
#include <libremidi/libremidi.hpp>

#define LOCTEXT_NAMESPACE "FLibremidi4UEModule"

void FLibremidi4UEModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	// Test: Enumerate available MIDI input ports
	UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: Testing libremidi integration..."));
	
	try
	{
		// 1) Available MIDI 1.0 APIs
		UE_LOG(LogLibremidi4UE, Log, TEXT("=== MIDI 1.0 APIs ==="));
		for (auto api : libremidi::available_apis())
		{
			FString apiName = UTF8_TO_TCHAR(libremidi::get_api_display_name(api).data());
			UE_LOG(LogLibremidi4UE, Log, TEXT("  - %s (ID: %d)"), *apiName, static_cast<int>(api));
		}

		// 2) Available MIDI 2.0 (UMP) APIs
		UE_LOG(LogLibremidi4UE, Log, TEXT("=== MIDI 2.0 (UMP) APIs ==="));
		const auto umpApis = libremidi::available_ump_apis();
		if (umpApis.empty())
		{
			UE_LOG(LogLibremidi4UE, Warning, TEXT("  No MIDI 2.0 APIs available!"));
		}
		else
		{
			for (auto api : umpApis)
			{
				FString apiName = UTF8_TO_TCHAR(libremidi::get_api_display_name(api).data());
				UE_LOG(LogLibremidi4UE, Log, TEXT("  - %s (ID: %d)"), *apiName, static_cast<int>(api));
			}
		}

		// Setup observer configuration with hot plug callbacks
		libremidi::observer_configuration obs_conf;
		
		obs_conf.input_added = [this](const libremidi::input_port& port)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: Input device connected - %s"), UTF8_TO_TCHAR(port.display_name.c_str()));
		};
		
		obs_conf.input_removed = [this](const libremidi::input_port& port)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: Input device disconnected - %s"), UTF8_TO_TCHAR(port.display_name.c_str()));
		};

		obs_conf.output_added = [this](const libremidi::output_port& port)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: Output device connected - %s"), UTF8_TO_TCHAR(port.display_name.c_str()));
		};
		
		obs_conf.output_removed = [this](const libremidi::output_port& port)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: Output device disconnected - %s"), UTF8_TO_TCHAR(port.display_name.c_str()));
		};
		
		// Create the observer with MIDI 2.0 API if available
		if (!umpApis.empty())
		{
			// Use the first available MIDI 2.0 API (e.g., Windows MIDI Services)
			MidiObserver = std::make_unique<libremidi::observer>(obs_conf, libremidi::observer_configuration_for(umpApis[0]));
			UE_LOG(LogLibremidi4UE, Log, TEXT("Observer created with MIDI 2.0 API"));
		}
		else
		{
			// Fallback to default API
			MidiObserver = std::make_unique<libremidi::observer>(obs_conf);
			UE_LOG(LogLibremidi4UE, Warning, TEXT("Observer created with default API (MIDI 2.0 not available)"));
		}

		// 3) Current observer API and MIDI2 capability
		libremidi::API currentApi = MidiObserver->get_current_api();
		FString currentApiName = UTF8_TO_TCHAR(libremidi::get_api_display_name(currentApi).data());
		const bool isMidi2 = libremidi::is_midi2(currentApi);
		UE_LOG(LogLibremidi4UE, Log, TEXT("=== Current Observer API ==="));
		UE_LOG(LogLibremidi4UE, Log, TEXT("  API: %s (ID: %d)"), *currentApiName, static_cast<int>(currentApi));
		UE_LOG(LogLibremidi4UE, Log, TEXT("  MIDI 2.0 Support: %s"), isMidi2 ? TEXT("YES") : TEXT("NO"));
		
		std::vector<libremidi::input_port> input_ports = MidiObserver->get_input_ports();
		UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: Found %d MIDI input port(s)"), input_ports.size());
		
		for (const libremidi::input_port& port : input_ports)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("  - Input: %s"), UTF8_TO_TCHAR(port.display_name.c_str()));
		}
		
		std::vector<libremidi::output_port> output_ports = MidiObserver->get_output_ports();
		UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: Found %d MIDI output port(s)"), output_ports.size());
		
		for (const libremidi::output_port& port : output_ports)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("  - Output: %s"), UTF8_TO_TCHAR(port.display_name.c_str()));
		}
		
		UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: libremidi integration test successful!"));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("Libremidi4UE: Exception during test: %s"), UTF8_TO_TCHAR(e.what()));
	}
}

void FLibremidi4UEModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
	UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: Module shutting down"));
	
	// Clean up the observer
	MidiObserver.reset();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLibremidi4UEModule, Libremidi4UE)
