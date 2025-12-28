#include "LibremidiUtilities.h"
#include "LibremidiEngineSubsystem.h"
#include "LibremidiInput.h"
#include "LibremidiOutput.h"
#include "Libremidi4UELog.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/Engine.h"

// ============================================================================
// Static Data
// ============================================================================

const TArray<FString> ULibremidiUtilities::NoteNames = {
	TEXT("C"), TEXT("C#"), TEXT("D"), TEXT("D#"), TEXT("E"), TEXT("F"),
	TEXT("F#"), TEXT("G"), TEXT("G#"), TEXT("A"), TEXT("A#"), TEXT("B")
};

// ============================================================================
// Private Helpers
// ============================================================================

namespace
{
	/** Get the Engine Subsystem singleton - common pattern for all utility functions */
	FORCEINLINE ULibremidiEngineSubsystem* GetSubsystem()
	{
		return GEngine ? GEngine->GetEngineSubsystem<ULibremidiEngineSubsystem>() : nullptr;
	}

	/** Note name lookup table for faster parsing */
	struct FNoteNameLookup
	{
		const TCHAR* Name;
		int32 Index;
	};

	// Sorted by most common notes first for faster lookup
	static const FNoteNameLookup NoteNameTable[] = {
		{ TEXT("C"),  0 }, { TEXT("D"),  2 }, { TEXT("E"),  4 }, { TEXT("F"),  5 },
		{ TEXT("G"),  7 }, { TEXT("A"),  9 }, { TEXT("B"), 11 },
		{ TEXT("C#"), 1 }, { TEXT("D#"), 3 }, { TEXT("F#"), 6 }, { TEXT("G#"), 8 }, { TEXT("A#"), 10 },
		{ TEXT("DB"), 1 }, { TEXT("EB"), 3 }, { TEXT("GB"), 6 }, { TEXT("AB"), 8 }, { TEXT("BB"), 10 },
	};

	static constexpr int32 NoteNameTableSize = UE_ARRAY_COUNT(NoteNameTable);

	/** Find note index from note name string */
	int32 FindNoteIndex(const FString& NotePart)
	{
		for (int32 i = 0; i < NoteNameTableSize; ++i)
		{
			if (NotePart == NoteNameTable[i].Name)
			{
				return NoteNameTable[i].Index;
			}
		}
		return -1;
	}

	/** Normalization constants */
	namespace MidiConstants
	{
		constexpr float Inv127 = 1.0f / 127.0f;
		constexpr float Inv16383 = 1.0f / 16383.0f;
		constexpr float Inv8192 = 1.0f / 8192.0f;
		constexpr float A4Frequency = 440.0f;
		constexpr int32 A4MidiNote = 69;
		constexpr float InvTwelfth = 1.0f / 12.0f;
	}
}

// ============================================================================
// Note Name Conversion
// ============================================================================

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

	// Find where the octave number starts
	int32 OctaveIndex = 0;
	const int32 Len = NoteName.Len();
	
	for (int32 i = 0; i < Len; ++i)
	{
		const TCHAR Ch = NoteName[i];
		if (FChar::IsDigit(Ch) || Ch == TEXT('-'))
		{
			OctaveIndex = i;
			break;
		}
	}
	
	if (OctaveIndex == 0)
	{
		return -1;
	}
	
	const FString NotePart = NoteName.Left(OctaveIndex).ToUpper();
	const int32 Octave = FCString::Atoi(*NoteName + OctaveIndex);
	const int32 NoteIndex = FindNoteIndex(NotePart);
	
	if (NoteIndex == -1)
	{
		return -1;
	}
	
	const int32 MidiNote = (Octave + 1) * 12 + NoteIndex;
	return (MidiNote >= 0 && MidiNote <= 127) ? MidiNote : -1;
}

float ULibremidiUtilities::NoteNumberToFrequency(int32 NoteNumber)
{
	using namespace MidiConstants;
	return A4Frequency * FMath::Pow(2.0f, (NoteNumber - A4MidiNote) * InvTwelfth);
}

int32 ULibremidiUtilities::FrequencyToNoteNumber(float Frequency)
{
	if (Frequency <= 0.0f)
	{
		return 0;
	}
	
	using namespace MidiConstants;
	const float MidiNote = A4MidiNote + 12.0f * FMath::Log2(Frequency / A4Frequency);
	return FMath::Clamp(FMath::RoundToInt(MidiNote), 0, 127);
}

// ============================================================================
// Value Normalization
// ============================================================================

float ULibremidiUtilities::Normalize7Bit(int32 Value)
{
	return FMath::Clamp(Value, 0, 127) * MidiConstants::Inv127;
}

int32 ULibremidiUtilities::Denormalize7Bit(float NormalizedValue)
{
	return FMath::Clamp(FMath::RoundToInt(NormalizedValue * 127.0f), 0, 127);
}

float ULibremidiUtilities::Normalize14Bit(int32 Value)
{
	return FMath::Clamp(Value, 0, 16383) * MidiConstants::Inv16383;
}

int32 ULibremidiUtilities::Denormalize14Bit(float NormalizedValue)
{
	return FMath::Clamp(FMath::RoundToInt(NormalizedValue * 16383.0f), 0, 16383);
}

float ULibremidiUtilities::NormalizePitchBend(int32 Value)
{
	return FMath::Clamp(Value, -8192, 8191) * MidiConstants::Inv8192;
}

int32 ULibremidiUtilities::DenormalizePitchBend(float NormalizedValue)
{
	return FMath::Clamp(FMath::RoundToInt(NormalizedValue * 8192.0f), -8192, 8191);
}

// ============================================================================
// Common Operations
// ============================================================================

bool ULibremidiUtilities::IsValid7Bit(int32 Value)
{
	return static_cast<uint32>(Value) <= 127;  // Single comparison for 0-127 range
}

bool ULibremidiUtilities::IsValidChannel(int32 Channel)
{
	return static_cast<uint32>(Channel) <= 15;  // Single comparison for 0-15 range
}

// ============================================================================
// Port Management Utilities
// ============================================================================

int32 ULibremidiUtilities::CloseAllInputPorts(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("CloseAllInputPorts: Invalid WorldContextObject"));
		return 0;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("CloseAllInputPorts: Failed to get LibremidiEngineSubsystem"));
		return 0;
	}

	int32 ClosedCount = 0;
	for (ULibremidiInput* Port : Subsystem->GetActiveInputPorts())
	{
		if (Port && Port->IsPortOpen() && Port->ClosePort())
		{
			++ClosedCount;
		}
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("Closed %d input port(s)"), ClosedCount);
	return ClosedCount;
}

int32 ULibremidiUtilities::CloseAllOutputPorts(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("CloseAllOutputPorts: Invalid WorldContextObject"));
		return 0;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("CloseAllOutputPorts: Failed to get LibremidiEngineSubsystem"));
		return 0;
	}

	int32 ClosedCount = 0;
	for (ULibremidiOutput* Port : Subsystem->GetActiveOutputPorts())
	{
		if (Port && Port->IsPortOpen() && Port->ClosePort())
		{
			++ClosedCount;
		}
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("Closed %d output port(s)"), ClosedCount);
	return ClosedCount;
}

int32 ULibremidiUtilities::CloseAllMidiPorts(UObject* WorldContextObject)
{
	const int32 InputsClosed = CloseAllInputPorts(WorldContextObject);
	const int32 OutputsClosed = CloseAllOutputPorts(WorldContextObject);
	const int32 TotalClosed = InputsClosed + OutputsClosed;

	UE_LOG(LogLibremidi4UE, Log, TEXT("Closed total %d MIDI port(s) (%d inputs, %d outputs)"), 
		TotalClosed, InputsClosed, OutputsClosed);

	return TotalClosed;
}

TArray<ULibremidiInput*> ULibremidiUtilities::GetActiveInputPorts(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return {};
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	return Subsystem ? Subsystem->GetActiveInputPorts() : TArray<ULibremidiInput*>();
}

TArray<ULibremidiOutput*> ULibremidiUtilities::GetActiveOutputPorts(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return {};
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	return Subsystem ? Subsystem->GetActiveOutputPorts() : TArray<ULibremidiOutput*>();
}

ULibremidiInput* ULibremidiUtilities::FindInputPortByName(UObject* WorldContextObject, const FString& DisplayName)
{
	for (ULibremidiInput* Port : GetActiveInputPorts(WorldContextObject))
	{
		if (Port && Port->IsPortOpen() && 
			Port->GetCurrentPortInfo().DisplayName.Equals(DisplayName, ESearchCase::IgnoreCase))
		{
			return Port;
		}
	}
	return nullptr;
}

ULibremidiOutput* ULibremidiUtilities::FindOutputPortByName(UObject* WorldContextObject, const FString& DisplayName)
{
	for (ULibremidiOutput* Port : GetActiveOutputPorts(WorldContextObject))
	{
		if (Port && Port->IsPortOpen() && 
			Port->GetCurrentPortInfo().DisplayName.Equals(DisplayName, ESearchCase::IgnoreCase))
		{
			return Port;
		}
	}
	return nullptr;
}

int32 ULibremidiUtilities::GetActiveInputPortCount(UObject* WorldContextObject)
{
	return GetActiveInputPorts(WorldContextObject).Num();
}

int32 ULibremidiUtilities::GetActiveOutputPortCount(UObject* WorldContextObject)
{
	return GetActiveOutputPorts(WorldContextObject).Num();
}

ULibremidiInput* ULibremidiUtilities::OpenInputPortByName(UObject* WorldContextObject, const FString& DisplayName, const FString& ClientName)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("OpenInputPortByName: Invalid WorldContextObject"));
		return nullptr;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("OpenInputPortByName: Failed to get LibremidiEngineSubsystem"));
		return nullptr;
	}

	// First, check if this port is already open
	// If so, return the existing instance instead of creating a new one
	ULibremidiInput* ExistingInput = FindInputPortByName(WorldContextObject, DisplayName);
	if (ExistingInput)
	{
		UE_LOG(LogLibremidi4UE, Verbose, TEXT("OpenInputPortByName: Port '%s' is already open, returning existing instance"), 
			*DisplayName);
		return ExistingInput;
	}

	// Find available port with matching display name
	const FLibremidiPortInfo* TargetPort = nullptr;
	for (const FLibremidiPortInfo& Port : Subsystem->GetAvailableInputPorts())
	{
		if (Port.DisplayName.Equals(DisplayName, ESearchCase::IgnoreCase))
		{
			TargetPort = &Port;
			break;
		}
	}

	if (!TargetPort)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("OpenInputPortByName: Port '%s' not found in available ports"), 
			*DisplayName);
		return nullptr;
	}

	// Create and open input port
	ULibremidiInput* Input = ULibremidiInput::CreateMidiInput(WorldContextObject);
	if (!Input)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("OpenInputPortByName: Failed to create input instance"));
		return nullptr;
	}

	if (!Input->OpenPort(*TargetPort, ClientName))
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("OpenInputPortByName: Failed to open port '%s'"), *DisplayName);
		return nullptr;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("OpenInputPortByName: Opened input port '%s'"), *DisplayName);
	return Input;
}

ULibremidiOutput* ULibremidiUtilities::OpenOutputPortByName(UObject* WorldContextObject, const FString& DisplayName, const FString& ClientName)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("OpenOutputPortByName: Invalid WorldContextObject"));
		return nullptr;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("OpenOutputPortByName: Failed to get LibremidiEngineSubsystem"));
		return nullptr;
	}

	// First, check if this port is already open
	// If so, return the existing instance instead of creating a new one
	ULibremidiOutput* ExistingOutput = FindOutputPortByName(WorldContextObject, DisplayName);
	if (ExistingOutput)
	{
		UE_LOG(LogLibremidi4UE, Verbose, TEXT("OpenOutputPortByName: Port '%s' is already open, returning existing instance"), 
			*DisplayName);
		return ExistingOutput;
	}

	// Find available port with matching display name
	const FLibremidiPortInfo* TargetPort = nullptr;
	for (const FLibremidiPortInfo& Port : Subsystem->GetAvailableOutputPorts())
	{
		if (Port.DisplayName.Equals(DisplayName, ESearchCase::IgnoreCase))
		{
			TargetPort = &Port;
			break;
		}
	}

	if (!TargetPort)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("OpenOutputPortByName: Port '%s' not found in available ports"), 
			*DisplayName);
		return nullptr;
	}

	// Create and open output port
	ULibremidiOutput* Output = ULibremidiOutput::CreateMidiOutput(WorldContextObject);
	if (!Output)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("OpenOutputPortByName: Failed to create output instance"));
		return nullptr;
	}

	if (!Output->OpenPort(*TargetPort, ClientName))
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("OpenOutputPortByName: Failed to open port '%s'"), *DisplayName);
		return nullptr;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("OpenOutputPortByName: Opened output port '%s'"), *DisplayName);
	return Output;
}
