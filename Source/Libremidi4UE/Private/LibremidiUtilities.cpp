#include "LibremidiUtilities.h"
#include "Math/UnrealMathUtility.h"

const TArray<FString> ULibremidiUtilities::NoteNames = {
	TEXT("C"), TEXT("C#"), TEXT("D"), TEXT("D#"), TEXT("E"), TEXT("F"),
	TEXT("F#"), TEXT("G"), TEXT("G#"), TEXT("A"), TEXT("A#"), TEXT("B")
};

// Note Name Conversion

FString ULibremidiUtilities::NoteNumberToName(int32 NoteNumber)
{
	if (NoteNumber < 0 || NoteNumber > 127)
	{
		return TEXT("Invalid");
	}

	const int32 Octave = (NoteNumber / 12) - 1;
	const int32 NoteIndex = NoteNumber % 12;
	
	return FString::Printf(TEXT("%s%d"), *NoteNames[NoteIndex], Octave);
}

int32 ULibremidiUtilities::NoteNameToNumber(const FString& NoteName)
{
	if (NoteName.IsEmpty())
	{
		return -1;
	}

	// Parse note name (e.g., "C4", "C#4", "Db4")
	FString NotePart;
	FString OctavePart;
	
	// Find where the octave number starts
	int32 OctaveIndex = 0;
	for (int32 i = 0; i < NoteName.Len(); ++i)
	{
		if (FChar::IsDigit(NoteName[i]) || NoteName[i] == '-')
		{
			OctaveIndex = i;
			break;
		}
	}
	
	if (OctaveIndex == 0)
	{
		return -1;
	}
	
	NotePart = NoteName.Left(OctaveIndex).ToUpper();
	OctavePart = NoteName.Mid(OctaveIndex);
	
	// Parse octave
	int32 Octave = FCString::Atoi(*OctavePart);
	
	// Find note index
	int32 NoteIndex = -1;
	
	// Check for sharp notes
	if (NotePart == TEXT("C")) NoteIndex = 0;
	else if (NotePart == TEXT("C#") || NotePart == TEXT("DB")) NoteIndex = 1;
	else if (NotePart == TEXT("D")) NoteIndex = 2;
	else if (NotePart == TEXT("D#") || NotePart == TEXT("EB")) NoteIndex = 3;
	else if (NotePart == TEXT("E")) NoteIndex = 4;
	else if (NotePart == TEXT("F")) NoteIndex = 5;
	else if (NotePart == TEXT("F#") || NotePart == TEXT("GB")) NoteIndex = 6;
	else if (NotePart == TEXT("G")) NoteIndex = 7;
	else if (NotePart == TEXT("G#") || NotePart == TEXT("AB")) NoteIndex = 8;
	else if (NotePart == TEXT("A")) NoteIndex = 9;
	else if (NotePart == TEXT("A#") || NotePart == TEXT("BB")) NoteIndex = 10;
	else if (NotePart == TEXT("B")) NoteIndex = 11;
	
	if (NoteIndex == -1)
	{
		return -1;
	}
	
	const int32 MidiNote = (Octave + 1) * 12 + NoteIndex;
	
	return (MidiNote >= 0 && MidiNote <= 127) ? MidiNote : -1;
}

float ULibremidiUtilities::NoteNumberToFrequency(int32 NoteNumber)
{
	// A4 (MIDI note 69) = 440 Hz
	// Formula: f = 440 * 2^((n - 69) / 12)
	return 440.0f * FMath::Pow(2.0f, (NoteNumber - 69) / 12.0f);
}

int32 ULibremidiUtilities::FrequencyToNoteNumber(float Frequency)
{
	if (Frequency <= 0.0f)
	{
		return 0;
	}
	
	// Formula: n = 69 + 12 * log2(f / 440)
	const float MidiNote = 69.0f + 12.0f * FMath::Log2(Frequency / 440.0f);
	return FMath::Clamp(FMath::RoundToInt(MidiNote), 0, 127);
}

// Value Normalization

float ULibremidiUtilities::Normalize7Bit(int32 Value)
{
	return FMath::Clamp(Value, 0, 127) / 127.0f;
}

int32 ULibremidiUtilities::Denormalize7Bit(float NormalizedValue)
{
	return FMath::Clamp(FMath::RoundToInt(NormalizedValue * 127.0f), 0, 127);
}

float ULibremidiUtilities::Normalize14Bit(int32 Value)
{
	return FMath::Clamp(Value, 0, 16383) / 16383.0f;
}

int32 ULibremidiUtilities::Denormalize14Bit(float NormalizedValue)
{
	return FMath::Clamp(FMath::RoundToInt(NormalizedValue * 16383.0f), 0, 16383);
}

float ULibremidiUtilities::NormalizePitchBend(int32 Value)
{
	return FMath::Clamp(Value, -8192, 8191) / 8192.0f;
}

int32 ULibremidiUtilities::DenormalizePitchBend(float NormalizedValue)
{
	return FMath::Clamp(FMath::RoundToInt(NormalizedValue * 8192.0f), -8192, 8191);
}

// Common Operations

bool ULibremidiUtilities::IsValid7Bit(int32 Value)
{
	return Value >= 0 && Value <= 127;
}

bool ULibremidiUtilities::IsValidChannel(int32 Channel)
{
	return Channel >= 0 && Channel <= 15;
}
