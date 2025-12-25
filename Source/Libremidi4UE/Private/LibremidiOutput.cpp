#include "LibremidiOutput.h"
#include "LibremidiEngineSubsystem.h"
#include "Libremidi4UELog.h"
#include "Engine/Engine.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

ULibremidiOutput::ULibremidiOutput()
	: bIsMidi2(false)
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
	return GEngine ? GEngine->GetEngineSubsystem<ULibremidiEngineSubsystem>() : nullptr;
}

libremidi::output_port ULibremidiOutput::ConvertPortInfo(const FMidiPortInfo& PortInfo) const
{
	libremidi::output_port Port;
	Port.client = static_cast<std::uintptr_t>(PortInfo.ClientHandle);
	Port.port = static_cast<uint32_t>(PortInfo.PortHandle);
	Port.display_name = TCHAR_TO_UTF8(*PortInfo.DisplayName);
	Port.port_name = TCHAR_TO_UTF8(*PortInfo.PortName);
	Port.device_name = TCHAR_TO_UTF8(*PortInfo.DeviceName);
	Port.manufacturer = TCHAR_TO_UTF8(*PortInfo.Manufacturer);
	return Port;
}

bool ULibremidiOutput::CreateMidiOut(bool bUseMidi2, libremidi::API API)
{
	try
	{
		MidiOut = MakeUnique<libremidi::midi_out>(CreateOutputConfiguration(), API);
		bIsMidi2 = bUseMidi2;
		return true;
	}
	catch (const std::exception& e)
	{
		HandleError(FString::Printf(TEXT("Failed to create MIDI output%s: %s"), 
			bUseMidi2 ? TEXT(" (UMP)") : TEXT(""), 
			UTF8_TO_TCHAR(e.what())));
		return false;
	}
}

bool ULibremidiOutput::OpenPortInternal(const FMidiPortInfo& PortInfo, const FString& ClientName, bool bUseMidi2)
{
	if (MidiOut && MidiOut->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("MidiOutput: A port is already open. Close it first."));
		return false;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	if (!CreateMidiOut(bUseMidi2, LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiOut->open_port(ConvertPortInfo(PortInfo), TCHAR_TO_UTF8(*ClientName)); 
		Error != stdx::error{})
	{
		HandleError(FString::Printf(TEXT("Failed to open port%s '%s'"), 
			bUseMidi2 ? TEXT(" (UMP)") : TEXT(""), 
			*PortInfo.DisplayName));
		MidiOut.Reset();
		return false;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiOutput: Opened port%s '%s' with API: %s"), 
		bUseMidi2 ? TEXT(" (UMP)") : TEXT(""),
		*PortInfo.DisplayName, 
		*UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiOutput::OpenVirtualPortInternal(const FString& PortName, bool bUseMidi2)
{
	if (MidiOut && MidiOut->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("MidiOutput: A port is already open. Close it first."));
		return false;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	if (!CreateMidiOut(bUseMidi2, LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiOut->open_virtual_port(TCHAR_TO_UTF8(*PortName)); 
		Error != stdx::error{})
	{
		HandleError(FString::Printf(TEXT("Failed to open virtual port%s '%s'"), 
			bUseMidi2 ? TEXT(" (UMP)") : TEXT(""), 
			*PortName));
		MidiOut.Reset();
		return false;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiOutput: Opened virtual port%s '%s' with API: %s"), 
		bUseMidi2 ? TEXT(" (UMP)") : TEXT(""),
		*PortName, 
		*UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiOutput::OpenPort(const FMidiPortInfo& PortInfo, const FString& ClientName)
{
	return OpenPortInternal(PortInfo, ClientName, false);
}

bool ULibremidiOutput::OpenPortUMP(const FMidiPortInfo& PortInfo, const FString& ClientName)
{
	return OpenPortInternal(PortInfo, ClientName, true);
}

bool ULibremidiOutput::OpenVirtualPort(const FString& PortName)
{
	return OpenVirtualPortInternal(PortName, false);
}

bool ULibremidiOutput::OpenVirtualPortUMP(const FString& PortName)
{
	return OpenVirtualPortInternal(PortName, true);
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

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiOutput: Port closed"));
	MidiOut.Reset();
	bIsMidi2 = false;
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
	UE_LOG(LogLibremidi4UE, Error, TEXT("MidiOutput Error: %s"), *ErrorMessage);
	
	AsyncTask(ENamedThreads::GameThread, [this, ErrorMessage]()
	{
		if (IsValid(this))
		{
			OnError.Broadcast(ErrorMessage);
		}
	});
}

