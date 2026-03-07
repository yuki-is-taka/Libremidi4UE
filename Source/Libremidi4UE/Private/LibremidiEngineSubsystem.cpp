// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#include "LibremidiEngineSubsystem.h"
#include "Libremidi4UELog.h"
#include "LibremidiSettings.h"

void ULibremidiEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem Initialize start."));

	ULibremidiSettings* Settings = GetMutableDefault<ULibremidiSettings>();
	if (!Settings)
	{
		return;
	}

#if WITH_EDITOR
	SettingsChangedHandle = Settings->OnSettingChanged().AddUObject(
		this, &ULibremidiEngineSubsystem::HandleSettingsChanged);
#endif

	StartObserver();
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem Initialize complete."));
}

void ULibremidiEngineSubsystem::Deinitialize()
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem Deinitialize start."));
#if WITH_EDITOR
	ULibremidiSettings* Settings = GetMutableDefault<ULibremidiSettings>();
	if (Settings && SettingsChangedHandle.IsValid())
	{
		Settings->OnSettingChanged().Remove(SettingsChangedHandle);
		SettingsChangedHandle.Reset();
	}
#endif

	StopObserver();
	Super::Deinitialize();
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem Deinitialize complete."));
}

TArray<FLibremidiInputInfo> ULibremidiEngineSubsystem::GetInputPorts() const
{
	TArray<FLibremidiInputInfo> Ports;
	if (!Observer)
	{
		return Ports;
	}

	std::vector<libremidi::input_port> InputPorts = Observer->get_input_ports();
	Ports.Reserve(static_cast<int32>(InputPorts.size()));
	for (libremidi::input_port& Port : InputPorts)
	{
		Ports.Emplace(MoveTemp(Port));
	}

	return Ports;
}

libremidi::API ULibremidiEngineSubsystem::GetObserverApi() const
{
	if (Observer)
	{
		return Observer->get_current_api();
	}

	const ULibremidiSettings* Settings = GetDefault<ULibremidiSettings>();
	if (Settings)
	{
		return Settings->GetResolvedBackendAPI();
	}

	return libremidi::API::UNSPECIFIED;
}

ULibremidiInput* ULibremidiEngineSubsystem::CreateInput(
	ELibremidiMidiProtocol Protocol,
	bool bInIgnoreSysex,
	bool bInIgnoreTiming,
	bool bInIgnoreSensing,
	ELibremidiTimestampMode Mode,
	bool bInMidi1ChannelEventsToMidi2)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem CreateInput start."));
	ULibremidiInput* Input = ULibremidiInput::Create(
		this,
		Protocol,
		bInIgnoreSysex,
		bInIgnoreTiming,
		bInIgnoreSensing,
		Mode,
		bInMidi1ChannelEventsToMidi2);
	if (Input)
	{
		Inputs.Add(Input);
		UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem CreateInput complete."));
	}
	else
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiEngineSubsystem CreateInput failed."));
	}
	return Input;
}

void ULibremidiEngineSubsystem::DestroyInput(ULibremidiInput* Input)
{
	if (!Input)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiEngineSubsystem DestroyInput skipped: null input."));
		return;
	}

	Inputs.RemoveSingle(Input);
	Input->MarkAsGarbage();
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem DestroyInput complete."));
}

ULibremidiOutput* ULibremidiEngineSubsystem::CreateOutput(
	ELibremidiMidiProtocol Protocol,
	ELibremidiTimestampMode Mode)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem CreateOutput start."));
	ULibremidiOutput* Output = ULibremidiOutput::Create(this, Protocol, Mode);
	if (Output)
	{
		Outputs.Add(Output);
		UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem CreateOutput complete."));
	}
	else
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiEngineSubsystem CreateOutput failed."));
	}
	return Output;
}

void ULibremidiEngineSubsystem::DestroyOutput(ULibremidiOutput* Output)
{
	if (!Output)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiEngineSubsystem DestroyOutput skipped: null output."));
		return;
	}

	Outputs.RemoveSingle(Output);
	Output->MarkAsGarbage();
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem DestroyOutput complete."));
}

TArray<FLibremidiOutputInfo> ULibremidiEngineSubsystem::GetOutputPorts() const
{
	TArray<FLibremidiOutputInfo> Ports;
	if (!Observer)
	{
		return Ports;
	}

	std::vector<libremidi::output_port> OutputPorts = Observer->get_output_ports();
	Ports.Reserve(static_cast<int32>(OutputPorts.size()));
	for (libremidi::output_port& Port : OutputPorts)
	{
		Ports.Emplace(MoveTemp(Port));
	}

	return Ports;
}

void ULibremidiEngineSubsystem::StartObserver()
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem StartObserver start."));
	const ULibremidiSettings* Settings = GetDefault<ULibremidiSettings>();
	if (!Settings)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiEngineSubsystem StartObserver failed: settings missing."));
		return;
	}

	libremidi::observer_configuration Config;
	Config.track_hardware = Settings->bTrackHardware;
	Config.track_virtual = Settings->bTrackVirtual;
	Config.track_any = Settings->bTrackAny;
	Config.input_added = [this](const libremidi::input_port& Port)
	{
		HandleInputPortAdded(Port);
	};
	Config.input_removed = [this](const libremidi::input_port& Port)
	{
		HandleInputPortRemoved(Port);
	};
	Config.output_added = [this](const libremidi::output_port& Port)
	{
		HandleOutputPortAdded(Port);
	};
	Config.output_removed = [this](const libremidi::output_port& Port)
	{
		HandleOutputPortRemoved(Port);
	};

	const libremidi::API Api = Settings->GetResolvedBackendAPI();
	libremidi::observer_api_configuration ApiConfig = libremidi::observer_configuration_for(Api);
	Observer = MakeUnique<libremidi::observer>(Config, ApiConfig);

	LogObserverInfo();
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem StartObserver complete."));
}

void ULibremidiEngineSubsystem::StopObserver()
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem StopObserver start."));
	Observer.Reset();
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem StopObserver complete."));
}

void ULibremidiEngineSubsystem::LogObserverInfo() const
{
	if (!Observer)
	{
		return;
	}

	const libremidi::API CurrentApi = Observer->get_current_api();
	const std::string_view ApiName = libremidi::get_api_display_name(CurrentApi);
	const std::vector<libremidi::input_port> InputPorts = Observer->get_input_ports();
	const std::vector<libremidi::output_port> OutputPorts = Observer->get_output_ports();
	const int32 InputCount = static_cast<int32>(InputPorts.size());
	const int32 OutputCount = static_cast<int32>(OutputPorts.size());

	UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE observer started: API=%s Inputs=%d Outputs=%d"),
		UTF8_TO_TCHAR(ApiName.data()), InputCount, OutputCount);
}

#if WITH_EDITOR
void ULibremidiEngineSubsystem::HandleSettingsChanged(UObject* SettingsObject, FPropertyChangedEvent& PropertyChangedEvent)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiEngineSubsystem HandleSettingsChanged."));
	const ULibremidiSettings* Settings = GetDefault<ULibremidiSettings>();
	if (!Settings)
	{
		return;
	}

	const FProperty* ChangedProperty = PropertyChangedEvent.Property;
	if (!ChangedProperty)
	{
		return;
	}

	const FName PropertyName = ChangedProperty->GetFName();
	const FName BackendProtocolName(TEXT("BackendProtocol"));
	const FName BackendApiName(TEXT("BackendAPIName"));
	const bool bBackendProperty = PropertyName == BackendProtocolName || PropertyName == BackendApiName;
	if (!bBackendProperty)
	{
		return;
	}

	if (!Observer)
	{
		StartObserver();
		OnObserverRestarted.Broadcast();
		OnObserverRestartedNative.Broadcast();
		return;
	}

	const libremidi::API DesiredApi = Settings->GetResolvedBackendAPI();
	const libremidi::API CurrentApi = Observer->get_current_api();
	const bool bApiChanged = DesiredApi != CurrentApi;
	if (!bApiChanged)
	{
		return;
	}

	StopObserver();
	StartObserver();
	OnObserverRestarted.Broadcast();
	OnObserverRestartedNative.Broadcast();
}
#endif // WITH_EDITOR

void ULibremidiEngineSubsystem::HandleInputPortAdded(const libremidi::input_port& Port)
{
	FLibremidiInputInfo PortInfo(Port);
	OnInputPortAdded.Broadcast(PortInfo);
	OnInputPortAddedNative.Broadcast(PortInfo);
}

void ULibremidiEngineSubsystem::HandleInputPortRemoved(const libremidi::input_port& Port)
{
	FLibremidiInputInfo PortInfo(Port);
	OnInputPortRemoved.Broadcast(PortInfo);
	OnInputPortRemovedNative.Broadcast(PortInfo);
}

void ULibremidiEngineSubsystem::HandleOutputPortAdded(const libremidi::output_port& Port)
{
	FLibremidiOutputInfo PortInfo(Port);
	OnOutputPortAdded.Broadcast(PortInfo);
	OnOutputPortAddedNative.Broadcast(PortInfo);
}

void ULibremidiEngineSubsystem::HandleOutputPortRemoved(const libremidi::output_port& Port)
{
	FLibremidiOutputInfo PortInfo(Port);
	OnOutputPortRemoved.Broadcast(PortInfo);
	OnOutputPortRemovedNative.Broadcast(PortInfo);
}
