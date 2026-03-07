// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "LibremidiSettings.h"
#include "LibremidiTypes.h"
#include "LibremidiMessage.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/input_configuration.hpp>
THIRD_PARTY_INCLUDES_END
#include "LibremidiInput.generated.h"

class ULibremidiInput;

// ---------------------------------------------------------------------------
// Delegate types
// ---------------------------------------------------------------------------

/** Fired when a MIDI 1.0 message is received. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLibremidiMidi1Message, ULibremidiInput* /*Source*/, const FLibremidiMidi1Message& /*Message*/);

/** Fired when a UMP (MIDI 2.0) message is received. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLibremidiUmpMessage, ULibremidiInput* /*Source*/, const FLibremidiUmpMessage& /*Message*/);

// FOnLibremidiDiagnostic is declared in LibremidiTypes.h (shared by Input and Output).

class ULibremidiEngineSubsystem;
namespace libremidi
{
	class midi_in;
}

UCLASS(BlueprintType, Blueprintable)
class LIBREMIDI4UE_API ULibremidiInput : public UObject
{
	GENERATED_BODY()

public:
	/** Factory: creates the object and calls Initialize with the provided settings. */
	static ULibremidiInput* Create(
		UObject* Outer,
		ELibremidiMidiProtocol Protocol,
		bool bInIgnoreSysex,
		bool bInIgnoreTiming,
		bool bInIgnoreSensing,
		ELibremidiTimestampMode Mode,
		bool bInMidi1ChannelEventsToMidi2);
	/** Return the owning engine subsystem (outer) that created this input. */
	ULibremidiEngineSubsystem* GetOwnerSubsystem() const;

	/** Initialize the input configuration and create midi_in using the observer API from the subsystem. */
	bool Initialize(
		ELibremidiMidiProtocol Protocol,
		bool bInIgnoreSysex,
		bool bInIgnoreTiming,
		bool bInIgnoreSensing,
		ELibremidiTimestampMode Mode,
		bool bInMidi1ChannelEventsToMidi2);
	/** Open a MIDI input port using the current midi_in instance. */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Input")
	bool OpenInput(const FLibremidiInputInfo& PortInfo);
	/** Close the currently open MIDI input port, if any. */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Input")
	void CloseInput();
	/** Returns true when a port has been opened successfully. */
	UFUNCTION(BlueprintPure, Category = "MIDI|Input")
	bool IsPortOpen() const;
	/** Returns true when a port is connected to another port. */
	UFUNCTION(BlueprintPure, Category = "MIDI|Input")
	bool IsPortConnected() const;

	ELibremidiMidiProtocol GetMidiProtocol() const;
	void SetMidiProtocol(ELibremidiMidiProtocol Protocol);
	bool GetIsUmp() const;

	bool GetIgnoreSysex() const;
	void SetIgnoreSysex(bool bIgnore);

	bool GetIgnoreTiming() const;
	void SetIgnoreTiming(bool bIgnore);

	bool GetIgnoreSensing() const;
	void SetIgnoreSensing(bool bIgnore);

	ELibremidiTimestampMode GetTimestampMode() const;
	void SetTimestampMode(ELibremidiTimestampMode Mode);

	bool GetMidi1ChannelEventsToMidi2() const;
	void SetMidi1ChannelEventsToMidi2(bool bEnabled);

	// --- Delegates ---

	/** Broadcast when a MIDI 1.0 message is received (only in MIDI 1.0 protocol mode). */
	FOnLibremidiMidi1Message OnMidi1Message;

	/** Broadcast when a UMP message is received (only in MIDI 2.0 protocol mode). */
	FOnLibremidiUmpMessage OnUmpMessage;

	/** Broadcast when libremidi reports an error. */
	FOnLibremidiDiagnostic OnError;

	/** Broadcast when libremidi reports a warning. */
	FOnLibremidiDiagnostic OnWarning;

private:
	TUniquePtr<libremidi::midi_in> MidiIn;

	/** Timestamp behavior used by libremidi input. */
	libremidi::timestamp_mode TimestampMode = libremidi::timestamp_mode::Absolute;

	/** Ignore incoming SysEx messages when true. */
	bool bIgnoreSysex = true;

	/** Ignore MIDI timing clock messages when true. */
	bool bIgnoreTiming = true;

	/** Ignore active sensing messages when true. */
	bool bIgnoreSensing = true;

	/** Upgrade MIDI 1.0 channel events to MIDI 2.0 when using UMP. */
	bool bMidi1ChannelEventsToMidi2 = true;

	/** Selected MIDI protocol (MIDI 1.0 or MIDI 2.0/UMP). */
	ELibremidiMidiProtocol MidiProtocol = ELibremidiMidiProtocol::Midi2;

	void HandleMessage(libremidi::message&& Message);
	void HandleRawData(std::span<const uint8_t> Data, libremidi::timestamp Timestamp);
	void HandleUmpMessage(libremidi::ump&& Message);
	void HandleUmpRawData(std::span<const uint32_t> Data, libremidi::timestamp Timestamp);
	void HandleError(std::string_view ErrorText, const libremidi::source_location& Location);
	void HandleWarning(std::string_view WarningText, const libremidi::source_location& Location);
};
