#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LibremidiMessageBuilder.generated.h"

/**
 * Blueprint Function Library for creating MIDI 1.0 messages
 * Provides easy-to-use functions for building MIDI messages without manual byte array construction
 */
UCLASS()
class LIBREMIDI4UE_API ULibremidiMessageBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// Channel Voice Messages
	// ============================================================================

	/**
	 * Create a Note On message
	 * @param Channel MIDI channel (0-15)
	 * @param Note Note number (0-127)
	 * @param Velocity Note velocity (0-127)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Note On"))
	static TArray<uint8> MakeNoteOn(int32 Channel, int32 Note, int32 Velocity);

	/**
	 * Create a Note Off message
	 * @param Channel MIDI channel (0-15)
	 * @param Note Note number (0-127)
	 * @param Velocity Release velocity (0-127)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Note Off"))
	static TArray<uint8> MakeNoteOff(int32 Channel, int32 Note, int32 Velocity);

	/**
	 * Create a Control Change message
	 * @param Channel MIDI channel (0-15)
	 * @param Controller Controller number (0-127)
	 * @param Value Controller value (0-127)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Control Change"))
	static TArray<uint8> MakeControlChange(int32 Channel, int32 Controller, int32 Value);

	/**
	 * Create a Program Change message
	 * @param Channel MIDI channel (0-15)
	 * @param Program Program number (0-127)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Program Change"))
	static TArray<uint8> MakeProgramChange(int32 Channel, int32 Program);

	/**
	 * Create a Pitch Bend message
	 * @param Channel MIDI channel (0-15)
	 * @param Value Pitch bend value (-8192 to 8191, 0 = center)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Pitch Bend"))
	static TArray<uint8> MakePitchBend(int32 Channel, int32 Value);

	/**
	 * Create a Channel Pressure (Aftertouch) message
	 * @param Channel MIDI channel (0-15)
	 * @param Pressure Pressure value (0-127)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Channel Pressure"))
	static TArray<uint8> MakeChannelPressure(int32 Channel, int32 Pressure);

	/**
	 * Create a Polyphonic Key Pressure (Poly Aftertouch) message
	 * @param Channel MIDI channel (0-15)
	 * @param Note Note number (0-127)
	 * @param Pressure Pressure value (0-127)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Poly Pressure"))
	static TArray<uint8> MakePolyPressure(int32 Channel, int32 Note, int32 Pressure);

	// ============================================================================
	// System Common Messages
	// ============================================================================

	/**
	 * Create a System Exclusive (SysEx) message
	 * @param Data SysEx data (without F0 start and F7 end bytes - they will be added automatically)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make SysEx"))
	static TArray<uint8> MakeSysEx(const TArray<uint8>& Data);

	/**
	 * Create a Song Position Pointer message
	 * @param Beats Song position in MIDI beats (0-16383)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Song Position"))
	static TArray<uint8> MakeSongPosition(int32 Beats);

	/**
	 * Create a Song Select message
	 * @param Song Song number (0-127)
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Song Select"))
	static TArray<uint8> MakeSongSelect(int32 Song);

	/**
	 * Create a Tune Request message
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Tune Request"))
	static TArray<uint8> MakeTuneRequest();

	// ============================================================================
	// System Real-Time Messages
	// ============================================================================

	/**
	 * Create a MIDI Clock (Timing Clock) message
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make MIDI Clock"))
	static TArray<uint8> MakeMidiClock();

	/**
	 * Create a Start message
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Start"))
	static TArray<uint8> MakeStart();

	/**
	 * Create a Continue message
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Continue"))
	static TArray<uint8> MakeContinue();

	/**
	 * Create a Stop message
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Stop"))
	static TArray<uint8> MakeStop();

	/**
	 * Create an Active Sensing message
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make Active Sensing"))
	static TArray<uint8> MakeActiveSensing();

	/**
	 * Create a System Reset message
	 * @return MIDI message bytes
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Message Builder", meta = (DisplayName = "Make System Reset"))
	static TArray<uint8> MakeSystemReset();

private:
	static constexpr uint8 ClampMidi7Bit(int32 Value) { return static_cast<uint8>(FMath::Clamp(Value, 0, 127)); }
	static constexpr uint8 ClampMidi4Bit(int32 Value) { return static_cast<uint8>(FMath::Clamp(Value, 0, 15)); }
};
