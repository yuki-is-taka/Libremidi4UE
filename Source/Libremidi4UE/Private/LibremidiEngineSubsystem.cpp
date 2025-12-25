#include "LibremidiEngineSubsystem.h"
#include "LibremidiSettings.h"
#include "Libremidi4UELog.h"

namespace
{
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

	void LogDetailedPortInformation(const FString& EventType, const libremidi::port_information& port, bool bIsInput)
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
		UE_LOG(LogLibremidi4UE, Log, TEXT("  Port Type: %s"), *PortTypeToString(port.type));
		UE_LOG(LogLibremidi4UE, Log, TEXT("========================================"));
	}
}

void ULibremidiEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const ULibremidiSettings* Settings = GetDefault<ULibremidiSettings>();
	if (Settings)
	{
		MidiAPI = Settings->BackendAPI;
		bTrackHardware = Settings->bTrackHardware;
		bTrackVirtual = Settings->bTrackVirtual;
		bTrackAny = Settings->bTrackAny;
	}

	StartObserver();

	LogAvailableAPIs();
	LogInitialDevices();

	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem initialized"));
}

void ULibremidiEngineSubsystem::Deinitialize()
{
	StopObserver();

	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem deinitialized"));

	Super::Deinitialize();
}

void ULibremidiEngineSubsystem::SetMidiAPI(ELibremidiAPI API)
{
	if (MidiAPI != API)
	{
		MidiAPI = API;
		RestartObserver();
		
		FString APIName = UEnum::GetValueAsString(API);
		UE_LOG(LogLibremidi4UE, Log, TEXT("MIDI API set to %s"), *APIName);
	}
}

void ULibremidiEngineSubsystem::SetTrackHardware(bool bEnabled)
{
	if (bTrackHardware != bEnabled)
	{
		bTrackHardware = bEnabled;
		RestartObserver();
		UE_LOG(LogLibremidi4UE, Log, TEXT("Track Hardware set to %s"), bEnabled ? TEXT("true") : TEXT("false"));
	}
}

void ULibremidiEngineSubsystem::SetTrackVirtual(bool bEnabled)
{
	if (bTrackVirtual != bEnabled)
	{
		bTrackVirtual = bEnabled;
		RestartObserver();
		UE_LOG(LogLibremidi4UE, Log, TEXT("Track Virtual set to %s"), bEnabled ? TEXT("true") : TEXT("false"));
	}
}

void ULibremidiEngineSubsystem::SetTrackAny(bool bEnabled)
{
	if (bTrackAny != bEnabled)
	{
		bTrackAny = bEnabled;
		RestartObserver();
		UE_LOG(LogLibremidi4UE, Log, TEXT("Track Any set to %s"), bEnabled ? TEXT("true") : TEXT("false"));
	}
}

ELibremidiAPI ULibremidiEngineSubsystem::GetCurrentAPI() const
{
	if (Observer)
	{
		return LibremidiTypeConversion::FromLibremidiAPI(Observer->get_current_api());
	}
	return ELibremidiAPI::Unspecified;
}

TArray<FMidiPortInfo> ULibremidiEngineSubsystem::GetAvailableInputPorts() const
{
	TArray<FMidiPortInfo> Result;
	
	if (Observer)
	{
		auto ports = Observer->get_input_ports();
		for (const auto& port : ports)
		{
			Result.Add(FMidiPortInfo(port));
		}
	}
	
	return Result;
}

TArray<FMidiPortInfo> ULibremidiEngineSubsystem::GetAvailableOutputPorts() const
{
	TArray<FMidiPortInfo> Result;
	
	if (Observer)
	{
		auto ports = Observer->get_output_ports();
		for (const auto& port : ports)
		{
			Result.Add(FMidiPortInfo(port));
		}
	}
	
	return Result;
}

void ULibremidiEngineSubsystem::StartObserver()
{
	if (Observer)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("Observer already exists. Stopping before starting new one."));
		StopObserver();
	}

	Observer = MakeUnique<libremidi::observer>(CreateObserverConfiguration(), CreateObserverAPIConfiguration());
	
	ELibremidiAPI CurrentAPI = GetCurrentAPI();
	FString APIName = UEnum::GetValueAsString(CurrentAPI);
	bool bIsMidi2 = libremidi::is_midi2(LibremidiTypeConversion::ToLibremidiAPI(CurrentAPI));
	
	UE_LOG(LogLibremidi4UE, Log, TEXT("MIDI Observer started"));
	UE_LOG(LogLibremidi4UE, Log, TEXT("  API: %s"), *APIName);
	UE_LOG(LogLibremidi4UE, Log, TEXT("  MIDI 2.0 Support: %s"), bIsMidi2 ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogLibremidi4UE, Log, TEXT("  Track Hardware: %s"), bTrackHardware ? TEXT("true") : TEXT("false"));
	UE_LOG(LogLibremidi4UE, Log, TEXT("  Track Virtual: %s"), bTrackVirtual ? TEXT("true") : TEXT("false"));
	UE_LOG(LogLibremidi4UE, Log, TEXT("  Track Any: %s"), bTrackAny ? TEXT("true") : TEXT("false"));
	
	bLoggedInitialDevices = false;
}

void ULibremidiEngineSubsystem::StopObserver()
{
	if (Observer)
	{
		Observer.Reset();
		UE_LOG(LogLibremidi4UE, Log, TEXT("MIDI Observer stopped"));
	}
}

void ULibremidiEngineSubsystem::RestartObserver()
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("Restarting MIDI Observer with new configuration..."));
	StopObserver();
	StartObserver();
	LogInitialDevices();
}

void ULibremidiEngineSubsystem::LogAvailableAPIs() const
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("=== Available MIDI 1.0 APIs ==="));
	for (auto api : libremidi::available_apis())
	{
		FString apiName = UTF8_TO_TCHAR(libremidi::get_api_display_name(api).data());
		UE_LOG(LogLibremidi4UE, Log, TEXT("  - %s"), *apiName);
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("=== Available MIDI 2.0 (UMP) APIs ==="));
	const auto umpApis = libremidi::available_ump_apis();
	if (umpApis.empty())
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("  No MIDI 2.0 APIs available"));
	}
	else
	{
		for (auto api : umpApis)
		{
			FString apiName = UTF8_TO_TCHAR(libremidi::get_api_display_name(api).data());
			UE_LOG(LogLibremidi4UE, Log, TEXT("  - %s"), *apiName);
		}
	}
}

void ULibremidiEngineSubsystem::LogInitialDevices()
{
	if (!Observer || bLoggedInitialDevices)
	{
		return;
	}

	std::vector<libremidi::input_port> input_ports = Observer->get_input_ports();
	UE_LOG(LogLibremidi4UE, Log, TEXT("=== Currently Connected MIDI Devices ==="));
	UE_LOG(LogLibremidi4UE, Log, TEXT("Found %d MIDI input port(s)"), input_ports.size());
	
	for (const libremidi::input_port& port : input_ports)
	{
		LogDetailedPortInformation(TEXT("EXISTING INPUT DEVICE"), port, true);
	}
	
	std::vector<libremidi::output_port> output_ports = Observer->get_output_ports();
	UE_LOG(LogLibremidi4UE, Log, TEXT("Found %d MIDI output port(s)"), output_ports.size());
	
	for (const libremidi::output_port& port : output_ports)
	{
		LogDetailedPortInformation(TEXT("EXISTING OUTPUT DEVICE"), port, false);
	}
	
	bLoggedInitialDevices = true;
}

libremidi::observer_configuration ULibremidiEngineSubsystem::CreateObserverConfiguration() const
{
	libremidi::observer_configuration config;

	config.on_error = [this](std::string_view errorText, const libremidi::source_location& loc)
	{
		HandleError(errorText, loc);
	};

	config.on_warning = [this](std::string_view warningText, const libremidi::source_location& loc)
	{
		HandleWarning(warningText, loc);
	};

	config.input_added = [this](const libremidi::input_port& port)
	{
		HandleInputDeviceAdded(port);
	};

	config.input_removed = [this](const libremidi::input_port& port)
	{
		HandleInputDeviceRemoved(port);
	};

	config.output_added = [this](const libremidi::output_port& port)
	{
		HandleOutputDeviceAdded(port);
	};

	config.output_removed = [this](const libremidi::output_port& port)
	{
		HandleOutputDeviceRemoved(port);
	};

	config.track_hardware = bTrackHardware;
	config.track_virtual = bTrackVirtual;
	config.track_any = bTrackAny;
	config.notify_in_constructor = true;

	return config;
}

libremidi::observer_api_configuration ULibremidiEngineSubsystem::CreateObserverAPIConfiguration() const
{
	return LibremidiTypeConversion::ToLibremidiAPI(MidiAPI);
}

void ULibremidiEngineSubsystem::HandleError(std::string_view ErrorText, const libremidi::source_location& Location) const
{
	FString ErrorMessage = UTF8_TO_TCHAR(ErrorText.data());
	FString FileName = UTF8_TO_TCHAR(Location.file_name());
	int32 LineNumber = Location.line();

	UE_LOG(LogLibremidi4UE, Error, TEXT("MIDI Error at %s:%d - %s"), *FileName, LineNumber, *ErrorMessage);

	AsyncTask(ENamedThreads::GameThread, [this, ErrorMessage, FileName, LineNumber]()
	{
		const_cast<ULibremidiEngineSubsystem*>(this)->OnError.Broadcast(ErrorMessage, FileName, LineNumber);
	});
}

void ULibremidiEngineSubsystem::HandleWarning(std::string_view WarningText, const libremidi::source_location& Location) const
{
	FString WarningMessage = UTF8_TO_TCHAR(WarningText.data());
	FString FileName = UTF8_TO_TCHAR(Location.file_name());
	int32 LineNumber = Location.line();

	UE_LOG(LogLibremidi4UE, Warning, TEXT("MIDI Warning at %s:%d - %s"), *FileName, LineNumber, *WarningMessage);

	AsyncTask(ENamedThreads::GameThread, [this, WarningMessage, FileName, LineNumber]()
	{
		const_cast<ULibremidiEngineSubsystem*>(this)->OnWarning.Broadcast(WarningMessage, FileName, LineNumber);
	});
}

void ULibremidiEngineSubsystem::HandleInputDeviceAdded(const libremidi::input_port& Port) const
{
	FMidiPortInfo PortInfo(Port);
	
	// Only log detailed info on hot-plug events, not during initialization
	if (bLoggedInitialDevices)
	{
		LogDetailedPortInformation(TEXT("DEVICE CONNECTED"), Port, true);
	}

	AsyncTask(ENamedThreads::GameThread, [this, PortInfo]()
	{
		const_cast<ULibremidiEngineSubsystem*>(this)->OnInputDeviceAdded.Broadcast(PortInfo);
	});
}

void ULibremidiEngineSubsystem::HandleInputDeviceRemoved(const libremidi::input_port& Port) const
{
	FMidiPortInfo PortInfo(Port);
	
	UE_LOG(LogLibremidi4UE, Log, TEXT("=== DEVICE DISCONNECTED: Input Port ==="));
	UE_LOG(LogLibremidi4UE, Log, TEXT("  Display Name: %s"), *PortInfo.DisplayName);
	UE_LOG(LogLibremidi4UE, Log, TEXT("========================================"));

	AsyncTask(ENamedThreads::GameThread, [this, PortInfo]()
	{
		const_cast<ULibremidiEngineSubsystem*>(this)->OnInputDeviceRemoved.Broadcast(PortInfo);
	});
}

void ULibremidiEngineSubsystem::HandleOutputDeviceAdded(const libremidi::output_port& Port) const
{
	FMidiPortInfo PortInfo(Port);
	
	// Only log detailed info on hot-plug events, not during initialization
	if (bLoggedInitialDevices)
	{
		LogDetailedPortInformation(TEXT("DEVICE CONNECTED"), Port, false);
	}

	AsyncTask(ENamedThreads::GameThread, [this, PortInfo]()
	{
		const_cast<ULibremidiEngineSubsystem*>(this)->OnOutputDeviceAdded.Broadcast(PortInfo);
	});
}

void ULibremidiEngineSubsystem::HandleOutputDeviceRemoved(const libremidi::output_port& Port) const
{
	FMidiPortInfo PortInfo(Port);
	
	UE_LOG(LogLibremidi4UE, Log, TEXT("=== DEVICE DISCONNECTED: Output Port ==="));
	UE_LOG(LogLibremidi4UE, Log, TEXT("  Display Name: %s"), *PortInfo.DisplayName);
	UE_LOG(LogLibremidi4UE, Log, TEXT("========================================"));

	AsyncTask(ENamedThreads::GameThread, [this, PortInfo]()
	{
		const_cast<ULibremidiEngineSubsystem*>(this)->OnOutputDeviceRemoved.Broadcast(PortInfo);
	});
}
