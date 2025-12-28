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

// ============================================================================
// Private Helper Functions
// ============================================================================

namespace
{
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
	using namespace FLibremidiConstants;
	
	FORCEINLINE void BroadcastNoteOn(ULibremidiInput* Input, int32 Channel, int32 Note, float Velocity, int64 Timestamp)
	{
		UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] Ch:%d NoteOn Note:%d Vel:%.3f Time:%lld"), 
			Channel, Note, Velocity, Timestamp);
		
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
		const TCHAR* StatusName = TEXT("Unknown");
		switch (Status)
		{
			case Clock:
				StatusName = TEXT("Clock");
				Input->OnMidiClock.Broadcast(Input, Timestamp);
				break;
			case Start:
				StatusName = TEXT("Start");
				Input->OnMidiStart.Broadcast(Input, Timestamp);
				break;
			case Continue:
				StatusName = TEXT("Continue");
				Input->OnMidiContinue.Broadcast(Input, Timestamp);
				break;
			case Stop:
				StatusName = TEXT("Stop");
				Input->OnMidiStop.Broadcast(Input, Timestamp);
				break;
		}
		
		UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] System: %s Time:%lld"), StatusName, Timestamp);
	}
	
	FORCEINLINE void BroadcastChannelVoice(ULibremidiInput* Input, const FNormalizedMidiData& Data)
	{
		switch (Data.Status)
		{
			case NoteOff:
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] Ch:%d NoteOff Note:%d Vel:%.3f Time:%lld"), 
					Data.Channel, Data.Data1, Data.Velocity, Data.Timestamp);
				Input->OnNoteOff.Broadcast(Input, Data.Channel, Data.Data1, Data.Velocity, Data.Timestamp);
				break;
				
			case NoteOn:
				BroadcastNoteOn(Input, Data.Channel, Data.Data1, Data.Velocity, Data.Timestamp);
				break;
				
			case PolyPressure:
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] Ch:%d PolyPressure Note:%d Press:%.3f Time:%lld"), 
					Data.Channel, Data.Data1, Data.Value, Data.Timestamp);
				Input->OnPolyPressure.Broadcast(Input, Data.Channel, Data.Data1, Data.Value, Data.Timestamp);
				break;
				
			case ControlChange:
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] Ch:%d CC:%d Val:%.3f Time:%lld"), 
					Data.Channel, Data.Data1, Data.Value, Data.Timestamp);
				Input->OnControlChange.Broadcast(Input, Data.Channel, Data.Data1, Data.Value, Data.Timestamp);
				break;
				
			case ProgramChange:
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] Ch:%d ProgramChange Prog:%d Time:%lld"), 
					Data.Channel, Data.Data1, Data.Timestamp);
				Input->OnProgramChange.Broadcast(Input, Data.Channel, Data.Data1, Data.Timestamp);
				break;
				
			case ChannelPressure:
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] Ch:%d ChannelPressure Press:%.3f Time:%lld"), 
					Data.Channel, Data.Value, Data.Timestamp);
				Input->OnChannelPressure.Broadcast(Input, Data.Channel, Data.Value, Data.Timestamp);
				break;
				
			case PitchBend:
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] Ch:%d PitchBend Val:%.3f Time:%lld"), 
					Data.Channel, Data.PitchBend, Data.Timestamp);
				Input->OnPitchBend.Broadcast(Input, Data.Channel, Data.PitchBend, Data.Timestamp);
				break;
		}
	}
	
	/**
	 * Extract SysEx data from a UMP packet word.
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

// ============================================================================
// Lifecycle
// ============================================================================

ULibremidiInput::ULibremidiInput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsVirtualPort(false)
	, bEnableAutoReconnect(true)
	, bUMPSysExInProgress(false)
	, UMPSysExStartTimestamp(0)
{
}

void ULibremidiInput::BeginDestroy()
{
	UnregisterHotPlugDelegate();
	ClosePort();
	Super::BeginDestroy();
}

// ============================================================================
// Factory & Subsystem Access
// ============================================================================

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

// ============================================================================
// Port Management
// ============================================================================

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
	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	// Check if this instance already has a port open
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiIn.Get(), TEXT("Input")))
	{
		// Check if it's the SAME port we're trying to open
		if (CurrentPortInfo == PortInfo)
		{
			UE_LOG(LogLibremidi4UE, Verbose, TEXT("MidiInput: Port '%s' is already open by this instance"), 
				*PortInfo.DisplayName);
			return true;
		}
		
		// Different port - error
		const FString ErrorMsg = FString::Printf(
			TEXT("This instance already has port '%s' open. Close it before opening '%s'"),
			*CurrentPortInfo.DisplayName,
			*PortInfo.DisplayName
		);
		UE_LOG(LogLibremidi4UE, Warning, TEXT("MidiInput: %s"), *ErrorMsg);
		HandleError(ErrorMsg);
		return false;
	}

	// Check if this port is already open by another instance
	if (Subsystem->FindActiveInputPort(PortInfo))
	{
		const FString ErrorMsg = FString::Printf(
			TEXT("Port '%s' is already open by another instance"),
			*PortInfo.DisplayName
		);
		UE_LOG(LogLibremidi4UE, Warning, TEXT("MidiInput: %s"), *ErrorMsg);
		HandleError(ErrorMsg);
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

	LastClientName = ClientName;
	NotifyPortOpened(PortInfo, false);

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Opened port '%s' with API: %s (UMP processing)"), 
		*PortInfo.DisplayName, *UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiInput::OpenVirtualPort(const FString& PortName)
{
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiIn.Get(), TEXT("Input")))
	{
		HandleError(TEXT("Port is already open"));
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
		*PortName, *UEnum::GetValueAsString(API));
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

// ============================================================================
// Port Status
// ============================================================================

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

// ============================================================================
// Configuration & Message Handling
// ============================================================================

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
	const size_t Size = FMath::Min<size_t>(Message.size(), 4);
	
	for (size_t i = 0; i < Size; ++i)
	{
		Data.Add(DataPtr[i]);
	}
	
	const int64 Timestamp = Message.timestamp;
	
	AsyncTask(ENamedThreads::GameThread, [this, Data = MoveTemp(Data), Timestamp]()
	{
		if (IsValid(this))
		{
			if (Data.Num() > 0)
			{
				ParseAndBroadcastUMP(Data, Timestamp);
			}
			
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

	UnregisterHotPlugDelegate();

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
	if (!bEnableAutoReconnect || (IsPortOpen() && IsPortConnected()))
	{
		return;
	}

	const bool bSamePort = 
		(ReconnectedPort.DisplayName == LastDisconnectedPort.DisplayName) ||
		(ReconnectedPort.ClientHandle == LastDisconnectedPort.ClientHandle && 
		 ReconnectedPort.PortHandle == LastDisconnectedPort.PortHandle);

	if (!bSamePort)
	{
		return;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Attempting auto-reconnection to '%s'"), 
		*ReconnectedPort.DisplayName);

	if (IsPortOpen())
	{
		ClosePort();
	}

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

void ULibremidiInput::ParseAndBroadcastUMP(const TArray<uint32>& Data, int64 Timestamp)
{
	if (Data.Num() == 0)
	{
		return;
	}
	
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
		UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] SongPosition Beats:%d Time:%lld"), Beats, Timestamp);
		OnSongPosition.Broadcast(this, Beats, Timestamp);
	}
	else if (Status == FLibremidiConstants::SongSelect)
	{
		const int32 Song = (Word0 >> 8) & 0x7F;
		UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] SongSelect Song:%d Time:%lld"), Song, Timestamp);
		OnSongSelect.Broadcast(this, Song, Timestamp);
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
	
	// Extract bytes from first word
	for (int32 j = 0; j < FMath::Min<int32>(NumBytes, 2); ++j)
	{
		const uint8 Byte = ExtractUMPByte(Word0, j);
		if (!ShouldExcludeSysExByte(Byte))
		{
			PacketData.Add(Byte);
		}
	}
	
	// Extract bytes from remaining words
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
		case 0: // Single packet
			if (PacketData.Num() > 0)
			{
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] SysEx (single packet) Size:%d Time:%lld"), 
					PacketData.Num(), Timestamp);
				OnSysEx.Broadcast(this, PacketData, Timestamp);
			}
			break;
		
		case 1: // Start
			UMPSysExBuffer = MoveTemp(PacketData);
			bUMPSysExInProgress = true;
			UMPSysExStartTimestamp = Timestamp;
			UE_LOG(LogLibremidi4UE, Verbose, TEXT("UMP SysEx: Start (%d bytes)"), UMPSysExBuffer.Num());
			break;
		
		case 2: // Continue
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
		
		case 3: // End
			if (bUMPSysExInProgress)
			{
				UMPSysExBuffer.Append(MoveTemp(PacketData));
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] SysEx (multi-packet complete) Size:%d Time:%lld"), 
					UMPSysExBuffer.Num(), UMPSysExStartTimestamp);
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

