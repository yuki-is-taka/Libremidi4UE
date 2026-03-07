// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.
//
// UE-friendly wrappers for libremidi MIDI messages.
// Two separate types for MIDI 1.0 and UMP — no runtime branching needed.
// Follows the same pattern as FLibremidiPortInfo: hold the native type
// privately, expose UE-friendly getters that decode on demand.
//
// SysEx reassembly is NOT handled here — that is IMFMIDI2Input's responsibility.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/message.hpp>
#include <libremidi/ump.hpp>
THIRD_PARTY_INCLUDES_END

#include "LibremidiMessage.generated.h"

// ---------------------------------------------------------------------------
// ELibremidiMessageType — MIDI 1.0 message type
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ELibremidiMessageType : uint8
{
	Invalid = 0         UMETA(DisplayName = "Invalid"),
	NoteOff             UMETA(DisplayName = "Note Off"),
	NoteOn              UMETA(DisplayName = "Note On"),
	PolyPressure        UMETA(DisplayName = "Polyphonic Pressure"),
	ControlChange       UMETA(DisplayName = "Control Change"),
	ProgramChange       UMETA(DisplayName = "Program Change"),
	Aftertouch          UMETA(DisplayName = "Aftertouch"),
	PitchBend           UMETA(DisplayName = "Pitch Bend"),
	SystemExclusive     UMETA(DisplayName = "System Exclusive"),
	TimeCode            UMETA(DisplayName = "Time Code"),
	SongPosition        UMETA(DisplayName = "Song Position"),
	SongSelect          UMETA(DisplayName = "Song Select"),
	TuneRequest         UMETA(DisplayName = "Tune Request"),
	EndOfExclusive      UMETA(DisplayName = "End of Exclusive"),
	Clock               UMETA(DisplayName = "Clock"),
	Start               UMETA(DisplayName = "Start"),
	Continue            UMETA(DisplayName = "Continue"),
	Stop                UMETA(DisplayName = "Stop"),
	ActiveSensing       UMETA(DisplayName = "Active Sensing"),
	SystemReset         UMETA(DisplayName = "System Reset"),
};

// ---------------------------------------------------------------------------
// ELibremidiUmpMessageType — UMP message type (bits 31-28 of word 0)
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ELibremidiUmpMessageType : uint8
{
	Utility             UMETA(DisplayName = "Utility"),
	System              UMETA(DisplayName = "System"),
	Midi1Channel        UMETA(DisplayName = "MIDI 1.0 Channel Voice"),
	Sysex7              UMETA(DisplayName = "SysEx 7-bit"),
	Midi2Channel        UMETA(DisplayName = "MIDI 2.0 Channel Voice"),
	Sysex8Mds           UMETA(DisplayName = "SysEx 8-bit / MDS"),
};

// ---------------------------------------------------------------------------
// FLibremidiMidi1Message — MIDI 1.0 message wrapper
// ---------------------------------------------------------------------------
//
// Wraps libremidi::message. One-to-one with the MIDI 1.0 callback path
// (input_configuration::on_message).

USTRUCT(BlueprintType)
struct LIBREMIDI4UE_API FLibremidiMidi1Message
{
	GENERATED_BODY()

	FLibremidiMidi1Message() = default;

	/** Construct from MIDI 1.0 message (move). */
	explicit FLibremidiMidi1Message(libremidi::message&& InMsg)
		: Msg(MoveTemp(InMsg)) {}

	/** Construct from MIDI 1.0 message (copy). */
	explicit FLibremidiMidi1Message(const libremidi::message& InMsg)
		: Msg(InMsg) {}

	// --- Native access ---

	/** Access the underlying libremidi::message. */
	const libremidi::message& GetNativeMessage() const { return Msg; }

	// --- Timestamp ---

	/** Timestamp (nanoseconds). Interpretation depends on timestamp_mode. */
	int64 GetTimestamp() const { return static_cast<int64>(Msg.timestamp); }

	// --- Getters ---

	/** Raw byte data. */
	TArrayView<const uint8> GetRawBytes() const
	{
		return TArrayView<const uint8>(Msg.bytes.data(), Msg.bytes.size());
	}

	/** Number of bytes. */
	int32 GetByteCount() const { return static_cast<int32>(Msg.bytes.size()); }

	/** Status byte (first byte). Returns 0 if empty. */
	uint8 GetStatusByte() const { return Msg.empty() ? 0 : Msg.bytes[0]; }

	/** Decoded message type. */
	ELibremidiMessageType GetMessageType() const;

	/**
	 * MIDI channel (1-based, 1..16).
	 * Returns 0 for non-channel messages (System, Realtime, etc.).
	 */
	int32 GetChannel() const
	{
		if (Msg.empty()) { return 0; }
		const uint8 Hi = Msg.bytes[0] & 0xF0;
		if (Hi < 0x80 || Hi > 0xE0) { return 0; }
		return static_cast<int32>(Msg.bytes[0] & 0x0F) + 1;
	}

	/** True if this is a Note On or Note Off message. */
	bool IsNoteOnOrOff() const { return Msg.is_note_on_or_off(); }

private:
	libremidi::message Msg;
};

// ---------------------------------------------------------------------------
// FLibremidiUmpMessage — UMP (MIDI 2.0) message wrapper
// ---------------------------------------------------------------------------
//
// Wraps libremidi::ump. One-to-one with the UMP callback path
// (ump_input_configuration::on_message).

USTRUCT(BlueprintType)
struct LIBREMIDI4UE_API FLibremidiUmpMessage
{
	GENERATED_BODY()

	FLibremidiUmpMessage() = default;

	/** Construct from UMP message (move). */
	explicit FLibremidiUmpMessage(libremidi::ump&& InMsg)
		: Msg(MoveTemp(InMsg)) {}

	/** Construct from UMP message (copy). */
	explicit FLibremidiUmpMessage(const libremidi::ump& InMsg)
		: Msg(InMsg) {}

	// --- Native access ---

	/** Access the underlying libremidi::ump. */
	const libremidi::ump& GetNativeMessage() const { return Msg; }

	// --- Timestamp ---

	/** Timestamp (nanoseconds). Interpretation depends on timestamp_mode. */
	int64 GetTimestamp() const { return static_cast<int64>(Msg.timestamp); }

	// --- Getters ---

	/** Raw 32-bit words. Length is GetWordCount(). */
	TArrayView<const uint32> GetRawWords() const
	{
		return TArrayView<const uint32>(Msg.data, Msg.size());
	}

	/** Number of 32-bit words (1, 2, or 4). */
	int32 GetWordCount() const { return static_cast<int32>(Msg.size()); }

	/** UMP message type (bits 31–28 of word 0). */
	ELibremidiUmpMessageType GetMessageType() const;

	/** Group (0-based, bits 27–24 of word 0). */
	int32 GetGroup() const { return static_cast<int32>(Msg.get_group()); }

	/**
	 * Channel (1-based, 1..16).
	 * libremidi::ump::get_channel() already returns 1-based.
	 */
	int32 GetChannel() const { return static_cast<int32>(Msg.get_channel()); }

	/** Status code (bits 23–20 of word 0, for channel voice messages). */
	uint8 GetStatusCode() const { return static_cast<uint8>(Msg.get_status_code()); }

private:
	libremidi::ump Msg;
};
