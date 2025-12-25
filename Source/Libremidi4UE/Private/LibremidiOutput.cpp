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

ULibremidiOutput::ULibremidiOutput()
{
}

ULibremidiOutput::~ULibremidiOutput() = default;

void ULibremidiOutput::BeginDestroy()
{
	ClosePort();
	Super::BeginDestroy();
}

ULibremidiOutput* ULibremidiOutput::CreateMidiOutput(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("CreateMidiOutput: Invalid WorldContextObject"));
		return nullptr;
	}

	return NewObject<ULibremidiOutput>(WorldContextObject);
}

ULibremidiEngineSubsystem* ULibremidiOutput::GetSubsystem() const
{
	return UE::MIDI::Private::GetEngineSubsystem();
}

libremidi::output_port ULibremidiOutput::ConvertPortInfo(const FMidiPortInfo& PortInfo) const
{
	return UE::MIDI::Private::ToLibremidiOutputPort(PortInfo);
}

bool ULibremidiOutput::CreateMidiOut(libremidi::API API)
{
	try
	{
		MidiOut = MakeUnique<libremidi::midi_out>(CreateOutputConfiguration(), API);
		
		if (MidiOut)
		{
			UE::MIDI::Private::LogBackendType(MidiOut->get_current_api(), false);
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		HandleError(FString::Printf(TEXT("Failed to create MIDI output: %s"), UTF8_TO_TCHAR(e.what())));
		return false;
	}
}

bool ULibremidiOutput::OpenPort(const FMidiPortInfo& PortInfo, const FString& ClientName)
{
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiOut.Get(), TEXT("Output")))
	{
		return false;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	if (ULibremidiOutput* ExistingPort = Subsystem->FindActiveOutputPort(PortInfo))
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("MidiOutput: Port '%s' is already open by another instance"), *PortInfo.DisplayName);
		HandleError(FString::Printf(TEXT("Port '%s' is already open"), *PortInfo.DisplayName));
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	if (!CreateMidiOut(LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiOut->open_port(ConvertPortInfo(PortInfo), TCHAR_TO_UTF8(*ClientName)); 
		Error != stdx::error{})
	{
		HandleError(FString::Printf(TEXT("Failed to open port '%s'"), *PortInfo.DisplayName));
		MidiOut.Reset();
		return false;
	}

	NotifyPortOpened(PortInfo, false);

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiOutput: Opened port '%s' with API: %s (UMP processing)"), 
		*PortInfo.DisplayName, 
		*UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiOutput::OpenVirtualPort(const FString& PortName)
{
	if (UE::MIDI::Private::IsPortAlreadyOpen(MidiOut.Get(), TEXT("Output")))
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
	if (!CreateMidiOut(LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiOut->open_virtual_port(TCHAR_TO_UTF8(*PortName)); 
		Error != stdx::error{})
	{
		HandleError(FString::Printf(TEXT("Failed to open virtual port '%s'"), *PortName));
		MidiOut.Reset();
		return false;
	}

	FMidiPortInfo VirtualPortInfo;
	VirtualPortInfo.DisplayName = PortName;
	VirtualPortInfo.PortName = PortName;
	NotifyPortOpened(VirtualPortInfo, true);

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiOutput: Opened virtual port '%s' with API: %s (UMP processing)"), 
		*PortName, 
		*UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiOutput::ClosePort()
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		return false;
	}

	if (const auto Error = MidiOut->close_port(); Error != stdx::error{})
	{
		HandleError(TEXT("Failed to close port"));
		return false;
	}

	NotifyPortClosed();

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiOutput: Port closed"));
	MidiOut.Reset();
	return true;
}

bool ULibremidiOutput::SendMessage(const TArray<uint8>& Data)
{
	if (!MidiOut || !MidiOut->is_port_open())
	{
		HandleError(TEXT("Port is not open"));
		return false;
	}

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

	if (const auto Error = MidiOut->send_ump(Data.GetData(), Data.Num()); Error != stdx::error{})
	{
		HandleError(TEXT("Failed to send UMP message"));
		return false;
	}

	return true;
}

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

void ULibremidiOutput::NotifyPortOpened(const FMidiPortInfo& PortInfo, bool bVirtual)
{
	CurrentPortInfo = PortInfo;
	bIsVirtualPort = bVirtual;

	if (ULibremidiEngineSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->RegisterOutputPort(this);
	}
}

void ULibremidiOutput::NotifyPortClosed()
{
	if (ULibremidiEngineSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->UnregisterOutputPort(this);
	}

	CurrentPortInfo = FMidiPortInfo();
	bIsVirtualPort = false;
}

// ============================================================================
// High-Level MIDI Message Sending (Normalized Float to High-Resolution)
// ============================================================================

namespace
{
	/**
	 * Normalization constants for float ? MIDI data conversion.
	 * Converts normalized float values to maximum resolution supported by MIDI 2.0.
	 */
	namespace MidiDenormalization
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
		return static_cast<uint16>(FMath::Clamp(Value, 0.0f, 1.0f) * MidiDenormalization::Velocity16Bit);
	}

	FORCEINLINE uint32 FloatToValue32(float Value)
	{
		return static_cast<uint32>(FMath::Clamp(Value, 0.0f, 1.0f) * MidiDenormalization::Value32Bit);
	}

	FORCEINLINE int32 FloatToPitchBend32(float Value)
	{
		return static_cast<int32>(FMath::Clamp(Value, -1.0f, 1.0f) * MidiDenormalization::PitchBend32Bit);
	}
}

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

	// Build MIDI 2.0 UMP Note On message (Type 4, 64-bit)
	const uint32 Word0 = (0x4 << 28) | (0x9 << 20) | (Channel << 16) | (Note << 8);
	const uint32 Word1 = (Vel16 << 16);

	const uint32 UMPData[] = {Word0, Word1};

	if (const auto Error = MidiOut->send_ump(UMPData, 2); Error != stdx::error{})
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

	// Build MIDI 2.0 UMP Note Off message (Type 4, 64-bit)
	const uint32 Word0 = (0x4 << 28) | (0x8 << 20) | (Channel << 16) | (Note << 8);
	const uint32 Word1 = (Vel16 << 16);

	const uint32 UMPData[] = {Word0, Word1};

	if (const auto Error = MidiOut->send_ump(UMPData, 2); Error != stdx::error{})
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

	// Build MIDI 2.0 UMP Control Change message (Type 4, 64-bit)
	const uint32 Word0 = (0x4 << 28) | (0xB << 20) | (Channel << 16) | (Controller << 8);
	const uint32 Word1 = Val32;

	const uint32 UMPData[] = {Word0, Word1};

	if (const auto Error = MidiOut->send_ump(UMPData, 2); Error != stdx::error{})
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

	// Build MIDI 2.0 UMP Program Change message (Type 4, 64-bit)
	const uint32 Word0 = (0x4 << 28) | (0xC << 20) | (Channel << 16);
	const uint32 Word1 = (Program << 24);

	const uint32 UMPData[] = {Word0, Word1};

	if (const auto Error = MidiOut->send_ump(UMPData, 2); Error != stdx::error{})
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

	// Build MIDI 2.0 UMP Pitch Bend message (Type 4, 64-bit)
	const uint32 Word0 = (0x4 << 28) | (0xE << 20) | (Channel << 16);
	const uint32 Word1 = Bend32;

	const uint32 UMPData[] = {Word0, Word1};

	if (const auto Error = MidiOut->send_ump(UMPData, 2); Error != stdx::error{})
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

	// Build MIDI 2.0 UMP Channel Pressure message (Type 4, 64-bit)
	const uint32 Word0 = (0x4 << 28) | (0xD << 20) | (Channel << 16);
	const uint32 Word1 = Press32;

	const uint32 UMPData[] = {Word0, Word1};

	if (const auto Error = MidiOut->send_ump(UMPData, 2); Error != stdx::error{})
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

	// Build MIDI 2.0 UMP Polyphonic Key Pressure message (Type 4, 64-bit)
	const uint32 Word0 = (0x4 << 28) | (0xA << 20) | (Channel << 16) | (Note << 8);
	const uint32 Word1 = Press32;

	const uint32 UMPData[] = {Word0, Word1};

	if (const auto Error = MidiOut->send_ump(UMPData, 2); Error != stdx::error{})
	{
		HandleError(TEXT("Failed to send Poly Pressure"));
		return false;
	}

	return true;
}

bool ULibremidiOutput::SendAllNotesOff(int32 Channel)
{
	return SendControlChange(Channel, 123, 0.0f);
}

bool ULibremidiOutput::SendAllSoundOff(int32 Channel)
{
	return SendControlChange(Channel, 120, 0.0f);
}

bool ULibremidiOutput::SendResetAllControllers(int32 Channel)
{
	return SendControlChange(Channel, 121, 0.0f);
}

