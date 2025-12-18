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
		
		// Create the observer with the configured callbacks
		MidiObserver = std::make_unique<libremidi::observer>(obs_conf);
		
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
