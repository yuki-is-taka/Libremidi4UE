#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

#include "LibremidiTypes.generated.h"

UENUM(BlueprintType)
enum class ELibremidiAPI : uint8
{
	Unspecified UMETA(DisplayName = "Unspecified (Auto-detect)", ToolTip = "Automatically select the best available API"),
	Dummy UMETA(DisplayName = "Dummy", ToolTip = "Dummy API for testing"),
	AlsaSeq UMETA(DisplayName = "ALSA Sequencer (Linux)", ToolTip = "ALSA Sequencer API for Linux"),
	AlsaRaw UMETA(DisplayName = "ALSA Raw (Linux)", ToolTip = "ALSA Raw MIDI API for Linux"),
	CoreMidi UMETA(DisplayName = "CoreMIDI (macOS/iOS)", ToolTip = "CoreMIDI API for macOS and iOS"),
	WindowsMM UMETA(DisplayName = "Windows Multimedia (Legacy)", ToolTip = "Legacy Windows Multimedia API"),
	WindowsUWP UMETA(DisplayName = "Windows UWP", ToolTip = "Universal Windows Platform MIDI API"),
	Jack UMETA(DisplayName = "JACK Audio", ToolTip = "JACK Audio Connection Kit"),
	PipeWire UMETA(DisplayName = "PipeWire (Linux)", ToolTip = "PipeWire multimedia framework"),
	Emscripten UMETA(DisplayName = "Web MIDI (Emscripten)", ToolTip = "Web MIDI API for browsers"),
	WindowsMidiServices UMETA(DisplayName = "Windows MIDI Services (MIDI 2.0)", ToolTip = "Modern Windows MIDI Services with MIDI 2.0 support"),
	AlsaSeqUMP UMETA(DisplayName = "ALSA Sequencer UMP (MIDI 2.0)", ToolTip = "ALSA Sequencer with Universal MIDI Packet support"),
	AlsaRawUMP UMETA(DisplayName = "ALSA Raw UMP (MIDI 2.0)", ToolTip = "ALSA Raw with Universal MIDI Packet support"),
	CoreMidiUMP UMETA(DisplayName = "CoreMIDI UMP (MIDI 2.0)", ToolTip = "CoreMIDI with Universal MIDI Packet support"),
	JackUMP UMETA(DisplayName = "JACK UMP (MIDI 2.0)", ToolTip = "JACK with Universal MIDI Packet support"),
	PipeWireUMP UMETA(DisplayName = "PipeWire UMP (MIDI 2.0)", ToolTip = "PipeWire with Universal MIDI Packet support")
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EMidiPortType : uint8
{
	Unknown = 0 UMETA(DisplayName = "Unknown", ToolTip = "Unknown port type"),
	Software = 1 UMETA(DisplayName = "Software (Virtual)", ToolTip = "Software virtual port created by applications"),
	Loopback = 2 UMETA(DisplayName = "Loopback", ToolTip = "Loopback port for internal routing"),
	Hardware = 3 UMETA(DisplayName = "Hardware", ToolTip = "Physical hardware MIDI port"),
	USB = 4 UMETA(DisplayName = "USB", ToolTip = "USB MIDI device"),
	Bluetooth = 5 UMETA(DisplayName = "Bluetooth", ToolTip = "Bluetooth MIDI device (BLE MIDI)"),
	PCI = 6 UMETA(DisplayName = "PCI", ToolTip = "PCI/PCIe MIDI device"),
	Network = 7 UMETA(DisplayName = "Network", ToolTip = "Network MIDI (RTP-MIDI, etc.)")
};

UENUM(BlueprintType)
enum class EMidiContainerType : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No container information available"),
	UUID UMETA(DisplayName = "UUID", ToolTip = "Container identified by UUID (e.g., WinMIDI ContainerID GUID)"),
	String UMETA(DisplayName = "String", ToolTip = "Container identified by string (e.g., ALSA device ID)"),
	Integer UMETA(DisplayName = "Integer", ToolTip = "Container identified by integer (e.g., CoreMIDI USBLocationID)")
};

UENUM(BlueprintType)
enum class EMidiDeviceType : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No device information available"),
	String UMETA(DisplayName = "String", ToolTip = "Device identified by string (e.g., WinMIDI EndpointDeviceId, ALSA sysfs path)"),
	Integer UMETA(DisplayName = "Integer", ToolTip = "Device identified by integer (e.g., CoreMIDI USBVendorProduct)")
};

USTRUCT(BlueprintType)
struct LIBREMIDI4UE_API FMidiPortInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Handles", meta = (DisplayName = "Client Handle", ToolTip = "API-specific client handle identifier"))
	int64 ClientHandle = -1;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Handles", meta = (DisplayName = "Port Handle", ToolTip = "API-specific port handle identifier"))
	int64 PortHandle = -1;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container Type", ToolTip = "Type of container identifier"))
	EMidiContainerType ContainerType = EMidiContainerType::None;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container UUID", ToolTip = "Container UUID (WinMIDI ContainerID as hex string)"))
	FString ContainerUUID;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container String", ToolTip = "Container string identifier (e.g., ALSA device ID path)"))
	FString ContainerString;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container Integer", ToolTip = "Container integer identifier (e.g., CoreMIDI USBLocationID)"))
	int64 ContainerInteger = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device", meta = (DisplayName = "Device Type", ToolTip = "Type of device identifier"))
	EMidiDeviceType DeviceType = EMidiDeviceType::None;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device", meta = (DisplayName = "Device String", ToolTip = "Device string identifier (e.g., WinMIDI EndpointDeviceId, ALSA sysfs path)"))
	FString DeviceString;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device", meta = (DisplayName = "Device Integer", ToolTip = "Device integer identifier (e.g., CoreMIDI USBVendorProduct)"))
	int64 DeviceInteger = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Manufacturer", ToolTip = "Device manufacturer name (e.g., Roland, Yamaha)"))
	FString Manufacturer;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Device Name", ToolTip = "Physical device name"))
	FString DeviceName;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Port Name", ToolTip = "Specific port name on the device"))
	FString PortName;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Display Name", ToolTip = "User-friendly display name (recommended for UI)"))
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Port Type", ToolTip = "Type of MIDI port (USB, Bluetooth, etc.)"))
	EMidiPortType PortType = EMidiPortType::Unknown;

	FMidiPortInfo() = default;

	explicit FMidiPortInfo(const libremidi::port_information& port)
		: ClientHandle(static_cast<int64>(port.client))
		, PortHandle(static_cast<int64>(port.port))
		, Manufacturer(UTF8_TO_TCHAR(port.manufacturer.c_str()))
		, DeviceName(UTF8_TO_TCHAR(port.device_name.c_str()))
		, PortName(UTF8_TO_TCHAR(port.port_name.c_str()))
		, DisplayName(UTF8_TO_TCHAR(port.display_name.c_str()))
		, PortType(static_cast<EMidiPortType>(port.type))
	{
		ParseContainerIdentifier(port.container);
		ParseDeviceIdentifier(port.device);
	}

private:
	void ParseContainerIdentifier(const libremidi::container_identifier& Container)
	{
		if (std::holds_alternative<libremidi::uuid>(Container))
		{
			ContainerType = EMidiContainerType::UUID;
			const auto& UUID = std::get<libremidi::uuid>(Container);
			ContainerUUID = BytesToHexString(UUID.bytes.data(), UUID.bytes.size());
		}
		else if (std::holds_alternative<std::string>(Container))
		{
			ContainerType = EMidiContainerType::String;
			ContainerString = UTF8_TO_TCHAR(std::get<std::string>(Container).c_str());
		}
		else if (std::holds_alternative<std::uint64_t>(Container))
		{
			ContainerType = EMidiContainerType::Integer;
			ContainerInteger = static_cast<int64>(std::get<std::uint64_t>(Container));
		}
		else
		{
			ContainerType = EMidiContainerType::None;
		}
	}

	void ParseDeviceIdentifier(const libremidi::device_identifier& Device)
	{
		if (std::holds_alternative<std::string>(Device))
		{
			DeviceType = EMidiDeviceType::String;
			DeviceString = UTF8_TO_TCHAR(std::get<std::string>(Device).c_str());
		}
		else if (std::holds_alternative<std::uint64_t>(Device))
		{
			DeviceType = EMidiDeviceType::Integer;
			DeviceInteger = static_cast<int64>(std::get<std::uint64_t>(Device));
		}
		else
		{
			DeviceType = EMidiDeviceType::None;
		}
	}

	static FString BytesToHexString(const uint8_t* Bytes, size_t Length)
	{
		FString Result;
		Result.Reserve(static_cast<int32>(Length * 2));
		for (size_t i = 0; i < Length; ++i)
		{
			Result += FString::Printf(TEXT("%02X"), Bytes[i]);
		}
		return Result;
	}
};

namespace LibremidiTypeConversion
{
	LIBREMIDI4UE_API libremidi::API ToLibremidiAPI(ELibremidiAPI API);
	LIBREMIDI4UE_API ELibremidiAPI FromLibremidiAPI(libremidi::API API);
}
