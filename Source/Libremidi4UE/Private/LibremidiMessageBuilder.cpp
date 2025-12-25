#include "LibremidiMessageBuilder.h"

// Channel Voice Messages

TArray<uint8> ULibremidiMessageBuilder::MakeNoteOn(int32 Channel, int32 Note, int32 Velocity)
{
	TArray<uint8> Message;
	Message.Reserve(3);
	Message.Add(0x90 | ClampMidi4Bit(Channel));
	Message.Add(ClampMidi7Bit(Note));
	Message.Add(ClampMidi7Bit(Velocity));
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeNoteOff(int32 Channel, int32 Note, int32 Velocity)
{
	TArray<uint8> Message;
	Message.Reserve(3);
	Message.Add(0x80 | ClampMidi4Bit(Channel));
	Message.Add(ClampMidi7Bit(Note));
	Message.Add(ClampMidi7Bit(Velocity));
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeControlChange(int32 Channel, int32 Controller, int32 Value)
{
	TArray<uint8> Message;
	Message.Reserve(3);
	Message.Add(0xB0 | ClampMidi4Bit(Channel));
	Message.Add(ClampMidi7Bit(Controller));
	Message.Add(ClampMidi7Bit(Value));
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeProgramChange(int32 Channel, int32 Program)
{
	TArray<uint8> Message;
	Message.Reserve(2);
	Message.Add(0xC0 | ClampMidi4Bit(Channel));
	Message.Add(ClampMidi7Bit(Program));
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakePitchBend(int32 Channel, int32 Value)
{
	// Clamp to -8192 to 8191, then convert to 0-16383
	const int32 ClampedValue = FMath::Clamp(Value, -8192, 8191) + 8192;
	const uint8 LSB = ClampedValue & 0x7F;
	const uint8 MSB = (ClampedValue >> 7) & 0x7F;
	
	TArray<uint8> Message;
	Message.Reserve(3);
	Message.Add(0xE0 | ClampMidi4Bit(Channel));
	Message.Add(LSB);
	Message.Add(MSB);
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeChannelPressure(int32 Channel, int32 Pressure)
{
	TArray<uint8> Message;
	Message.Reserve(2);
	Message.Add(0xD0 | ClampMidi4Bit(Channel));
	Message.Add(ClampMidi7Bit(Pressure));
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakePolyPressure(int32 Channel, int32 Note, int32 Pressure)
{
	TArray<uint8> Message;
	Message.Reserve(3);
	Message.Add(0xA0 | ClampMidi4Bit(Channel));
	Message.Add(ClampMidi7Bit(Note));
	Message.Add(ClampMidi7Bit(Pressure));
	return Message;
}

// System Common Messages

TArray<uint8> ULibremidiMessageBuilder::MakeSysEx(const TArray<uint8>& Data)
{
	TArray<uint8> Message;
	Message.Reserve(Data.Num() + 2);
	Message.Add(0xF0); // SysEx Start
	Message.Append(Data);
	Message.Add(0xF7); // SysEx End
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeSongPosition(int32 Beats)
{
	const int32 ClampedBeats = FMath::Clamp(Beats, 0, 16383);
	const uint8 LSB = ClampedBeats & 0x7F;
	const uint8 MSB = (ClampedBeats >> 7) & 0x7F;
	
	TArray<uint8> Message;
	Message.Reserve(3);
	Message.Add(0xF2);
	Message.Add(LSB);
	Message.Add(MSB);
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeSongSelect(int32 Song)
{
	TArray<uint8> Message;
	Message.Reserve(2);
	Message.Add(0xF3);
	Message.Add(ClampMidi7Bit(Song));
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeTuneRequest()
{
	TArray<uint8> Message;
	Message.Add(0xF6);
	return Message;
}

// System Real-Time Messages

TArray<uint8> ULibremidiMessageBuilder::MakeMidiClock()
{
	TArray<uint8> Message;
	Message.Add(0xF8);
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeStart()
{
	TArray<uint8> Message;
	Message.Add(0xFA);
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeContinue()
{
	TArray<uint8> Message;
	Message.Add(0xFB);
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeStop()
{
	TArray<uint8> Message;
	Message.Add(0xFC);
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeActiveSensing()
{
	TArray<uint8> Message;
	Message.Add(0xFE);
	return Message;
}

TArray<uint8> ULibremidiMessageBuilder::MakeSystemReset()
{
	TArray<uint8> Message;
	Message.Add(0xFF);
	return Message;
}
