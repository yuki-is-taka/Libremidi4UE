#include "LibremidiMessageParser.h"

// Message Type Detection

ELibremidiMessageType ULibremidiMessageParser::GetMessageType(const TArray<uint8>& Data)
{
	if (Data.Num() == 0)
	{
		return ELibremidiMessageType::Unknown;
	}

	const uint8 Status = Data[0];

	// System messages (0xF0-0xFF)
	if (Status >= 0xF0)
	{
		if (Status == 0xF0 || Status == 0xF7)
		{
			return ELibremidiMessageType::SystemExclusive;
		}
		else if (Status >= 0xF8)
		{
			return ELibremidiMessageType::SystemRealTime;
		}
		else
		{
			return ELibremidiMessageType::SystemCommon;
		}
	}

	// Channel messages (0x80-0xEF)
	const uint8 MessageType = Status & 0xF0;
	switch (MessageType)
	{
	case 0x80: return ELibremidiMessageType::NoteOff;
	case 0x90: return ELibremidiMessageType::NoteOn;
	case 0xA0: return ELibremidiMessageType::PolyPressure;
	case 0xB0: return ELibremidiMessageType::ControlChange;
	case 0xC0: return ELibremidiMessageType::ProgramChange;
	case 0xD0: return ELibremidiMessageType::ChannelPressure;
	case 0xE0: return ELibremidiMessageType::PitchBend;
	default: return ELibremidiMessageType::Unknown;
	}
}

bool ULibremidiMessageParser::IsValidMidiMessage(const TArray<uint8>& Data)
{
	if (Data.Num() == 0)
	{
		return false;
	}

	const uint8 Status = Data[0];

	// Status byte must have bit 7 set (0x80-0xFF)
	if ((Status & 0x80) == 0)
	{
		return false;
	}

	// System Real-Time messages (1 byte)
	if (Status >= 0xF8)
	{
		return Data.Num() == 1;
	}

	// System Common messages
	if (Status >= 0xF0)
	{
		switch (Status)
		{
		case 0xF0: // SysEx Start
		case 0xF7: // SysEx End
			return Data.Num() >= 1;
		case 0xF1: // MIDI Time Code Quarter Frame
		case 0xF3: // Song Select
			return Data.Num() == 2;
		case 0xF2: // Song Position Pointer
			return Data.Num() == 3;
		case 0xF6: // Tune Request
			return Data.Num() == 1;
		default:
			return false;
		}
	}

	// Channel messages
	const uint8 MessageType = Status & 0xF0;
	switch (MessageType)
	{
	case 0xC0: // Program Change
	case 0xD0: // Channel Pressure
		return Data.Num() == 2;
	case 0x80: // Note Off
	case 0x90: // Note On
	case 0xA0: // Poly Pressure
	case 0xB0: // Control Change
	case 0xE0: // Pitch Bend
		return Data.Num() == 3;
	default:
		return false;
	}
}

int32 ULibremidiMessageParser::GetChannel(const TArray<uint8>& Data)
{
	if (Data.Num() == 0)
	{
		return -1;
	}

	const uint8 Status = Data[0];

	// System messages don't have channels
	if (Status >= 0xF0)
	{
		return -1;
	}

	// Channel messages have channel in lower 4 bits
	return Status & 0x0F;
}

// Note Messages

bool ULibremidiMessageParser::ParseNoteMessage(const TArray<uint8>& Data, int32& OutChannel, int32& OutNote, int32& OutVelocity)
{
	if (Data.Num() < 3)
	{
		return false;
	}

	const uint8 Status = Data[0];
	const uint8 MessageType = Status & 0xF0;

	if (MessageType != 0x80 && MessageType != 0x90)
	{
		return false;
	}

	OutChannel = Status & 0x0F;
	OutNote = Data[1] & 0x7F;
	OutVelocity = Data[2] & 0x7F;
	return true;
}

bool ULibremidiMessageParser::IsNoteOn(const TArray<uint8>& Data)
{
	if (Data.Num() < 3)
	{
		return false;
	}

	const uint8 Status = Data[0];
	const uint8 MessageType = Status & 0xF0;
	const uint8 Velocity = Data[2];

	// Note On with velocity 0 is actually Note Off
	return MessageType == 0x90 && Velocity > 0;
}

bool ULibremidiMessageParser::IsNoteOff(const TArray<uint8>& Data)
{
	if (Data.Num() < 3)
	{
		return false;
	}

	const uint8 Status = Data[0];
	const uint8 MessageType = Status & 0xF0;
	const uint8 Velocity = Data[2];

	// Note Off, or Note On with velocity 0
	return MessageType == 0x80 || (MessageType == 0x90 && Velocity == 0);
}

// Control Change

bool ULibremidiMessageParser::ParseControlChange(const TArray<uint8>& Data, int32& OutChannel, int32& OutController, int32& OutValue)
{
	if (Data.Num() < 3)
	{
		return false;
	}

	const uint8 Status = Data[0];
	const uint8 MessageType = Status & 0xF0;

	if (MessageType != 0xB0)
	{
		return false;
	}

	OutChannel = Status & 0x0F;
	OutController = Data[1] & 0x7F;
	OutValue = Data[2] & 0x7F;
	return true;
}

// Program Change

bool ULibremidiMessageParser::ParseProgramChange(const TArray<uint8>& Data, int32& OutChannel, int32& OutProgram)
{
	if (Data.Num() < 2)
	{
		return false;
	}

	const uint8 Status = Data[0];
	const uint8 MessageType = Status & 0xF0;

	if (MessageType != 0xC0)
	{
		return false;
	}

	OutChannel = Status & 0x0F;
	OutProgram = Data[1] & 0x7F;
	return true;
}

// Pitch Bend

bool ULibremidiMessageParser::ParsePitchBend(const TArray<uint8>& Data, int32& OutChannel, int32& OutValue)
{
	if (Data.Num() < 3)
	{
		return false;
	}

	const uint8 Status = Data[0];
	const uint8 MessageType = Status & 0xF0;

	if (MessageType != 0xE0)
	{
		return false;
	}

	OutChannel = Status & 0x0F;
	
	// Combine LSB and MSB into 14-bit value (0-16383), then center at 0 (-8192 to 8191)
	const int32 Value14Bit = (Data[1] & 0x7F) | ((Data[2] & 0x7F) << 7);
	OutValue = Value14Bit - 8192;
	
	return true;
}

// Pressure (Aftertouch)

bool ULibremidiMessageParser::ParseChannelPressure(const TArray<uint8>& Data, int32& OutChannel, int32& OutPressure)
{
	if (Data.Num() < 2)
	{
		return false;
	}

	const uint8 Status = Data[0];
	const uint8 MessageType = Status & 0xF0;

	if (MessageType != 0xD0)
	{
		return false;
	}

	OutChannel = Status & 0x0F;
	OutPressure = Data[1] & 0x7F;
	return true;
}

bool ULibremidiMessageParser::ParsePolyPressure(const TArray<uint8>& Data, int32& OutChannel, int32& OutNote, int32& OutPressure)
{
	if (Data.Num() < 3)
	{
		return false;
	}

	const uint8 Status = Data[0];
	const uint8 MessageType = Status & 0xF0;

	if (MessageType != 0xA0)
	{
		return false;
	}

	OutChannel = Status & 0x0F;
	OutNote = Data[1] & 0x7F;
	OutPressure = Data[2] & 0x7F;
	return true;
}
