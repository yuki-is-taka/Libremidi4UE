// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "LibremidiSettings.h"
#include "LibremidiTypes.h"
#include "LibremidiMessage.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/output_configuration.hpp>
THIRD_PARTY_INCLUDES_END
#include "LibremidiOutput.generated.h"

class ULibremidiEngineSubsystem;
namespace libremidi
{
	class midi_out;
}

// FOnLibremidiDiagnostic is declared in LibremidiTypes.h (shared by Input and Output).

UCLASS(BlueprintType, Blueprintable)
class LIBREMIDI4UE_API ULibremidiOutput : public UObject
{
	GENERATED_BODY()

public:
	/** Factory: creates the object and calls Initialize with the provided settings. */
	static ULibremidiOutput* Create(
		UObject* Outer,
		ELibremidiMidiProtocol Protocol,
		ELibremidiTimestampMode Mode);

	/** Return the owning engine subsystem (outer) that created this output. */
	ULibremidiEngineSubsystem* GetOwnerSubsystem() const;

	/** Initialize the output configuration and create midi_out using the observer API from the subsystem. */
	bool Initialize(ELibremidiMidiProtocol Protocol, ELibremidiTimestampMode Mode);
	/** Open a MIDI output port using the current midi_out instance. */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Output")
	bool OpenOutput(const FLibremidiOutputInfo& PortInfo);
	/** Close the currently open MIDI output port, if any. */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Output")
	void CloseOutput();
	/** Returns true when a port has been opened successfully. */
	UFUNCTION(BlueprintPure, Category = "MIDI|Output")
	bool IsPortOpen() const;
	/** Returns true when a port is connected to another port. */
	UFUNCTION(BlueprintPure, Category = "MIDI|Output")
	bool IsPortConnected() const;

	// --- Send (wrapper type — ideal for MIDI Through / forwarding received messages) ---

	/** Send a MIDI 1.0 message. Returns true on success. */
	bool SendMIDIMessage(const FLibremidiMidi1Message& Message);

	/** Send a UMP message. Returns true on success. */
	bool SendUmpMessage(const FLibremidiUmpMessage& Message);

	// --- Send (raw — ideal for programmatic construction, no heap allocation) ---

	/** Send raw MIDI 1.0 bytes. Returns true on success. */
	bool SendRawMessage(TArrayView<const uint8> Data);

	/** Send raw UMP words. Returns true on success. */
	bool SendRawUmpMessage(TArrayView<const uint32> Data);

	ELibremidiMidiProtocol GetMidiProtocol() const;
	void SetMidiProtocol(ELibremidiMidiProtocol Protocol);
	bool GetIsUmp() const;

	ELibremidiTimestampMode GetTimestampMode() const;
	void SetTimestampMode(ELibremidiTimestampMode Mode);

	// --- Delegates ---

	/** Broadcast when libremidi reports an error. */
	FOnLibremidiDiagnostic OnError;

	/** Broadcast when libremidi reports a warning. */
	FOnLibremidiDiagnostic OnWarning;

private:
	TUniquePtr<libremidi::midi_out> MidiOut;

	/** Timestamp behavior used by libremidi output scheduling. */
	libremidi::timestamp_mode TimestampMode = libremidi::timestamp_mode::Absolute;
	/** Selected MIDI protocol (MIDI 1.0 or MIDI 2.0/UMP). */
	ELibremidiMidiProtocol MidiProtocol = ELibremidiMidiProtocol::Midi1;

	void HandleError(std::string_view ErrorText, const libremidi::source_location& Location);
	void HandleWarning(std::string_view WarningText, const libremidi::source_location& Location);
};
