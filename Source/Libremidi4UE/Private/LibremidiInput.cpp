/**
 * @file LibremidiInput.cpp
 * @brief MIDI Input implementation with unified UMP processing
 * 
 * ## Unified High-Resolution Processing
 * This implementation ALWAYS processes MIDI data as UMP (MIDI 2.0) format internally.
 * libremidi automatically handles format conversion:
 * 
 * - **MIDI 1.0 devices**: Data is automatically upconverted (7-bit ? 16-bit/32-bit)
 * - **MIDI 2.0 devices**: Native high-resolution data is preserved
 * 
 * ## Data Normalization
 * All continuous MIDI values are normalized to float for consistent Blueprint/C++ handling:
 * - Velocity: 16-bit (0-65535) ? float 0.0-1.0
 * - Control Change: 32-bit (0-4.2B) ? float 0.0-1.0
 * - Pressure: 32-bit (0-4.2B) ? float 0.0-1.0
 * - Pitch Bend: 32-bit signed ? float -1.0 to 1.0
 * 
 * ## Thread Safety
 * MIDI messages arrive on a background thread and are marshaled to the game thread
 * using AsyncTask(ENamedThreads::GameThread, ...) for safe delegate broadcasting.
 * 
 * ## SysEx Handling
 * Multi-packet SysEx is automatically buffered and concatenated in ParseAndBroadcastUMPSysEx().
 * F0/F7 framing bytes are automatically stripped.
 * 
 * ## Performance Optimizations
 * - constexpr normalization constants (multiplication instead of division)
 * - FORCEINLINE helper functions for common operations
 * - TArray::Reserve() to minimize reallocations
 * - MoveTemp() for zero-copy semantics
 */

#include "LibremidiInput.h"
#include "LibremidiEngineSubsystem.h"
#include "LibremidiCommon.h"
#include "Libremidi4UELog.h"
#include "Engine/Engine.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

ULibremidiInput::ULibremidiInput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVirtualPort = false;
	bEnableAutoReconnect = true;
	bUMPSysExInProgress = false;
	UMPSysExStartTimestamp = 0;
}

void ULibremidiInput::BeginDestroy()
{
	UnregisterHotPlugDelegate();
	ClosePort();
	Super::BeginDestroy();
}

ULibremidiInput* ULibremidiInput::CreateMidiInput(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("CreateMidiInput: Invalid WorldContextObject"));
		return nullptr;
	}

	return NewObject<ULibremidiInput>(WorldContextObject);
}

ULibremidiEngineSubsystem* ULibremidiInput::GetSubsystem() const
{
	return UE::MIDI::Private::GetEngineSubsystem();
}

libremidi::input_port ULibremidiInput::ConvertPortInfo(const FLibremidiPortInfo& PortInfo) const
{
	return UE::MIDI::Private::ToLibremidiInputPort(PortInfo);
}

bool ULibremidiInput::CreateMidiIn(libremidi::API API)
{
	try
	{
		MidiIn = MakeUnique<libremidi::midi_in>(CreateInputConfigurationUMP(), API);
		
		if (MidiIn)
		{
			UE::MIDI::Private::LogBackendType(MidiIn->get_current_api(), true);
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		HandleError(FString::Printf(TEXT("Failed to create MIDI input: %s"), UTF8_TO_TCHAR(e.what())));
		return false;
	}
}

bool ULibremidiInput::OpenPort(const FLibremidiPortInfo& PortInfo, const FString& ClientName)
{
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiIn.Get(), TEXT("Input")))
	{
		return false;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	if (ULibremidiInput* ExistingPort = Subsystem->FindActiveInputPort(PortInfo))
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("MidiInput: Port '%s' is already open by another instance"), *PortInfo.DisplayName);
		HandleError(FString::Printf(TEXT("Port '%s' is already open"), *PortInfo.DisplayName));
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	if (!CreateMidiIn(LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiIn->open_port(ConvertPortInfo(PortInfo), TCHAR_TO_UTF8(*ClientName)); 
		Error != stdx::error{})
	{
		HandleError(FString::Printf(TEXT("Failed to open port '%s'"), *PortInfo.DisplayName));
		MidiIn.Reset();
		return false;
	}

	// Store last client name for auto-reconnection
	LastClientName = ClientName;

	NotifyPortOpened(PortInfo, false);

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Opened port '%s' with API: %s (UMP processing)"), 
		*PortInfo.DisplayName, 
		*UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiInput::OpenVirtualPort(const FString& PortName)
{
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiIn.Get(), TEXT("Input")))
	{
		return false;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	if (!CreateMidiIn(LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiIn->open_virtual_port(TCHAR_TO_UTF8(*PortName)); 
		Error != stdx::error{})
	{
		HandleError(FString::Printf(TEXT("Failed to open virtual port '%s'"), *PortName));
		MidiIn.Reset();
		return false;
	}

	FLibremidiPortInfo VirtualPortInfo;
	VirtualPortInfo.DisplayName = PortName;
	VirtualPortInfo.PortName = PortName;
	
	LastClientName = PortName;
	
	NotifyPortOpened(VirtualPortInfo, true);

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Opened virtual port '%s' with API: %s (UMP processing)"),
		*PortName, 
		*UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiInput::ClosePort()
{
	if (!MidiIn || !MidiIn->is_port_open())
	{
		return false;
	}

	// Store port info before closing (for potential reconnection)
	if (!IsPortConnected() && bEnableAutoReconnect)
	{
		LastDisconnectedPort = CurrentPortInfo;
	}

	if (const auto Error = MidiIn->close_port(); Error != stdx::error{})
	{
		HandleError(TEXT("Failed to close port"));
		return false;
	}

	NotifyPortClosed();

	UMPSysExBuffer.Reset();
	bUMPSysExInProgress = false;
	UMPSysExStartTimestamp = 0;

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Port closed"));
	MidiIn.Reset();
	return true;
}

bool ULibremidiInput::IsPortOpen() const
{
	return MidiIn && MidiIn->is_port_open();
}

bool ULibremidiInput::IsPortConnected() const
{
	return MidiIn && MidiIn->is_port_connected();
}

ELibremidiAPI ULibremidiInput::GetCurrentAPI() const
{
	return MidiIn ? LibremidiTypeConversion::FromLibremidiAPI(MidiIn->get_current_api()) 
	              : ELibremidiAPI::Unspecified;
}

libremidi::ump_input_configuration ULibremidiInput::CreateInputConfigurationUMP()
{
	libremidi::ump_input_configuration Config;
	
	Config.on_message = [this](libremidi::ump&& msg)
	{
		HandleMidiMessageUMP(std::move(msg));
	};
	
	Config.on_error = [this](std::string_view errorText, const libremidi::source_location& loc)
	{
		HandleError(FString::Printf(TEXT("%s at %s:%d"), 
			UTF8_TO_TCHAR(errorText.data()), 
			UTF8_TO_TCHAR(loc.file_name()), 
			loc.line()));
	};

	return Config;
}

void ULibremidiInput::HandleMidiMessageUMP(libremidi::ump&& Message)
{
	TArray<uint32> Data;
	Data.Reserve(4);
	
	const uint32* DataPtr = Message.data;
	const size_t Size = Message.size();
	
	for (size_t i = 0; i < Size && i < 4; ++i)
	{
		Data.Add(DataPtr[i]);
	}
	
	const int64 Timestamp = Message.timestamp;
	
	AsyncTask(ENamedThreads::GameThread, [this, Data = MoveTemp(Data), Timestamp]()
	{
		if (IsValid(this))
		{
			// Parse and broadcast individual UMP message types
			if (Data.Num() > 0)
			{
				ParseAndBroadcastUMP(Data, Timestamp);
			}
			
			// Also broadcast the raw UMP message
			OnMidiMessageUMP.Broadcast(Data, Timestamp);
		}
	});
}

void ULibremidiInput::HandleError(const FString& ErrorMessage)
{
	UE::MIDI::Private::DispatchErrorToGameThread(this, ErrorMessage, OnError);
}

void ULibremidiInput::NotifyPortOpened(const FLibremidiPortInfo& PortInfo, bool bVirtual)
{
	CurrentPortInfo = PortInfo;
	bIsVirtualPort = bVirtual;

	if (ULibremidiEngineSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->RegisterInputPort(this);
	}

	// Fire OnPortOpened delegate
	OnPortOpened.Broadcast(PortInfo);
}

void ULibremidiInput::NotifyPortClosed()
{
	if (ULibremidiEngineSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->UnregisterInputPort(this);
	}

	CurrentPortInfo = FLibremidiPortInfo();
	bIsVirtualPort = false;
}

// ============================================================================
// Auto-Reconnection
// ============================================================================

void ULibremidiInput::SetAutoReconnect(bool bEnable)
{
	if (bEnableAutoReconnect == bEnable)
	{
		return;
	}

	bEnableAutoReconnect = bEnable;

	if (bEnable)
	{
		RegisterHotPlugDelegate();
		UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Auto-reconnection enabled for '%s'"), 
			*CurrentPortInfo.DisplayName);
	}
	else
	{
		UnregisterHotPlugDelegate();
		UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Auto-reconnection disabled"));
	}
}

void ULibremidiInput::RegisterHotPlugDelegate()
{
	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}

	// Unregister first if already registered
	UnregisterHotPlugDelegate();

	// Register delegate for hot-plug events
	HotPlugDelegateHandle = Subsystem->OnInputPortConnected.AddUObject(
		this, 
		&ULibremidiInput::HandleHotPlugEvent
	);

	UE_LOG(LogLibremidi4UE, Verbose, TEXT("MidiInput: Registered hot-plug delegate"));
}

void ULibremidiInput::UnregisterHotPlugDelegate()
{
	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (Subsystem && HotPlugDelegateHandle.IsValid())
	{
		Subsystem->OnInputPortConnected.Remove(HotPlugDelegateHandle);
		HotPlugDelegateHandle.Reset();
		UE_LOG(LogLibremidi4UE, Verbose, TEXT("MidiInput: Unregistered hot-plug delegate"));
	}
}

void ULibremidiInput::HandleHotPlugEvent(const FLibremidiPortInfo& ReconnectedPort)
{
	// Check if auto-reconnection is enabled
	if (!bEnableAutoReconnect)
	{
		return;
	}

	// Check if currently disconnected
	if (IsPortOpen() && IsPortConnected())
	{
		return;
	}

	// Check if this is the port we were connected to
	const bool bSamePort = 
		(ReconnectedPort.DisplayName == LastDisconnectedPort.DisplayName) ||
		(ReconnectedPort.ClientHandle == LastDisconnectedPort.ClientHandle && 
		 ReconnectedPort.PortHandle == LastDisconnectedPort.PortHandle);

	if (!bSamePort)
	{
		return;
	}

	// Attempt to reconnect
	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Attempting auto-reconnection to '%s'"), 
		*ReconnectedPort.DisplayName);

	// Close existing port if still open
	if (IsPortOpen())
	{
		ClosePort();
	}

	// Try to reopen the port
	if (OpenPort(ReconnectedPort, LastClientName))
	{
		UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Auto-reconnection successful"));
	}
	else
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("MidiInput: Auto-reconnection failed"));
	}
}

// ============================================================================
// MIDI Message Parsing and Broadcasting
// ============================================================================

/**
 * Normalization constants for MIDI data ? float conversion.
 * Using multiplication (1/N) instead of division for better performance.
 */
namespace MidiNormalization
{
	constexpr float Velocity7Bit = 1.0f / 127.0f;
	constexpr float PitchBend14Bit = 1.0f / 8192.0f;
	constexpr float Velocity16Bit = 1.0f / 65535.0f;
	constexpr float Value32Bit = 1.0f / 4294967295.0f;
	constexpr float PitchBend32Bit = 1.0f / 2147483648.0f;
}

/**
 * Normalized MIDI message data for unified broadcasting.
 */
struct FNormalizedMidiData
{
	uint8 Status;
	int32 Channel;
	int32 Data1;
	int32 Data2;
	float Velocity;
	float Value;
	float PitchBend;
	int64 Timestamp;
};

/**
 * Common broadcasting helper functions.
 */
namespace
{
	using namespace FLibremidiConstants;
	
	FORCEINLINE void BroadcastNoteOn(ULibremidiInput* Input, int32 Channel, int32 Note, float Velocity, int64 Timestamp)
	{
		if (Velocity > 0.0f)
		{
			Input->OnNoteOn.Broadcast(Input, Channel, Note, Velocity, Timestamp);
		}
		else
		{
			Input->OnNoteOff.Broadcast(Input, Channel, Note, 0.0f, Timestamp);
		}
	}
	
	FORCEINLINE void BroadcastSystemRealTime(ULibremidiInput* Input, uint8 Status, int64 Timestamp)
	{
		switch (Status)
		{
			case Clock:
				Input->OnMidiClock.Broadcast(Input, Timestamp);
				break;
			case Start:
				Input->OnMidiStart.Broadcast(Input, Timestamp);
				break;
			case Continue:
				Input->OnMidiContinue.Broadcast(Input, Timestamp);
				break;
			case Stop:
				Input->OnMidiStop.Broadcast(Input, Timestamp);
				break;
		}
	}
	
	FORCEINLINE void BroadcastChannelVoice(ULibremidiInput* Input, const FNormalizedMidiData& Data)
	{
		switch (Data.Status)
		{
			case NoteOff:
				Input->OnNoteOff.Broadcast(Input, Data.Channel, Data.Data1, Data.Velocity, Data.Timestamp);
				break;
				
			case NoteOn:
				BroadcastNoteOn(Input, Data.Channel, Data.Data1, Data.Velocity, Data.Timestamp);
				break;
				
			case PolyPressure:
				Input->OnPolyPressure.Broadcast(Input, Data.Channel, Data.Data1, Data.Value, Data.Timestamp);
				break;
				
			case ControlChange:
				Input->OnControlChange.Broadcast(Input, Data.Channel, Data.Data1, Data.Value, Data.Timestamp);
				break;
				
			case ProgramChange:
				Input->OnProgramChange.Broadcast(Input, Data.Channel, Data.Data1, Data.Timestamp);
				break;
				
			case ChannelPressure:
				Input->OnChannelPressure.Broadcast(Input, Data.Channel, Data.Value, Data.Timestamp);
				break;
				
			case PitchBend:
				Input->OnPitchBend.Broadcast(Input, Data.Channel, Data.PitchBend, Data.Timestamp);
				break;
		}
	}
	
	/**
	 * Extract SysEx data from a UMP packet word.
	 * @param Word UMP 32-bit word
	 * @param ByteIndex Byte index within word (0-3)
	 * @return Extracted byte, or 0 if invalid
	 */
	FORCEINLINE uint8 ExtractUMPByte(uint32 Word, int32 ByteIndex)
	{
		return (Word >> (24 - ByteIndex * 8)) & 0xFF;
	}
	
	/**
	 * Check if byte should be excluded from SysEx data.
	 */
	FORCEINLINE bool ShouldExcludeSysExByte(uint8 Byte)
	{
		return Byte == 0x00 || Byte == SysExStart || Byte == SysExEnd;
	}
}

/**
 * MIDI message parsing and broadcasting.
 */

void ULibremidiInput::ParseAndBroadcastUMP(const TArray<uint32>& Data, int64 Timestamp)
{
	if (Data.Num() == 0)
	{
		return;
	}
	
	// Get message type from first word
	const uint32 Word0 = Data[0];
	const uint8 MessageType = (Word0 >> 28) & 0x0F;
	
	// MIDI 2.0 Message Types:
	// 0x0 = Utility Messages
	// 0x1 = System Messages
	// 0x2 = MIDI 1.0 Channel Voice Messages
	// 0x3 = Data Messages (including SysEx)
	// 0x4 = MIDI 2.0 Channel Voice Messages
	// 0x5 = Data Messages
	
	switch (MessageType)
	{
		case 0x1: // System Messages
			ParseAndBroadcastUMPSystem(Data, Timestamp);
			break;
			
		case 0x2: // MIDI 1.0 Channel Voice (in UMP format)
			ParseAndBroadcastUMPMidi1(Data, Timestamp);
			break;
			
		case 0x4: // MIDI 2.0 Channel Voice
			ParseAndBroadcastUMPMidi2(Data, Timestamp);
			break;
			
		case 0x3: // SysEx 7-bit
		case 0x5: // SysEx 8-bit / Mixed Data Set
			ParseAndBroadcastUMPSysEx(Data, Timestamp);
			break;
			
		case 0x0: // Utility (JR Clock, JR Timestamp, etc.)
			// Utility messages are typically handled internally
			break;
			
		default:
			break;
	}
}

void ULibremidiInput::ParseAndBroadcastUMPSystem(const TArray<uint32>& Data, int64 Timestamp)
{
	if (Data.Num() == 0) return;
	
	const uint32 Word0 = Data[0];
	const uint8 Status = (Word0 >> 16) & 0xFF;
	
	if (Status == FLibremidiConstants::SongPosition)
	{
		const int32 Beats = ((Word0 >> 8) & 0x7F) | ((Word0 & 0x7F) << 7);
		OnSongPosition.Broadcast(this, Beats, Timestamp);
	}
	else if (Status == FLibremidiConstants::SongSelect)
	{
		OnSongSelect.Broadcast(this, (Word0 >> 8) & 0x7F, Timestamp);
	}
	else
	{
		BroadcastSystemRealTime(this, Status, Timestamp);
	}
}

void ULibremidiInput::ParseAndBroadcastUMPMidi1(const TArray<uint32>& Data, int64 Timestamp)
{
	if (Data.Num() == 0) return;
	
	const uint32 Word0 = Data[0];
	
	FNormalizedMidiData MidiData;
	MidiData.Status = (Word0 >> 16) & 0xF0;
	MidiData.Channel = (Word0 >> 16) & 0x0F;
	MidiData.Data1 = (Word0 >> 8) & 0xFF;
	MidiData.Data2 = Word0 & 0xFF;
	MidiData.Timestamp = Timestamp;
	
	MidiData.Velocity = MidiData.Data2 * MidiNormalization::Velocity7Bit;
	MidiData.Value = MidiData.Data2 * MidiNormalization::Velocity7Bit;
	
	const int32 PitchBend14 = (MidiData.Data1 | (MidiData.Data2 << 7)) - 8192;
	MidiData.PitchBend = PitchBend14 * MidiNormalization::PitchBend14Bit;
	
	BroadcastChannelVoice(this, MidiData);
}

void ULibremidiInput::ParseAndBroadcastUMPMidi2(const TArray<uint32>& Data, int64 Timestamp)
{
	if (Data.Num() < 2) return;
	
	const uint32 Word0 = Data[0];
	const uint32 Word1 = Data[1];
	
	FNormalizedMidiData MidiData;
	MidiData.Status = (Word0 >> 16) & 0xF0;
	MidiData.Channel = (Word0 >> 16) & 0x0F;
	MidiData.Data1 = (Word0 >> 8) & 0xFF;
	MidiData.Data2 = (Word1 >> 24) & 0xFF;
	MidiData.Timestamp = Timestamp;
	
	MidiData.Velocity = ((Word1 >> 16) & 0xFFFF) * MidiNormalization::Velocity16Bit;
	MidiData.Value = Word1 * MidiNormalization::Value32Bit;
	MidiData.PitchBend = static_cast<int32>(Word1) * MidiNormalization::PitchBend32Bit;
	
	BroadcastChannelVoice(this, MidiData);
}

void ULibremidiInput::ParseAndBroadcastUMPSysEx(const TArray<uint32>& Data, int64 Timestamp)
{
	if (Data.Num() == 0) return;
	
	const uint32 Word0 = Data[0];
	const uint8 Status = (Word0 >> 20) & 0x0F;
	const int32 NumBytes = (Word0 >> 16) & 0x0F;
	
	TArray<uint8> PacketData;
	PacketData.Reserve(NumBytes + (Data.Num() - 1) * 4);
	
	for (int32 j = 0; j < FMath::Min<int32>(NumBytes, 2); ++j)
	{
		const uint8 Byte = ExtractUMPByte(Word0, j);
		if (!ShouldExcludeSysExByte(Byte))
		{
			PacketData.Add(Byte);
		}
	}
	
	for (int32 i = 1; i < Data.Num(); ++i)
	{
		const uint32 Word = Data[i];
		for (int32 j = 0; j < 4; ++j)
		{
			const uint8 Byte = ExtractUMPByte(Word, j);
			if (!ShouldExcludeSysExByte(Byte))
			{
				PacketData.Add(Byte);
			}
		}
	}
	
	switch (Status)
	{
		case 0:
			if (PacketData.Num() > 0)
			{
				OnSysEx.Broadcast(this, PacketData, Timestamp);
			}
			break;
		
		case 1:
			UMPSysExBuffer.Reset();
			UMPSysExBuffer = MoveTemp(PacketData);
			bUMPSysExInProgress = true;
			UMPSysExStartTimestamp = Timestamp;
			UE_LOG(LogLibremidi4UE, Verbose, TEXT("UMP SysEx: Start (%d bytes)"), UMPSysExBuffer.Num());
			break;
		
		case 2:
			if (bUMPSysExInProgress)
			{
				UMPSysExBuffer.Append(MoveTemp(PacketData));
				UE_LOG(LogLibremidi4UE, Verbose, TEXT("UMP SysEx: Continue (total %d)"), UMPSysExBuffer.Num());
			}
			else
			{
				UE_LOG(LogLibremidi4UE, Warning, TEXT("UMP SysEx: Received Continue without Start"));
			}
			break;
		
		case 3:
			if (bUMPSysExInProgress)
			{
				UMPSysExBuffer.Append(MoveTemp(PacketData));
				UE_LOG(LogLibremidi4UE, Verbose, TEXT("UMP SysEx: End (total %d)"), UMPSysExBuffer.Num());
				
				OnSysEx.Broadcast(this, UMPSysExBuffer, UMPSysExStartTimestamp);
				
				UMPSysExBuffer.Reset();
				bUMPSysExInProgress = false;
				UMPSysExStartTimestamp = 0;
			}
			else
			{
				UE_LOG(LogLibremidi4UE, Warning, TEXT("UMP SysEx: Received End without Start"));
			}
			break;
		
		default:
			UE_LOG(LogLibremidi4UE, Warning, TEXT("UMP SysEx: Unknown status %d"), Status);
			break;
	}
}

