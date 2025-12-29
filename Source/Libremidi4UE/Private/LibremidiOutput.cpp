/**
 * @file LibremidiOutput.cpp
 * @brief MIDI Output implementation with unified UMP processing
 * 
 * ## Unified High-Resolution Processing
 * This implementation ALWAYS sends MIDI data as UMP (MIDI 2.0) format internally.
 * libremidi automatically handles format conversion:
 * 
 * - **MIDI 1.0 devices**: High-resolution data is automatically downsampled (16-bit ? 7-bit)
 * - **MIDI 2.0 devices**: Native high-resolution data is preserved
 * 
 * ## Data Conversion
 * Normalized float values are converted to high-resolution MIDI 2.0:
 * - Velocity: float 0.0-1.0 ? uint16 (0-65535)
 * - Control Change / Pressure: float 0.0-1.0 ? uint32 (0-4294967295)
 * - Pitch Bend: float -1.0-1.0 ? int32 (-2147483648 to 2147483647)
 * 
 * ## Thread Safety
 * Send operations are thread-safe and can be called from any thread.
 * Error broadcasts are dispatched to the game thread via AsyncTask.
 */

#include "LibremidiOutput.h"
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
	 * Normalization constants for float ? MIDI data conversion.
	 * Converts normalized float values to maximum resolution supported by MIDI 2.0.
	 */
	namespace MidiConversion
	{
		constexpr uint32 Velocity16Bit = 65535;      // 0.0-1.0 ? 0-65535 (MIDI 2.0)
		constexpr uint32 Value32Bit = 4294967295;    // 0.0-1.0 ? 0-4294967295 (MIDI 2.0)
		constexpr int32  PitchBend32Bit = 2147483647; // -1.0-1.0 ? -2147483648-2147483647 (MIDI 2.0)
	}

	/**
	 * Clamp and convert normalized float to high-resolution MIDI 2.0 values.
	 * libremidi will automatically downsample to MIDI 1.0 if needed.
	 */
	FORCEINLINE uint16 FloatToVelocity16(float Value)
	{
		return static_cast<uint16>(FMath::Clamp(Value, 0.0f, 1.0f) * MidiConversion::Velocity16Bit);
	}

	FORCEINLINE uint32 FloatToValue32(float Value)
	{
		return static_cast<uint32>(FMath::Clamp(Value, 0.0f, 1.0f) * MidiConversion::Value32Bit);
	}

	FORCEINLINE int32 FloatToPitchBend32(float Value)
	{
		return static_cast<int32>(FMath::Clamp(Value, -1.0f, 1.0f) * MidiConversion::PitchBend32Bit);
	}

	/**
	 * Build a MIDI 2.0 UMP message (Type 4, 64-bit channel voice message)
	 * @param Status MIDI status byte (0x8-0xE)
	 * @param Channel MIDI channel (0-15)
	 * @param Data1 First data byte
	 * @param Data2 Second data value (16-bit or 32-bit)
	 * @return Two 32-bit UMP words
	 */
	FORCEINLINE void BuildUMPMessage(uint32 OutData[2], uint8 Status, int32 Channel, int32 Data1, uint32 Data2)
	{
		OutData[0] = (0x4 << 28) | (Status << 20) | (Channel << 16) | (Data1 << 8);
		OutData[1] = Data2;
	}

	/**
	 * Send a UMP message and handle errors
	 */
	FORCEINLINE bool SendUMP(libremidi::midi_out* MidiOut, const uint32 Data[2], const TCHAR* ErrorMsg)
	{
		if (const auto Error = MidiOut->send_ump(Data, 2); Error != stdx::error{})
		{
			return false;
		}
		return true;
	}
}

// ============================================================================
// Lifecycle
// ============================================================================

ULibremidiOutput::ULibremidiOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsVirtualPort(false)
	, bEnableAutoReconnect(true)
{
}

void ULibremidiOutput::BeginDestroy()
{
	UnregisterHotPlugDelegate();
	ClosePort();
	Super::BeginDestroy();
}

// ============================================================================
// Factory & Subsystem Access
// ============================================================================

ULibremidiOutput* ULibremidiOutput::CreateMidiOutput(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] CreateMidiOutput failed: Invalid WorldContextObject"));
		return nullptr;
	}

	ULibremidiOutput* NewOutput = NewObject<ULibremidiOutput>(WorldContextObject);
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Created new MidiOutput instance"));
	return NewOutput;
}

ULibremidiEngineSubsystem* ULibremidiOutput::GetSubsystem() const
{
	return UE::MIDI::Private::GetEngineSubsystem();
}

libremidi::output_port ULibremidiOutput::ConvertPortInfo(const FLibremidiPortInfo& PortInfo) const
{
	return UE::MIDI::Private::ToLibremidiOutputPort(PortInfo);
}

// ============================================================================
// Port Management
// ============================================================================

bool ULibremidiOutput::CreateMidiOut(libremidi::API API)
{
	try
	{
		MidiOut = MakeUnique<libremidi::midi_out>(CreateOutputConfiguration(), API);
		
		if (MidiOut)
		{
			UE::MIDI::Private::LogBackendType(MidiOut->get_current_api(), false);
			return true;
		}
		else
		{
			UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] Failed to create midi_out instance"));
			return false;
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] Exception creating midi_out: %s"), UTF8_TO_TCHAR(e.what()));
		HandleError(FString::Printf(TEXT("Failed to create MIDI output: %s"), UTF8_TO_TCHAR(e.what())));
		return false;
	}
}

bool ULibremidiOutput::OpenPort(const FLibremidiPortInfo& PortInfo, const FString& ClientName)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Opening port '%s'..."), *PortInfo.DisplayName);
	
	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] Failed to get LibremidiEngineSubsystem"));
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	// Check if this instance already has a port open
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiOut.Get(), TEXT("Output")))
	{
		if (CurrentPortInfo == PortInfo)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Port '%s' is already open"), *PortInfo.DisplayName);
			return true;
		}
		
		const FString ErrorMsg = FString::Printf(
			TEXT("Close port '%s' before opening '%s'"),
			*CurrentPortInfo.DisplayName,
			*PortInfo.DisplayName
		);
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] %s"), *ErrorMsg);
		HandleError(ErrorMsg);
		return false;
	}

	// Check if port is already open by another instance
	if (Subsystem->FindActiveOutputPort(PortInfo))
	{
		const FString ErrorMsg = FString::Printf(TEXT("Port '%s' is already in use"), *PortInfo.DisplayName);
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] %s"), *ErrorMsg);
		HandleError(ErrorMsg);
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MidiOutput] Using API: %s"), *UEnum::GetValueAsString(API));
	
	if (!CreateMidiOut(LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiOut->open_port(ConvertPortInfo(PortInfo), TCHAR_TO_UTF8(*ClientName)); 
		Error != stdx::error{})
	{
		MidiOut.Reset();
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] Failed to open port '%s'"), *PortInfo.DisplayName);
		HandleError(FString::Printf(TEXT("Failed to open port '%s'"), *PortInfo.DisplayName));
		return false;
	}

	LastClientName = ClientName;
	NotifyPortOpened(PortInfo, false);

	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Opened port '%s' (API: %s)"), 
		*PortInfo.DisplayName, *UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiOutput::OpenVirtualPort(const FString& PortName)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Opening virtual port '%s'..."), *PortName);
	
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiOut.Get(), TEXT("Output")))
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] A port is already open"));
		HandleError(TEXT("Port is already open"));
		return false;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] Failed to get LibremidiEngineSubsystem"));
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	if (!CreateMidiOut(LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiOut->open_virtual_port(TCHAR_TO_UTF8(*PortName)); 
		Error != stdx::error{})
	{
		MidiOut.Reset();
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] Failed to open virtual port '%s'"), *PortName);
		HandleError(FString::Printf(TEXT("Failed to open virtual port '%s'"), *PortName));
		return false;
	}

	FLibremidiPortInfo VirtualPortInfo;
	VirtualPortInfo.DisplayName = PortName;
	VirtualPortInfo.PortName = PortName;
	
	LastClientName = PortName;
	NotifyPortOpened(VirtualPortInfo, true);

	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Opened virtual port '%s' (API: %s)"), 
		*PortName, *UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiOutput::ClosePort()
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MidiOutput] No port to close"));
		return false;
	}

	const FString PortName = CurrentPortInfo.DisplayName;
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Closing port '%s'..."), *PortName);

	// Store port info before closing (for potential reconnection)
	if (!IsPortConnected() && bEnableAutoReconnect)
	{
		LastDisconnectedPort = CurrentPortInfo;
	}

	if (const auto Error = MidiOut->close_port(); Error != stdx::error{})
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("[MidiOutput] Failed to close port"));
		HandleError(TEXT("Failed to close port"));
		return false;
	}

	NotifyPortClosed();
	MidiOut.Reset();
	
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Closed port '%s'"), *PortName);
	return true;
}

// ============================================================================
// Port Status
// ============================================================================

bool ULibremidiOutput::IsPortOpen() const
{
	return MidiOut && MidiOut->is_port_open();
}

bool ULibremidiOutput::IsPortConnected() const
{
	return MidiOut && MidiOut->is_port_connected();
}

ELibremidiAPI ULibremidiOutput::GetCurrentAPI() const
{
	return MidiOut ? LibremidiTypeConversion::FromLibremidiAPI(MidiOut->get_current_api()) 
	               : ELibremidiAPI::Unspecified;
}

// ============================================================================
// Raw Message Sending
// ============================================================================

bool ULibremidiOutput::SendMessage(const TArray<uint8>& Data)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

	// Log raw MIDI message
	FString DataStr;
	const int32 DisplayCount = FMath::Min(Data.Num(), 16);
	DataStr.Reserve(DisplayCount * 3 + 4);
	
	for (int32 i = 0; i < DisplayCount; ++i)
	{
		DataStr += FString::Printf(TEXT("%02X "), Data[i]);
	}
	if (Data.Num() > 16)
	{
		DataStr += TEXT("...");
	}
	
	UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI OUT] Raw MIDI1.0 Size:%d Data:[%s]"), 
		Data.Num(), *DataStr);

	libremidi::message Msg;
	Msg.bytes.assign(Data.GetData(), Data.GetData() + Data.Num());

	if (const auto Error = MidiOut->send_message(Msg); Error != stdx::error{})
	{
		HandleError(TEXT("Failed to send message"));
		return false;
	}

	return true;
}

bool ULibremidiOutput::SendMessageUMP(const TArray<uint32>& Data)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

	if (Data.Num() == 0 || Data.Num() > 4)
	{
		HandleError(TEXT("Invalid UMP message size (must be 1-4 words)"));
		return false;
	}

	// Log UMP message
	FString DataStr;
	DataStr.Reserve(Data.Num() * 9);
	
for (int32 i = 0; i < Data.Num(); ++i)
	{
		DataStr += FString::Printf(TEXT("%08X "), Data[i]);
	}
	
	UE_LOG(LogLibremidi4UE, VeryVerbose, TEXT("[MIDI OUT] Raw UMP Words:%d Data:[%s]"), 
		Data.Num(), *DataStr);

	if (const auto Error = MidiOut->send_ump(Data.GetData(), Data.Num()); Error != stdx::error{})
	{
		HandleError(TEXT("Failed to send UMP message"));
		return false;
	}

	return true;
}

// ============================================================================
// Configuration & Error Handling
// ============================================================================

libremidi::output_configuration ULibremidiOutput::CreateOutputConfiguration()
{
	libremidi::output_configuration Config;
	
	Config.on_error = [this](std::string_view errorText, const libremidi::source_location& loc)
	{
		HandleError(FString::Printf(TEXT("%s at %s:%d"), 
			UTF8_TO_TCHAR(errorText.data()), 
			UTF8_TO_TCHAR(loc.file_name()), 
			loc.line()));
	};

	return Config;
}

void ULibremidiOutput::HandleError(const FString& ErrorMessage)
{
	UE::MIDI::Private::DispatchErrorToGameThread(this, ErrorMessage, OnError);
}

void ULibremidiOutput::NotifyPortOpened(const FLibremidiPortInfo& PortInfo, bool bVirtual)
{
	CurrentPortInfo = PortInfo;
	bIsVirtualPort = bVirtual;

	if (ULibremidiEngineSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->RegisterOutputPort(this);
	}

	OnPortOpened.Broadcast(PortInfo);
}

void ULibremidiOutput::NotifyPortClosed()
{
	if (ULibremidiEngineSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->UnregisterOutputPort(this);
	}

	CurrentPortInfo = FLibremidiPortInfo();
	bIsVirtualPort = false;
}

// ============================================================================
// Auto-Reconnection
// ============================================================================

void ULibremidiOutput::SetAutoReconnect(bool bEnable)
{
	if (bEnableAutoReconnect == bEnable)
	{
		return;
	}

	bEnableAutoReconnect = bEnable;

	if (bEnable)
	{
		RegisterHotPlugDelegate();
		UE_LOG(LogLibremidi4UE, Log, TEXT("MidiOutput: Auto-reconnection enabled for '%s'"), 
			*CurrentPortInfo.DisplayName);
	}
	else
	{
		UnregisterHotPlugDelegate();
		UE_LOG(LogLibremidi4UE, Log, TEXT("MidiOutput: Auto-reconnection disabled"));
	}
}

void ULibremidiOutput::RegisterHotPlugDelegate()
{
	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}

	UnregisterHotPlugDelegate();

	HotPlugDelegateHandle = Subsystem->OnOutputPortReconnected.AddUObject(
		this, 
		&ULibremidiOutput::HandleHotPlugEvent
	);

	UE_LOG(LogLibremidi4UE, Verbose, TEXT("MidiOutput: Registered hot-plug delegate"));
}

void ULibremidiOutput::UnregisterHotPlugDelegate()
{
	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (Subsystem && HotPlugDelegateHandle.IsValid())
	{
		Subsystem->OnOutputPortReconnected.Remove(HotPlugDelegateHandle);
		HotPlugDelegateHandle.Reset();
		UE_LOG(LogLibremidi4UE, Verbose, TEXT("MidiOutput: Unregistered hot-plug delegate"));
	}
}

void ULibremidiOutput::HandleHotPlugEvent(const FLibremidiPortInfo& ReconnectedPort)
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

	UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Auto-reconnecting to '%s'..."), 
		*ReconnectedPort.DisplayName);

	if (IsPortOpen())
	{
		ClosePort();
	}

	if (OpenPort(ReconnectedPort, LastClientName))
	{
		UE_LOG(LogLibremidi4UE, Log, TEXT("[MidiOutput] Auto-reconnect successful"));
	}
	else
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("[MidiOutput] Auto-reconnect failed"));
	}
}

// ============================================================================
// High-Level MIDI Message Sending (Normalized Float to High-Resolution)
// ============================================================================

bool ULibremidiOutput::SendNoteOn(int32 Channel, int32 Note, float Velocity)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

	Channel = FLibremidiUtils::ClampChannel(Channel);
	Note = FLibremidiUtils::ClampNote(Note);
	const uint16 Vel16 = FloatToVelocity16(Velocity);

	UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI OUT] NoteOn  Ch:%2d Note:%3d Vel:%.3f"), 
		Channel, Note, Velocity);

	uint32 UMPData[2];
	BuildUMPMessage(UMPData, 0x9, Channel, Note, Vel16 << 16);
	
	if (!SendUMP(MidiOut.Get(), UMPData, TEXT("Failed to send Note On")))
	{
		HandleError(TEXT("Failed to send Note On"));
		return false;
	}
	return true;
}

bool ULibremidiOutput::SendNoteOff(int32 Channel, int32 Note, float Velocity)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

	Channel = FLibremidiUtils::ClampChannel(Channel);
	Note = FLibremidiUtils::ClampNote(Note);
	const uint16 Vel16 = FloatToVelocity16(Velocity);

	UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI OUT] NoteOff Ch:%2d Note:%3d Vel:%.3f"), 
		Channel, Note, Velocity);

	uint32 UMPData[2];
	BuildUMPMessage(UMPData, 0x8, Channel, Note, Vel16 << 16);
	
	if (!SendUMP(MidiOut.Get(), UMPData, TEXT("Failed to send Note Off")))
	{
		HandleError(TEXT("Failed to send Note Off"));
		return false;
	}
	return true;
}

bool ULibremidiOutput::SendControlChange(int32 Channel, int32 Controller, float Value)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

	Channel = FLibremidiUtils::ClampChannel(Channel);
	Controller = FLibremidiUtils::ClampController(Controller);
	const uint32 Val32 = FloatToValue32(Value);

	UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI OUT] CC      Ch:%2d CC:%3d Val:%.3f"), 
		Channel, Controller, Value);

	uint32 UMPData[2];
	BuildUMPMessage(UMPData, 0xB, Channel, Controller, Val32);
	
	if (!SendUMP(MidiOut.Get(), UMPData, TEXT("Failed to send Control Change")))
	{
		HandleError(TEXT("Failed to send Control Change"));
		return false;
	}
	return true;
}

bool ULibremidiOutput::SendProgramChange(int32 Channel, int32 Program)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

	Channel = FLibremidiUtils::ClampChannel(Channel);
	Program = FLibremidiUtils::ClampProgram(Program);

	UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI OUT] Program Ch:%2d Prog:%d"), 
		Channel, Program);

	uint32 UMPData[2];
	BuildUMPMessage(UMPData, 0xC, Channel, 0, Program << 24);
	
	if (!SendUMP(MidiOut.Get(), UMPData, TEXT("Failed to send Program Change")))
	{
		HandleError(TEXT("Failed to send Program Change"));
		return false;
	}
	return true;
}

bool ULibremidiOutput::SendPitchBend(int32 Channel, float Value)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

	Channel = FLibremidiUtils::ClampChannel(Channel);
	const uint32 Bend32 = static_cast<uint32>(FloatToPitchBend32(Value));

	UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI OUT] Pitch   Ch:%2d Val:%.3f"), 
		Channel, Value);

	uint32 UMPData[2];
	BuildUMPMessage(UMPData, 0xE, Channel, 0, Bend32);
	
	if (!SendUMP(MidiOut.Get(), UMPData, TEXT("Failed to send Pitch Bend")))
	{
		HandleError(TEXT("Failed to send Pitch Bend"));
		return false;
	}
	return true;
}

bool ULibremidiOutput::SendChannelPressure(int32 Channel, float Pressure)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

	Channel = FLibremidiUtils::ClampChannel(Channel);
	const uint32 Press32 = FloatToValue32(Pressure);

	UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI OUT] ChanAT  Ch:%2d Press:%.3f"), 
		Channel, Pressure);

	uint32 UMPData[2];
	BuildUMPMessage(UMPData, 0xD, Channel, 0, Press32);
	
	if (!SendUMP(MidiOut.Get(), UMPData, TEXT("Failed to send Channel Pressure")))
	{
		HandleError(TEXT("Failed to send Channel Pressure"));
		return false;
	}
	return true;
}

bool ULibremidiOutput::SendPolyPressure(int32 Channel, int32 Note, float Pressure)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

	Channel = FLibremidiUtils::ClampChannel(Channel);
	Note = FLibremidiUtils::ClampNote(Note);
	const uint32 Press32 = FloatToValue32(Pressure);

	UE_LOG(LogLibremidi4UE, Verbose, TEXT("[MIDI OUT] PolyAT  Ch:%2d Note:%3d Press:%.3f"), 
		Channel, Note, Pressure);

	uint32 UMPData[2];
	BuildUMPMessage(UMPData, 0xA, Channel, Note, Press32);
	
	if (!SendUMP(MidiOut.Get(), UMPData, TEXT("Failed to send Poly Pressure")))
	{
		HandleError(TEXT("Failed to send Poly Pressure"));
		return false;
	}
	return true;
}

bool ULibremidiOutput::SendAllNotesOff(int32 Channel)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI OUT] AllNotesOff Ch:%d"), Channel);
	return SendControlChange(Channel, 123, 0.0f);
}

bool ULibremidiOutput::SendAllSoundOff(int32 Channel)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI OUT] AllSoundOff Ch:%d"), Channel);
	return SendControlChange(Channel, 120, 0.0f);
}

bool ULibremidiOutput::SendResetAllControllers(int32 Channel)
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("[MIDI OUT] ResetAllControllers Ch:%d"), Channel);
	return SendControlChange(Channel, 121, 0.0f);
}

