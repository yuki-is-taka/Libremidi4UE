// Copyright (C) YUKI TAKA. All Rights Reserved.
//
// LibremidiMessage.cpp

#include "LibremidiMessage.h"

// ---------------------------------------------------------------------------
// FLibremidiMidi1Message — MIDI 1.0 message type mapping
// ---------------------------------------------------------------------------

ELibremidiMessageType FLibremidiMidi1Message::GetMessageType() const
{
	const libremidi::message_type Mt = Msg.get_message_type();
	switch (Mt)
	{
	case libremidi::message_type::NOTE_OFF:           return ELibremidiMessageType::NoteOff;
	case libremidi::message_type::NOTE_ON:            return ELibremidiMessageType::NoteOn;
	case libremidi::message_type::POLY_PRESSURE:      return ELibremidiMessageType::PolyPressure;
	case libremidi::message_type::CONTROL_CHANGE:     return ELibremidiMessageType::ControlChange;
	case libremidi::message_type::PROGRAM_CHANGE:     return ELibremidiMessageType::ProgramChange;
	case libremidi::message_type::AFTERTOUCH:         return ELibremidiMessageType::Aftertouch;
	case libremidi::message_type::PITCH_BEND:         return ELibremidiMessageType::PitchBend;
	case libremidi::message_type::SYSTEM_EXCLUSIVE:   return ELibremidiMessageType::SystemExclusive;
	case libremidi::message_type::TIME_CODE:          return ELibremidiMessageType::TimeCode;
	case libremidi::message_type::SONG_POS_POINTER:   return ELibremidiMessageType::SongPosition;
	case libremidi::message_type::SONG_SELECT:        return ELibremidiMessageType::SongSelect;
	case libremidi::message_type::TUNE_REQUEST:       return ELibremidiMessageType::TuneRequest;
	case libremidi::message_type::EOX:                return ELibremidiMessageType::EndOfExclusive;
	case libremidi::message_type::TIME_CLOCK:         return ELibremidiMessageType::Clock;
	case libremidi::message_type::START:              return ELibremidiMessageType::Start;
	case libremidi::message_type::CONTINUE:           return ELibremidiMessageType::Continue;
	case libremidi::message_type::STOP:               return ELibremidiMessageType::Stop;
	case libremidi::message_type::ACTIVE_SENSING:     return ELibremidiMessageType::ActiveSensing;
	case libremidi::message_type::SYSTEM_RESET:       return ELibremidiMessageType::SystemReset;
	default:                                          return ELibremidiMessageType::Invalid;
	}
}

// ---------------------------------------------------------------------------
// FLibremidiUmpMessage — UMP message type mapping
// ---------------------------------------------------------------------------

ELibremidiUmpMessageType FLibremidiUmpMessage::GetMessageType() const
{
	const libremidi::midi2::message_type Mt = Msg.get_type();
	switch (Mt)
	{
	case libremidi::midi2::message_type::UTILITY:          return ELibremidiUmpMessageType::Utility;
	case libremidi::midi2::message_type::SYSTEM:           return ELibremidiUmpMessageType::System;
	case libremidi::midi2::message_type::MIDI_1_CHANNEL:   return ELibremidiUmpMessageType::Midi1Channel;
	case libremidi::midi2::message_type::SYSEX7:           return ELibremidiUmpMessageType::Sysex7;
	case libremidi::midi2::message_type::MIDI_2_CHANNEL:   return ELibremidiUmpMessageType::Midi2Channel;
	case libremidi::midi2::message_type::SYSEX8_MDS:       return ELibremidiUmpMessageType::Sysex8Mds;
	default:                                               return ELibremidiUmpMessageType::Utility;
	}
}
