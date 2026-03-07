// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#include "LibremidiOutput.h"
#include "Libremidi4UELog.h"
#include "LibremidiEngineSubsystem.h"
#include "LibremidiMessage.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
#include <libremidi/configurations.hpp>
THIRD_PARTY_INCLUDES_END

ULibremidiEngineSubsystem* ULibremidiOutput::GetOwnerSubsystem() const
{
	return Cast<ULibremidiEngineSubsystem>(GetOuter());
}

ULibremidiOutput* ULibremidiOutput::Create(
	UObject* Outer,
	ELibremidiMidiProtocol Protocol,
	ELibremidiTimestampMode Mode)
{
	if (!Outer)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput Create failed: Outer is null."));
		return nullptr;
	}

	ULibremidiOutput* Output = NewObject<ULibremidiOutput>(Outer);
	if (!Output)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput Create failed: NewObject returned null."));
		return nullptr;
	}

	if (!Output->Initialize(Protocol, Mode))
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput Create failed: initialization failed."));
		return nullptr;
	}
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiOutput created and initialized."));
	return Output;
}

bool ULibremidiOutput::Initialize(ELibremidiMidiProtocol Protocol, ELibremidiTimestampMode Mode)
{
	if (MidiOut)
	{
		if (MidiOut->is_port_open())
		{
			MidiOut->close_port();
		}
		MidiOut.Reset();
	}

	MidiProtocol = Protocol;
	SetTimestampMode(Mode);

	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiOutput Initialize: Protocol=%s TimestampMode=%d"),
		MidiProtocol == ELibremidiMidiProtocol::Midi2 ? TEXT("MIDI2") : TEXT("MIDI1"),
		static_cast<int32>(Mode));

	const ULibremidiEngineSubsystem* OwnerSubsystem = GetOwnerSubsystem();
	const libremidi::API ObserverApi = OwnerSubsystem ? OwnerSubsystem->GetObserverApi() : libremidi::API::UNSPECIFIED;
	if (ObserverApi == libremidi::API::UNSPECIFIED)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput Initialize failed: observer API unspecified."));
		return false;
	}
	libremidi::output_api_configuration ApiConfig = ObserverApi;
	const std::string_view ApiName = libremidi::get_api_display_name(ObserverApi);
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiOutput Initialize using API: %s"), UTF8_TO_TCHAR(ApiName.data()));

	libremidi::output_configuration Config;
	Config.on_error = [this](std::string_view ErrorText, const libremidi::source_location& Location)
	{
		HandleError(ErrorText, Location);
	};
	Config.on_warning = [this](std::string_view WarningText, const libremidi::source_location& Location)
	{
		HandleWarning(WarningText, Location);
	};
	Config.timestamps = TimestampMode;

	MidiOut = MakeUnique<libremidi::midi_out>(Config, ApiConfig);
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiOutput created MIDI output."));
	return MidiOut.IsValid();
}

bool ULibremidiOutput::OpenOutput(const FLibremidiOutputInfo& PortInfo)
{
	if (!MidiOut)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput OpenOutput failed: MIDI output not initialized."));
		return false;
	}

	libremidi::output_port Port;
	static_cast<libremidi::port_information&>(Port) = PortInfo.GetPort();
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiOutput OpenOutput: %s"), UTF8_TO_TCHAR(Port.display_name.c_str()));
	const stdx::error Result = MidiOut->open_port(Port);
	if (Result != stdx::error{})
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput OpenOutput failed for port: %s"), UTF8_TO_TCHAR(Port.display_name.c_str()));
		return false;
	}

	return true;
}

void ULibremidiOutput::CloseOutput()
{
	if (!MidiOut)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput CloseOutput skipped: MIDI output not initialized."));
		return;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiOutput CloseOutput."));
	MidiOut->close_port();
}

bool ULibremidiOutput::IsPortOpen() const
{
	return MidiOut ? MidiOut->is_port_open() : false;
}

bool ULibremidiOutput::IsPortConnected() const
{
	return MidiOut ? MidiOut->is_port_connected() : false;
}

ELibremidiMidiProtocol ULibremidiOutput::GetMidiProtocol() const
{
	return MidiProtocol;
}

void ULibremidiOutput::SetMidiProtocol(ELibremidiMidiProtocol Protocol)
{
	MidiProtocol = Protocol;
}

bool ULibremidiOutput::GetIsUmp() const
{
	return MidiProtocol == ELibremidiMidiProtocol::Midi2;
}

ELibremidiTimestampMode ULibremidiOutput::GetTimestampMode() const
{
	switch (TimestampMode)
	{
	case libremidi::timestamp_mode::NoTimestamp:
		return ELibremidiTimestampMode::NoTimestamp;
	case libremidi::timestamp_mode::Relative:
		return ELibremidiTimestampMode::Relative;
	case libremidi::timestamp_mode::Absolute:
		return ELibremidiTimestampMode::Absolute;
	case libremidi::timestamp_mode::SystemMonotonic:
		return ELibremidiTimestampMode::SystemMonotonic;
	case libremidi::timestamp_mode::AudioFrame:
		return ELibremidiTimestampMode::AudioFrame;
	case libremidi::timestamp_mode::Custom:
	default:
		return ELibremidiTimestampMode::Custom;
	}
}

void ULibremidiOutput::SetTimestampMode(ELibremidiTimestampMode Mode)
{
	switch (Mode)
	{
	case ELibremidiTimestampMode::NoTimestamp:
		TimestampMode = libremidi::timestamp_mode::NoTimestamp;
		break;
	case ELibremidiTimestampMode::Relative:
		TimestampMode = libremidi::timestamp_mode::Relative;
		break;
	case ELibremidiTimestampMode::Absolute:
		TimestampMode = libremidi::timestamp_mode::Absolute;
		break;
	case ELibremidiTimestampMode::SystemMonotonic:
		TimestampMode = libremidi::timestamp_mode::SystemMonotonic;
		break;
	case ELibremidiTimestampMode::AudioFrame:
		TimestampMode = libremidi::timestamp_mode::AudioFrame;
		break;
	case ELibremidiTimestampMode::Custom:
	default:
		TimestampMode = libremidi::timestamp_mode::Custom;
		break;
	}
}

// ---------------------------------------------------------------------------
// Send — wrapper type (MIDI Through / forwarding)
// ---------------------------------------------------------------------------

bool ULibremidiOutput::SendMIDIMessage(const FLibremidiMidi1Message& Message)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput SendMIDIMessage failed: port not open."));
		return false;
	}

	const stdx::error Err = MidiOut->send_message(Message.GetNativeMessage());
	return Err == stdx::error{};
}

bool ULibremidiOutput::SendUmpMessage(const FLibremidiUmpMessage& Message)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput SendUmpMessage failed: port not open."));
		return false;
	}

	const stdx::error Err = MidiOut->send_ump(Message.GetNativeMessage());
	return Err == stdx::error{};
}

// ---------------------------------------------------------------------------
// Send — raw (programmatic, no heap allocation)
// ---------------------------------------------------------------------------

bool ULibremidiOutput::SendRawMessage(TArrayView<const uint8> Data)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput SendRawMessage failed: port not open."));
		return false;
	}

	const stdx::error Err = MidiOut->send_message(
		std::span<const unsigned char>(Data.GetData(), Data.Num()));
	return Err == stdx::error{};
}

bool ULibremidiOutput::SendRawUmpMessage(TArrayView<const uint32> Data)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput SendRawUmpMessage failed: port not open."));
		return false;
	}

	const stdx::error Err = MidiOut->send_ump(
		std::span<const uint32_t>(Data.GetData(), Data.Num()));
	return Err == stdx::error{};
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

void ULibremidiOutput::HandleError(std::string_view ErrorText, const libremidi::source_location& Location)
{
	FString Text = FString::Printf(TEXT("%s [%s:%d]"),
		UTF8_TO_TCHAR(ErrorText.data()),
		UTF8_TO_TCHAR(Location.file_name()),
		Location.line());
	UE_LOG(LogLibremidi4UE, Error, TEXT("LibremidiOutput Error: %s"), *Text);
	OnError.Broadcast(this, Text);
}

void ULibremidiOutput::HandleWarning(std::string_view WarningText, const libremidi::source_location& Location)
{
	FString Text = FString::Printf(TEXT("%s [%s:%d]"),
		UTF8_TO_TCHAR(WarningText.data()),
		UTF8_TO_TCHAR(Location.file_name()),
		Location.line());
	UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiOutput Warning: %s"), *Text);
	OnWarning.Broadcast(this, Text);
}
