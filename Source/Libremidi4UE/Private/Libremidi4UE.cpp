// Copyright Epic Games, Inc. All Rights Reserved.

#include "Libremidi4UE.h"
#include "Libremidi4UELog.h"
#include "Modules/ModuleManager.h"

// libremidi include test
#include <libremidi/libremidi.hpp>

#define LOCTEXT_NAMESPACE "FLibremidi4UEModule"

namespace
{
	// Helper function to convert device_identifier to string
	FString DeviceIdentifierToString(const libremidi::device_identifier& device)
	{
		if (std::holds_alternative<std::monostate>(device))
		{
			return TEXT("<none>");
		}
		else if (std::holds_alternative<std::string>(device))
		{
			return UTF8_TO_TCHAR(std::get<std::string>(device).c_str());
		}
		else if (std::holds_alternative<std::uint64_t>(device))
		{
			return FString::Printf(TEXT("0x%llX"), std::get<std::uint64_t>(device));
		}
		return TEXT("<unknown>");
	}

	// Helper function to convert container_identifier to string
	FString ContainerIdentifierToString(const libremidi::container_identifier& container)
	{
		if (std::holds_alternative<std::monostate>(container))
		{
			return TEXT("<none>");
		}
		else if (std::holds_alternative<libremidi::uuid>(container))
		{
			const auto& uuid = std::get<libremidi::uuid>(container);
			return FString::Printf(TEXT("{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}"),
				uuid.bytes[0], uuid.bytes[1], uuid.bytes[2], uuid.bytes[3],
				uuid.bytes[4], uuid.bytes[5],
				uuid.bytes[6], uuid.bytes[7],
				uuid.bytes[8], uuid.bytes[9],
				uuid.bytes[10], uuid.bytes[11], uuid.bytes[12], uuid.bytes[13], uuid.bytes[14], uuid.bytes[15]);
		}
		else if (std::holds_alternative<std::string>(container))
		{
			return UTF8_TO_TCHAR(std::get<std::string>(container).c_str());
		}
		else if (std::holds_alternative<std::uint64_t>(container))
		{
			return FString::Printf(TEXT("0x%llX"), std::get<std::uint64_t>(container));
		}
		return TEXT("<unknown>");
	}

	// Helper function to convert port_type to string
	FString PortTypeToString(libremidi::port_information::port_type type)
	{
		using pt = libremidi::port_information::port_type;
		TArray<FString> types;

		if (type == pt::unknown) return TEXT("Unknown");
		if (type & pt::software) types.Add(TEXT("Software"));
		if (type & pt::loopback) types.Add(TEXT("Loopback"));
		if (type & pt::hardware) types.Add(TEXT("Hardware"));
		if (type & pt::usb) types.Add(TEXT("USB"));
		if (type & pt::bluetooth) types.Add(TEXT("Bluetooth"));
		if (type & pt::pci) types.Add(TEXT("PCI"));
		if (type & pt::network) types.Add(TEXT("Network"));

		return types.Num() > 0 ? FString::Join(types, TEXT(" | ")) : TEXT("None");
	}

	// Helper function to log all port information
	void LogPortInformation(const FString& EventType, const libremidi::port_information& port, bool bIsInput)
	{
		UE_LOG(LogLibremidi4UE, Log, TEXT("=== %s: %s Port ==="), *EventType, bIsInput ? TEXT("Input") : TEXT("Output"));
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Display Name: %s"), UTF8_TO_TCHAR(port.display_name.c_str()));
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Port Name: %s"), UTF8_TO_TCHAR(port.port_name.c_str()));
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Device Name: %s"), 
			port.device_name.empty() ? TEXT("<empty>") : UTF8_TO_TCHAR(port.device_name.c_str()));
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Manufacturer: %s"), 
			port.manufacturer.empty() ? TEXT("<empty>") : UTF8_TO_TCHAR(port.manufacturer.c_str()));
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Client Handle: 0x%llX"), port.client);
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Port Handle: 0x%llX"), port.port);
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Container ID: %s"), *ContainerIdentifierToString(port.container));
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Device ID: %s"), *DeviceIdentifierToString(port.device));
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Port Type: %s"), *PortTypeToString(port.type));
		UE_LOG(LogLibremidi4UE, Log, TEXT("========================================"));
	}
}

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
			LogPortInformation(TEXT("DEVICE CONNECTED"), port, true);
		};
		
		obs_conf.input_removed = [this](const libremidi::input_port& port)
		{
			LogPortInformation(TEXT("DEVICE DISCONNECTED"), port, true);
		};

		obs_conf.output_added = [this](const libremidi::output_port& port)
		{
			LogPortInformation(TEXT("DEVICE CONNECTED"), port, false);
		};
		
		obs_conf.output_removed = [this](const libremidi::output_port& port)
		{
			LogPortInformation(TEXT("DEVICE DISCONNECTED"), port, false);
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
		
		// 4) Log all currently connected devices with full details
		std::vector<libremidi::input_port> input_ports = MidiObserver->get_input_ports();
		UE_LOG(LogLibremidi4UE, Log, TEXT("=== Currently Connected Devices ==="));
		UE_LOG(LogLibremidi4UE, Log, TEXT("Found %d MIDI input port(s)"), input_ports.size());
		
		for (const libremidi::input_port& port : input_ports)
		{
			LogPortInformation(TEXT("EXISTING INPUT DEVICE"), port, true);
		}
		
		std::vector<libremidi::output_port> output_ports = MidiObserver->get_output_ports();
		UE_LOG(LogLibremidi4UE, Log, TEXT("Found %d MIDI output port(s)"), output_ports.size());
		
		for (const libremidi::output_port& port : output_ports)
		{
			LogPortInformation(TEXT("EXISTING OUTPUT DEVICE"), port, false);
		}
		
		UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE: libremidi integration test successful!"));
		UE_LOG(LogLibremidi4UE, Log, TEXT("Hot-plug monitoring is now active. Connect/disconnect MIDI devices to see detailed logs."));
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
