#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LibremidiTypes.h"
#include "LibremidiUtilities.generated.h"

/**
 * Blueprint Function Library for MIDI utility functions
 * Provides helper functions for MIDI operations
 */
UCLASS()
class LIBREMIDI4UE_API ULibremidiUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// Note Name Conversion
	// ============================================================================

	/**
	 * Convert a MIDI note number to note name (e.g., 60 -> "C4")
	 * @param NoteNumber MIDI note number (0-127)
	 * @return Note name
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Note Number To Name"))
	static FString NoteNumberToName(int32 NoteNumber);

	/**
	 * Convert a note name to MIDI note number (e.g., "C4" -> 60)
	 * @param NoteName Note name (e.g., "C4", "C#4", "Db4")
	 * @return MIDI note number, or -1 if invalid
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Note Name To Number"))
	static int32 NoteNameToNumber(const FString& NoteName);

	/**
	 * Get the frequency in Hz for a MIDI note number
	 * @param NoteNumber MIDI note number (0-127)
	 * @return Frequency in Hz (A4 = 440Hz)
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Note Number To Frequency"))
	static float NoteNumberToFrequency(int32 NoteNumber);

	/**
	 * Get the MIDI note number closest to a frequency in Hz
	 * @param Frequency Frequency in Hz
	 * @return MIDI note number (0-127)
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Frequency To Note Number"))
	static int32 FrequencyToNoteNumber(float Frequency);

	// ============================================================================
	// Value Normalization
	// ============================================================================

	/**
	 * Normalize a 7-bit MIDI value (0-127) to 0.0-1.0 range
	 * @param Value MIDI value (0-127)
	 * @return Normalized value (0.0-1.0)
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Normalize 7-bit"))
	static float Normalize7Bit(int32 Value);

	/**
	 * Denormalize a 0.0-1.0 value to 7-bit MIDI range (0-127)
	 * @param NormalizedValue Normalized value (0.0-1.0)
	 * @return MIDI value (0-127)
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Denormalize 7-bit"))
	static int32 Denormalize7Bit(float NormalizedValue);

	/**
	 * Normalize a 14-bit MIDI value (0-16383) to 0.0-1.0 range
	 * @param Value MIDI value (0-16383)
	 * @return Normalized value (0.0-1.0)
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Normalize 14-bit"))
	static float Normalize14Bit(int32 Value);

	/**
	 * Denormalize a 0.0-1.0 value to 14-bit MIDI range (0-16383)
	 * @param NormalizedValue Normalized value (0.0-1.0)
	 * @return MIDI value (0-16383)
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Denormalize 14-bit"))
	static int32 Denormalize14Bit(float NormalizedValue);

	/**
	 * Normalize a pitch bend value (-8192 to 8191) to -1.0 to 1.0 range
	 * @param Value Pitch bend value (-8192 to 8191)
	 * @return Normalized value (-1.0 to 1.0)
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Normalize Pitch Bend"))
	static float NormalizePitchBend(int32 Value);

	/**
	 * Denormalize a -1.0 to 1.0 value to pitch bend range (-8192 to 8191)
	 * @param NormalizedValue Normalized value (-1.0 to 1.0)
	 * @return Pitch bend value (-8192 to 8191)
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Denormalize Pitch Bend"))
	static int32 DenormalizePitchBend(float NormalizedValue);

	// ============================================================================
	// Common Operations
	// ============================================================================

	/**
	 * Check if a value is in the valid 7-bit MIDI range (0-127)
	 * @param Value Value to check
	 * @return True if valid
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Is Valid 7-bit"))
	static bool IsValid7Bit(int32 Value);

	/**
	 * Check if a value is in the valid MIDI channel range (0-15)
	 * @param Channel Channel to check
	 * @return True if valid
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Utilities", meta = (DisplayName = "Is Valid Channel"))
	static bool IsValidChannel(int32 Channel);

private:
	static const TArray<FString> NoteNames;
};
