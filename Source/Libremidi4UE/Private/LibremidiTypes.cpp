#include "LibremidiTypes.h"

// FMidiPortInfo implementation

FLibremidiPortInfo::FLibremidiPortInfo(const libremidi::port_information& InPort)
	: ClientHandle(static_cast<int64>(InPort.client))
	, PortHandle(static_cast<int64>(InPort.port))
	, Manufacturer(UTF8_TO_TCHAR(InPort.manufacturer.c_str()))
	, DeviceName(UTF8_TO_TCHAR(InPort.device_name.c_str()))
	, PortName(UTF8_TO_TCHAR(InPort.port_name.c_str()))
	, DisplayName(UTF8_TO_TCHAR(InPort.display_name.c_str()))
	, PortType(static_cast<ELibremidiPortType>(InPort.type))
{
	ParseContainerIdentifier(InPort.container);
	ParseDeviceIdentifier(InPort.device);
}

void FLibremidiPortInfo::ParseContainerIdentifier(const libremidi::container_identifier& Container)
{
	if (std::holds_alternative<libremidi::uuid>(Container))
	{
		ContainerType = ELibremidiContainerType::UUID;
		const auto& UUID = std::get<libremidi::uuid>(Container);
		ContainerUUID = BytesToHexString(UUID.bytes.data(), UUID.bytes.size());
	}
	else if (std::holds_alternative<std::string>(Container))
	{
		ContainerType = ELibremidiContainerType::String;
		ContainerString = UTF8_TO_TCHAR(std::get<std::string>(Container).c_str());
	}
	else if (std::holds_alternative<std::uint64_t>(Container))
	{
		ContainerType = ELibremidiContainerType::Integer;
		ContainerInteger = static_cast<int64>(std::get<std::uint64_t>(Container));
	}
	else
	{
		ContainerType = ELibremidiContainerType::None;
	}
}

void FLibremidiPortInfo::ParseDeviceIdentifier(const libremidi::device_identifier& Device)
{
	if (std::holds_alternative<std::string>(Device))
	{
		DeviceType = ELibremidiDeviceType::String;
		DeviceString = UTF8_TO_TCHAR(std::get<std::string>(Device).c_str());
	}
	else if (std::holds_alternative<std::uint64_t>(Device))
	{
		DeviceType = ELibremidiDeviceType::Integer;
		DeviceInteger = static_cast<int64>(std::get<std::uint64_t>(Device));
	}
	else
	{
		DeviceType = ELibremidiDeviceType::None;
	}
}

FString FLibremidiPortInfo::BytesToHexString(const uint8_t* Bytes, size_t Length)
{
	FString Result;
	Result.Reserve(static_cast<int32>(Length * 2));
	for (size_t i = 0; i < Length; ++i)
	{
		Result += FString::Printf(TEXT("%02X"), Bytes[i]);
	}
	return Result;
}

// API conversion implementation

namespace LibremidiTypeConversion
{
	libremidi::API ToLibremidiAPI(ELibremidiAPI API)
	{
		switch (API)
		{
		case ELibremidiAPI::Unspecified: return libremidi::API::UNSPECIFIED;
		case ELibremidiAPI::Dummy: return libremidi::API::DUMMY;
		case ELibremidiAPI::AlsaSeq: return libremidi::API::ALSA_SEQ;
		case ELibremidiAPI::AlsaRaw: return libremidi::API::ALSA_RAW;
		case ELibremidiAPI::CoreMidi: return libremidi::API::COREMIDI;
		case ELibremidiAPI::WindowsMM: return libremidi::API::WINDOWS_MM;
		case ELibremidiAPI::WindowsUWP: return libremidi::API::WINDOWS_UWP;
		case ELibremidiAPI::Jack: return libremidi::API::JACK_MIDI;
		case ELibremidiAPI::PipeWire: return libremidi::API::PIPEWIRE;
		case ELibremidiAPI::Emscripten: return libremidi::API::WEBMIDI;
		case ELibremidiAPI::WindowsMidiServices: return libremidi::API::WINDOWS_MIDI_SERVICES;
		case ELibremidiAPI::AlsaSeqUMP: return libremidi::API::ALSA_SEQ_UMP;
		case ELibremidiAPI::AlsaRawUMP: return libremidi::API::ALSA_RAW_UMP;
		case ELibremidiAPI::CoreMidiUMP: return libremidi::API::COREMIDI_UMP;
		case ELibremidiAPI::JackUMP: return libremidi::API::JACK_UMP;
		case ELibremidiAPI::PipeWireUMP: return libremidi::API::PIPEWIRE_UMP;
		default: return libremidi::API::UNSPECIFIED;
		}
	}

	ELibremidiAPI FromLibremidiAPI(libremidi::API API)
	{
		switch (API)
		{
		case libremidi::API::UNSPECIFIED: return ELibremidiAPI::Unspecified;
		case libremidi::API::DUMMY: return ELibremidiAPI::Dummy;
		case libremidi::API::ALSA_SEQ: return ELibremidiAPI::AlsaSeq;
		case libremidi::API::ALSA_RAW: return ELibremidiAPI::AlsaRaw;
		case libremidi::API::COREMIDI: return ELibremidiAPI::CoreMidi;
		case libremidi::API::WINDOWS_MM: return ELibremidiAPI::WindowsMM;
		case libremidi::API::WINDOWS_UWP: return ELibremidiAPI::WindowsUWP;
		case libremidi::API::JACK_MIDI: return ELibremidiAPI::Jack;
		case libremidi::API::PIPEWIRE: return ELibremidiAPI::PipeWire;
		case libremidi::API::WEBMIDI: return ELibremidiAPI::Emscripten;
		case libremidi::API::WINDOWS_MIDI_SERVICES: return ELibremidiAPI::WindowsMidiServices;
		case libremidi::API::ALSA_SEQ_UMP: return ELibremidiAPI::AlsaSeqUMP;
		case libremidi::API::ALSA_RAW_UMP: return ELibremidiAPI::AlsaRawUMP;
		case libremidi::API::COREMIDI_UMP: return ELibremidiAPI::CoreMidiUMP;
		case libremidi::API::JACK_UMP: return ELibremidiAPI::JackUMP;
		case libremidi::API::PIPEWIRE_UMP: return ELibremidiAPI::PipeWireUMP;
		default: return ELibremidiAPI::Unspecified;
		}
	}
}
