#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LibremidiTypes.h"
#include "LibremidiMessageParser.generated.h"

/**
 * Blueprint Function Library for parsing MIDI 1.0 messages
 * Provides easy-to-use functions for extracting information from MIDI messages
 */
UCLASS()
class LIBREMIDI4UE_API ULibremidiMessageParser : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// Message Type Detection
	// ============================================================================

	/**
	 * Get the type of a MIDI message
	 * @param Data MIDI message bytes
	 * @return Message type
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Get Message Type"))
	static ELibremidiMessageType GetMessageType(const TArray<uint8>& Data);

	/**
	 * Check if a MIDI message is valid
	 * @param Data MIDI message bytes
	 * @return True if the message is valid
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Is Valid MIDI Message"))
	static bool IsValidMidiMessage(const TArray<uint8>& Data);

	/**
	 * Get the MIDI channel from a message (0-15)
	 * @param Data MIDI message bytes
	 * @return Channel number (0-15), or -1 if not a channel message
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Get Channel"))
	static int32 GetChannel(const TArray<uint8>& Data);

	// ============================================================================
	// Note Messages
	// ============================================================================

	/**
	 * Parse a Note On or Note Off message
	 * @param Data MIDI message bytes
	 * @param OutChannel Output channel (0-15)
	 * @param OutNote Output note number (0-127)
	 * @param OutVelocity Output velocity (0-127)
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Parse Note Message"))
	static bool ParseNoteMessage(const TArray<uint8>& Data, int32& OutChannel, int32& OutNote, int32& OutVelocity);

	/**
	 * Check if a message is a Note On
	 * @param Data MIDI message bytes
	 * @return True if the message is Note On
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Is Note On"))
	static bool IsNoteOn(const TArray<uint8>& Data);

	/**
	 * Check if a message is a Note Off
	 * @param Data MIDI message bytes
	 * @return True if the message is Note Off
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Is Note Off"))
	static bool IsNoteOff(const TArray<uint8>& Data);

	// ============================================================================
	// Control Change
	// ============================================================================

	/**
	 * Parse a Control Change message
	 * @param Data MIDI message bytes
	 * @param OutChannel Output channel (0-15)
	 * @param OutController Output controller number (0-127)
	 * @param OutValue Output controller value (0-127)
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Parse Control Change"))
	static bool ParseControlChange(const TArray<uint8>& Data, int32& OutChannel, int32& OutController, int32& OutValue);

	// ============================================================================
	// Program Change
	// ============================================================================

	/**
	 * Parse a Program Change message
	 * @param Data MIDI message bytes
	 * @param OutChannel Output channel (0-15)
	 * @param OutProgram Output program number (0-127)
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Parse Program Change"))
	static bool ParseProgramChange(const TArray<uint8>& Data, int32& OutChannel, int32& OutProgram);

	// ============================================================================
	// Pitch Bend
	// ============================================================================

	/**
	 * Parse a Pitch Bend message
	 * @param Data MIDI message bytes
	 * @param OutChannel Output channel (0-15)
	 * @param OutValue Output pitch bend value (-8192 to 8191)
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Parse Pitch Bend"))
	static bool ParsePitchBend(const TArray<uint8>& Data, int32& OutChannel, int32& OutValue);

	// ============================================================================
	// Pressure (Aftertouch)
	// ============================================================================

	/**
	 * Parse a Channel Pressure (Aftertouch) message
	 * @param Data MIDI message bytes
	 * @param OutChannel Output channel (0-15)
	 * @param OutPressure Output pressure value (0-127)
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Parse Channel Pressure"))
	static bool ParseChannelPressure(const TArray<uint8>& Data, int32& OutChannel, int32& OutPressure);

	/**
	 * Parse a Polyphonic Key Pressure (Poly Aftertouch) message
	 * @param Data MIDI message bytes
	 * @param OutChannel Output channel (0-15)
	 * @param OutNote Output note number (0-127)
	 * @param OutPressure Output pressure value (0-127)
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Parser", meta = (DisplayName = "Parse Poly Pressure"))
	static bool ParsePolyPressure(const TArray<uint8>& Data, int32& OutChannel, int32& OutNote, int32& OutPressure);
};
