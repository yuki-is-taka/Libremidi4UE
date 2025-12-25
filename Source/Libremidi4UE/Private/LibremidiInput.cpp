#include "LibremidiInput.h"
#include "LibremidiEngineSubsystem.h"
#include "Libremidi4UELog.h"
#include "Engine/Engine.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

ULibremidiInput::ULibremidiInput()
	: bIsMidi2(false)
{
}

ULibremidiInput::~ULibremidiInput() = default;

void ULibremidiInput::BeginDestroy()
{
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
	return GEngine ? GEngine->GetEngineSubsystem<ULibremidiEngineSubsystem>() : nullptr;
}

libremidi::input_port ULibremidiInput::ConvertPortInfo(const FMidiPortInfo& PortInfo) const
{
	libremidi::input_port Port;
	Port.client = static_cast<std::uintptr_t>(PortInfo.ClientHandle);
	Port.port = static_cast<uint32_t>(PortInfo.PortHandle);
	Port.display_name = TCHAR_TO_UTF8(*PortInfo.DisplayName);
	Port.port_name = TCHAR_TO_UTF8(*PortInfo.PortName);
	Port.device_name = TCHAR_TO_UTF8(*PortInfo.DeviceName);
	Port.manufacturer = TCHAR_TO_UTF8(*PortInfo.Manufacturer);
	return Port;
}

bool ULibremidiInput::CreateMidiIn(bool bUseMidi2, libremidi::API API)
{
	try
	{
		if (bUseMidi2)
		{
			MidiIn = MakeUnique<libremidi::midi_in>(CreateInputConfigurationUMP(), API);
		}
		else
		{
			MidiIn = MakeUnique<libremidi::midi_in>(CreateInputConfiguration(), API);
		}
		bIsMidi2 = bUseMidi2;
		return true;
	}
	catch (const std::exception& e)
	{
		HandleError(FString::Printf(TEXT("Failed to create MIDI input%s: %s"), 
			bUseMidi2 ? TEXT(" (UMP)") : TEXT(""), 
			UTF8_TO_TCHAR(e.what())));
		return false;
	}
}

bool ULibremidiInput::OpenPortInternal(const FMidiPortInfo& PortInfo, const FString& ClientName, bool bUseMidi2)
{
	if (MidiIn && MidiIn->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("MidiInput: A port is already open. Close it first."));
		return false;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	if (!CreateMidiIn(bUseMidi2, LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiIn->open_port(ConvertPortInfo(PortInfo), TCHAR_TO_UTF8(*ClientName)); 
		Error != stdx::error{})
	{
		HandleError(FString::Printf(TEXT("Failed to open port%s '%s'"), 
			bUseMidi2 ? TEXT(" (UMP)") : TEXT(""), 
			*PortInfo.DisplayName));
		MidiIn.Reset();
		return false;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Opened port%s '%s' with API: %s"), 
		bUseMidi2 ? TEXT(" (UMP)") : TEXT(""),
		*PortInfo.DisplayName, 
		*UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiInput::OpenVirtualPortInternal(const FString& PortName, bool bUseMidi2)
{
	if (MidiIn && MidiIn->is_port_open())
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("MidiInput: A port is already open. Close it first."));
		return false;
	}

	ULibremidiEngineSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		HandleError(TEXT("Failed to get LibremidiEngineSubsystem"));
		return false;
	}

	const ELibremidiAPI API = Subsystem->GetCurrentAPI();
	if (!CreateMidiIn(bUseMidi2, LibremidiTypeConversion::ToLibremidiAPI(API)))
	{
		return false;
	}

	if (const auto Error = MidiIn->open_virtual_port(TCHAR_TO_UTF8(*PortName)); 
		Error != stdx::error{})
	{
		HandleError(FString::Printf(TEXT("Failed to open virtual port%s '%s'"), 
			bUseMidi2 ? TEXT(" (UMP)") : TEXT(""), 
			*PortName));
		MidiIn.Reset();
		return false;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Opened virtual port%s '%s' with API: %s"), 
		bUseMidi2 ? TEXT(" (UMP)") : TEXT(""),
		*PortName, 
		*UEnum::GetValueAsString(API));
	return true;
}

bool ULibremidiInput::OpenPort(const FMidiPortInfo& PortInfo, const FString& ClientName)
{
	return OpenPortInternal(PortInfo, ClientName, false);
}

bool ULibremidiInput::OpenPortUMP(const FMidiPortInfo& PortInfo, const FString& ClientName)
{
	return OpenPortInternal(PortInfo, ClientName, true);
}

bool ULibremidiInput::OpenVirtualPort(const FString& PortName)
{
	return OpenVirtualPortInternal(PortName, false);
}

bool ULibremidiInput::OpenVirtualPortUMP(const FString& PortName)
{
	return OpenVirtualPortInternal(PortName, true);
}

bool ULibremidiInput::ClosePort()
{
	if (!MidiIn || !MidiIn->is_port_open())
	{
		return false;
	}

	if (const auto Error = MidiIn->close_port(); Error != stdx::error{})
	{
		HandleError(TEXT("Failed to close port"));
		return false;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("MidiInput: Port closed"));
	MidiIn.Reset();
	bIsMidi2 = false;
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

libremidi::input_configuration ULibremidiInput::CreateInputConfiguration()
{
	libremidi::input_configuration Config;
	
	Config.on_message = [this](libremidi::message&& msg)
	{
		HandleMidiMessage(std::move(msg));
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

void ULibremidiInput::HandleMidiMessage(libremidi::message&& Message)
{
	TArray<uint8> Data(Message.bytes.data(), Message.bytes.size());
	const int64 Timestamp = Message.timestamp;
	
	AsyncTask(ENamedThreads::GameThread, [this, Data = MoveTemp(Data), Timestamp]()
	{
		if (IsValid(this))
		{
			OnMidiMessage.Broadcast(Data, Timestamp);
		}
	});
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
			OnMidiMessageUMP.Broadcast(Data, Timestamp);
		}
	});
}

void ULibremidiInput::HandleError(const FString& ErrorMessage)
{
	UE_LOG(LogLibremidi4UE, Error, TEXT("MidiInput Error: %s"), *ErrorMessage);
	
	AsyncTask(ENamedThreads::GameThread, [this, ErrorMessage]()
	{
		if (IsValid(this))
		{
			OnError.Broadcast(ErrorMessage);
		}
	});
}

