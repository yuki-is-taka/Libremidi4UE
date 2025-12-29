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
		if (Velocity > 0.0f)
		{
			UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI IN] NoteOn  Ch:%2d Note:%3d Vel:%.3f"), 
				Channel, Note, Velocity);
			Input->OnNoteOn.Broadcast(Input, Channel, Note, Velocity, Timestamp);
		}
		else
		{
			UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI IN] NoteOff Ch:%2d Note:%3d (Vel=0)"), 
				Channel, Note);
			Input->OnNoteOff.Broadcast(Input, Channel, Note, 0.0f, Timestamp);
		}
	}
	
	FORCEINLINE void BroadcastSystemRealTime(ULibremidiInput* Input, uint8 Status, int64 Timestamp)
	{
		switch (Status)
		{
			case Clock:
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] Clock"));
				Input->OnMidiClock.Broadcast(Input, Timestamp);
				break;
			case Start:
				UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI IN] Transport: Start"));
				Input->OnMidiStart.Broadcast(Input, Timestamp);
				break;
			case Continue:
				UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI IN] Transport: Continue"));
				Input->OnMidiContinue.Broadcast(Input, Timestamp);
				break;
			case Stop:
				UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI IN] Transport: Stop"));
				Input->OnMidiStop.Broadcast(Input, Timestamp);
				break;
			default:
				UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI IN] System RT: 0x%02X"), Status);
				break;
		}
	}
	
	FORCEINLINE void BroadcastChannelVoice(ULibremidiInput* Input, const FNormalizedMidiData& Data)
	{
		switch (Data.Status)
		{
			case NoteOff:
				UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI IN] NoteOff Ch:%2d Note:%3d Vel:%.3f"), 
					Data.Channel, Data.Data1, Data.Velocity);
				Input->OnNoteOff.Broadcast(Input, Data.Channel, Data.Data1, Data.Velocity, Data.Timestamp);
				break;
				
			case NoteOn:
				BroadcastNoteOn(Input, Data.Channel, Data.Data1, Data.Velocity, Data.Timestamp);
				break;
				
			case PolyPressure:
				UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI IN] PolyAT  Ch:%2d Note:%3d Press:%.3f"), 
					Data.Channel, Data.Data1, Data.Value);
				Input->OnPolyPressure.Broadcast(Input, Data.Channel, Data.Data1, Data.Value, Data.Timestamp);
				break;
				
			case ControlChange:
				UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI IN] CC      Ch:%2d CC:%3d Val:%.3f"), 
					Data.Channel, Data.Data1, Data.Value);
				Input->OnControlChange.Broadcast(Input, Data.Channel, Data.Data1, Data.Value, Data.Timestamp);
				break;
				
			case ProgramChange:
				UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI IN] Program Ch:%2d Prog:%d"), 
					Data.Channel, Data.Data1);
				Input->OnProgramChange.Broadcast(Input, Data.Channel, Data.Data1, Data.Timestamp);
				break;
				
			case ChannelPressure:
				UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI IN] ChanAT  Ch:%2d Press:%.3f"), 
					Data.Channel, Data.Value);
				Input->OnChannelPressure.Broadcast(Input, Data.Channel, Data.Value, Data.Timestamp);
				break;
				
			case PitchBend:
				UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI IN] Pitch   Ch:%2d Val:%.3f"), 
					Data.Channel, Data.PitchBend);
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
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] CreateMidiInput failed: Invalid WorldContextObject"));
		return nullptr;
	}

	ULibremidiInput* NewInput = NewObject<ULibremidiInput>(WorldContextObject);
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Created new MidiInput instance"));
	return NewInput;
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
		libremidi::ump_input_configuration Config = CreateInputConfigurationUMP();
		MidiIn = MakeUnique<libremidi::midi_in>(Config, API);
		
		if (MidiIn)
		{
			UE::MIDI::Private::LogBackendType(MidiIn->get_current_api(), true);
			return true;
		}
		else
		{
			UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] Failed to create midi_in instance"));
			return false;
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] Exception creating midi_in: %s"), UTF8_TO_TCHAR(e.what()));
		HandleError(FString::Printf(TEXT("Failed to create MIDI input: %s"), UTF8_TO_TCHAR(e.what())));
		return false;
	}
}

bool ULibremidiInput::OpenPort(const FLibremidiPortInfo& PortInfo, const FString& ClientName)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Opening port '%s'..."), *PortInfo.DisplayName);
	
	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] Failed to get LibremidiEngineSubsystem"));
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	// Check if this instance already has a port open
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiIn.Get(), TEXT("Input")))
	{
		if (CurrentPortInfo == PortInfo)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Port '%s' is already open"), *PortInfo.DisplayName);
			return true;
		}
		
		const FString ErrorMsg = FString::Printf(
			TEXT("Close port '%s' before opening '%s'"),
			*CurrentPortInfo.DisplayName,
			*PortInfo.DisplayName
		);
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] %s"), *ErrorMsg);
		HandleError(ErrorMsg);
		return false;
	}

	// Check if port is already open by another instance
	if (Subsystem->FindActiveInputPort(PortInfo))
	{
		const FString ErrorMsg = FString::Printf(TEXT("Port '%s' is already in use"), *PortInfo.DisplayName);
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] %s"), *ErrorMsg);
		HandleError(ErrorMsg);
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MidiInput] Using API: %s"), *UEnum::GetValueAsString(API));
	
	if (!CreateMidiIn(LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	libremidi::input_port LibremidiPort = ConvertPortInfo(PortInfo);

	try
	{
		if (const auto Error = MidiIn->open_port(LibremidiPort, TCHAR_TO_UTF8(*ClientName)); 
			Error != stdx::error{})
		{
			MidiIn.Reset();
			UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] Failed to open port '%s'"), *PortInfo.DisplayName);
			HandleError(FString::Printf(TEXT("Failed to open port '%s'"), *PortInfo.DisplayName));
			return false;
		}
	}
	catch (const std::exception& e)
	{
		MidiIn.Reset();
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] Exception: %s"), UTF8_TO_TCHAR(e.what()));
		HandleError(FString::Printf(TEXT("Exception opening port '%s': %s"), 
			*PortInfo.DisplayName, UTF8_TO_TCHAR(e.what())));
		return false;
	}

	LastClientName = ClientName;
	NotifyPortOpened(PortInfo, false);

	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Opened port '%s' (API: %s)"), 
		*PortInfo.DisplayName, *UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiInput::OpenVirtualPort(const FString& PortName)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Opening virtual port '%s'..."), *PortName);
	
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiIn.Get(), TEXT("Input")))
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] A port is already open"));
		HandleError(TEXT("Port is already open"));
		return false;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] Failed to get LibremidiEngineSubsystem"));
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
		MidiIn.Reset();
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] Failed to open virtual port '%s'"), *PortName);
		HandleError(FString::Printf(TEXT("Failed to open virtual port '%s'"), *PortName));
		return false;
	}

	FLibremidiPortInfo VirtualPortInfo;
	VirtualPortInfo.DisplayName = PortName;
	VirtualPortInfo.PortName = PortName;
	
	LastClientName = PortName;
	NotifyPortOpened(VirtualPortInfo, true);

	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Opened virtual port '%s' (API: %s)"), 
		*PortName, *UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiInput::ClosePort()
{
	if (!MidiIn || !MidiIn->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MidiInput] No port to close"));
		return false;
	}

	const FString PortName = CurrentPortInfo.DisplayName;
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Closing port '%s'..."), *PortName);

	// Store port info before closing (for potential reconnection)
	if (!IsPortConnected() && bEnableAutoReconnect)
	{
		LastDisconnectedPort = CurrentPortInfo;
	}

	if (const auto Error = MidiIn->close_port(); Error != stdx::error{})
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiInput] Failed to close port"));
		HandleError(TEXT("Failed to close port"));
		return false;
	}

	NotifyPortClosed();

	// Clear SysEx state
	UMPSysExBuffer.Reset();
	bUMPSysExInProgress = false;
	UMPSysExStartTimestamp = 0;

	MidiIn.Reset();
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Closed port '%s'"), *PortName);
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
		OnUMPMessageReceived(std::move(msg));
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

void ULibremidiInput::OnUMPMessageReceived(libremidi::ump&& Message)
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
				ProcessUMP(Data, Timestamp);
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
		UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Auto-reconnect enabled for '%s'"), 
			*CurrentPortInfo.DisplayName);
	}
	else
	{
		UnregisterHotPlugDelegate();
		UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Auto-reconnect disabled"));
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

	UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MidiInput] Registered hot-plug delegate"));
}

void ULibremidiInput::UnregisterHotPlugDelegate()
{
	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (Subsystem && HotPlugDelegateHandle.IsValid())
	{
		Subsystem->OnInputPortConnected.Remove(HotPlugDelegateHandle);
		HotPlugDelegateHandle.Reset();
		UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MidiInput] Unregistered hot-plug delegate"));
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

	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Auto-reconnecting to '%s'..."), 
		*ReconnectedPort.DisplayName);

	if (IsPortOpen())
	{
		ClosePort();
	}

	if (OpenPort(ReconnectedPort, LastClientName))
	{
		UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiInput] Auto-reconnect successful"));
	}
	else
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("[MidiInput] Auto-reconnect failed"));
	}
}

// ============================================================================
// MIDI Message Parsing and Broadcasting
// ============================================================================

void ULibremidiInput::ProcessUMP(const TArray<uint32>& Data, int64 Timestamp)
{
	if (Data.Num() == 0)
	{
		return;
	}
	
	const uint32 Word0 = Data[0];
	const uint8 MessageType = (Word0 >> 28) & 0x0F;
	
	switch (MessageType)
	{
		case 0x1: // System Messages
			ProcessUMPSystem(Data, Timestamp);
			break;
			
		case 0x2: // MIDI 1.0 Channel Voice (in UMP format)
			ProcessUMPMidi1(Data, Timestamp);
			break;
			
		case 0x4: // MIDI 2.0 Channel Voice
			ProcessUMPMidi2(Data, Timestamp);
			break;
			
		case 0x3: // SysEx 7-bit
		case 0x5: // SysEx 8-bit / Mixed Data Set
			ProcessUMPSysEx(Data, Timestamp);
			break;
			
		case 0x0: // Utility (JR Clock, JR Timestamp, etc.)
		default:
			UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] UMP Type:0x%X (ignored)"), MessageType);
			break;
	}
}

void ULibremidiInput::ProcessUMPSystem(const TArray<uint32>& Data, int64 Timestamp)
{
	if (Data.Num() == 0) return;
	
	const uint32 Word0 = Data[0];
	const uint8 Status = (Word0 >> 16) & 0xFF;
	
	if (Status == FLibremidiConstants::SongPosition)
	{
		const int32 Beats = ((Word0 >> 8) & 0x7F) | ((Word0 & 0x7F) << 7);
		UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI IN] SongPos: %d beats"), Beats);
		OnSongPosition.Broadcast(this, Beats, Timestamp);
	}
	else if (Status == FLibremidiConstants::SongSelect)
	{
		const int32 Song = (Word0 >> 8) & 0x7F;
		UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI IN] SongSelect: %d"), Song);
		OnSongSelect.Broadcast(this, Song, Timestamp);
	}
	else
	{
		BroadcastSystemRealTime(this, Status, Timestamp);
	}
}

void ULibremidiInput::ProcessUMPMidi1(const TArray<uint32>& Data, int64 Timestamp)
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

void ULibremidiInput::ProcessUMPMidi2(const TArray<uint32>& Data, int64 Timestamp)
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

void ULibremidiInput::ProcessUMPSysEx(const TArray<uint32>& Data, int64 Timestamp)
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
				UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI IN] SysEx: %d bytes"), PacketData.Num());
				OnSysEx.Broadcast(this, PacketData, Timestamp);
			}
			break;
		
		case 1: // Start
			UMPSysExBuffer = MoveTemp(PacketData);
			bUMPSysExInProgress = true;
			UMPSysExStartTimestamp = Timestamp;
			UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI IN] SysEx Start: %d bytes"), UMPSysExBuffer.Num());
			break;
		
		case 2: // Continue
			if (bUMPSysExInProgress)
			{
				UMPSysExBuffer.Append(MoveTemp(PacketData));
				UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI IN] SysEx Continue: total %d bytes"), UMPSysExBuffer.Num());
			}
			else
			{
				UE_LOG(LogLibremidi4UE, Warning, TEXT("[MIDI IN] SysEx Continue without Start"));
			}
			break;
		
		case 3: // End
			if (bUMPSysExInProgress)
			{
				UMPSysExBuffer.Append(MoveTemp(PacketData));
				UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI IN] SysEx Complete: %d bytes"), UMPSysExBuffer.Num());
				
				OnSysEx.Broadcast(this, UMPSysExBuffer, UMPSysExStartTimestamp);
				
				UMPSysExBuffer.Reset();
				bUMPSysExInProgress = false;
				UMPSysExStartTimestamp = 0;
			}
			else
			{
				UE_LOG(LogLibremidi4UE, Warning, TEXT("[MIDI IN] SysEx End without Start"));
			}
			break;
		
		default:
			UE_LOG(LogLibremidi4UE, Warning, TEXT("[MIDI IN] SysEx Unknown status: %d"), Status);
			break;
	}
}

