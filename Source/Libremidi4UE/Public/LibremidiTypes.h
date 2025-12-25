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
enum class ELibremidiPortType : uint8
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
enum class ELibremidiContainerType : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No container information available"),
	UUID UMETA(DisplayName = "UUID", ToolTip = "Container identified by UUID (e.g., WinMIDI ContainerID GUID)"),
	String UMETA(DisplayName = "String", ToolTip = "Container identified by string (e.g., ALSA device ID)"),
	Integer UMETA(DisplayName = "Integer", ToolTip = "Container identified by integer (e.g., CoreMIDI USBLocationID)")
};

UENUM(BlueprintType)
enum class ELibremidiDeviceType : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No device information available"),
	String UMETA(DisplayName = "String", ToolTip = "Device identified by string (e.g., WinMIDI EndpointDeviceId, ALSA sysfs path)"),
	Integer UMETA(DisplayName = "Integer", ToolTip = "Device identified by integer (e.g., CoreMIDI USBVendorProduct)")
};

/**
 * MIDI Message Type enum
 */
UENUM(BlueprintType)
enum class ELibremidiMessageType : uint8
{
	NoteOff UMETA(DisplayName = "Note Off"),
	NoteOn UMETA(DisplayName = "Note On"),
	PolyPressure UMETA(DisplayName = "Poly Pressure (Aftertouch)"),
	ControlChange UMETA(DisplayName = "Control Change"),
	ProgramChange UMETA(DisplayName = "Program Change"),
	ChannelPressure UMETA(DisplayName = "Channel Pressure (Aftertouch)"),
	PitchBend UMETA(DisplayName = "Pitch Bend"),
	SystemExclusive UMETA(DisplayName = "System Exclusive (SysEx)"),
	SystemCommon UMETA(DisplayName = "System Common"),
	SystemRealTime UMETA(DisplayName = "System Real-Time"),
	Unknown UMETA(DisplayName = "Unknown")
};

/**
 * Common MIDI Control Change numbers
 */
UENUM(BlueprintType)
enum class ELibremidiControlChange : uint8
{
	BankSelect = 0 UMETA(DisplayName = "Bank Select"),
	ModulationWheel = 1 UMETA(DisplayName = "Modulation Wheel"),
	BreathController = 2 UMETA(DisplayName = "Breath Controller"),
	FootController = 4 UMETA(DisplayName = "Foot Controller"),
	PortamentoTime = 5 UMETA(DisplayName = "Portamento Time"),
	DataEntry = 6 UMETA(DisplayName = "Data Entry (MSB)"),
	Volume = 7 UMETA(DisplayName = "Volume"),
	Balance = 8 UMETA(DisplayName = "Balance"),
	Pan = 10 UMETA(DisplayName = "Pan"),
	Expression = 11 UMETA(DisplayName = "Expression"),
	EffectControl1 = 12 UMETA(DisplayName = "Effect Control 1"),
	EffectControl2 = 13 UMETA(DisplayName = "Effect Control 2"),
	GeneralPurpose1 = 16 UMETA(DisplayName = "General Purpose 1"),
	GeneralPurpose2 = 17 UMETA(DisplayName = "General Purpose 2"),
	GeneralPurpose3 = 18 UMETA(DisplayName = "General Purpose 3"),
	GeneralPurpose4 = 19 UMETA(DisplayName = "General Purpose 4"),
	Sustain = 64 UMETA(DisplayName = "Sustain Pedal"),
	Portamento = 65 UMETA(DisplayName = "Portamento On/Off"),
	Sostenuto = 66 UMETA(DisplayName = "Sostenuto Pedal"),
	SoftPedal = 67 UMETA(DisplayName = "Soft Pedal"),
	Legato = 68 UMETA(DisplayName = "Legato Footswitch"),
	Hold2 = 69 UMETA(DisplayName = "Hold 2"),
	SoundVariation = 70 UMETA(DisplayName = "Sound Variation"),
	Timbre = 71 UMETA(DisplayName = "Timbre/Harmonic Intensity"),
	ReleaseTime = 72 UMETA(DisplayName = "Release Time"),
	AttackTime = 73 UMETA(DisplayName = "Attack Time"),
	Brightness = 74 UMETA(DisplayName = "Brightness"),
	DecayTime = 75 UMETA(DisplayName = "Decay Time"),
	VibratoRate = 76 UMETA(DisplayName = "Vibrato Rate"),
	VibratoDepth = 77 UMETA(DisplayName = "Vibrato Depth"),
	VibratoDelay = 78 UMETA(DisplayName = "Vibrato Delay"),
	ReverbLevel = 91 UMETA(DisplayName = "Reverb Level"),
	TremoloLevel = 92 UMETA(DisplayName = "Tremolo Level"),
	ChorusLevel = 93 UMETA(DisplayName = "Chorus Level"),
	CelesteLevel = 94 UMETA(DisplayName = "Celeste (Detune) Level"),
	PhaserLevel = 95 UMETA(DisplayName = "Phaser Level"),
	DataIncrement = 96 UMETA(DisplayName = "Data Increment"),
	DataDecrement = 97 UMETA(DisplayName = "Data Decrement"),
	AllSoundOff = 120 UMETA(DisplayName = "All Sound Off"),
	ResetAllControllers = 121 UMETA(DisplayName = "Reset All Controllers"),
	LocalControl = 122 UMETA(DisplayName = "Local Control On/Off"),
	AllNotesOff = 123 UMETA(DisplayName = "All Notes Off"),
	OmniModeOff = 124 UMETA(DisplayName = "Omni Mode Off"),
	OmniModeOn = 125 UMETA(DisplayName = "Omni Mode On"),
	MonoModeOn = 126 UMETA(DisplayName = "Mono Mode On"),
	PolyModeOn = 127 UMETA(DisplayName = "Poly Mode On")
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
	ELibremidiContainerType ContainerType = ELibremidiContainerType::None;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container UUID", ToolTip = "Container UUID (WinMIDI ContainerID as hex string)"))
	FString ContainerUUID;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container String", ToolTip = "Container string identifier (e.g., ALSA device ID)"))
	FString ContainerString;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container Integer", ToolTip = "Container integer identifier (e.g., CoreMIDI USBLocationID)"))
	int64 ContainerInteger = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device", meta = (DisplayName = "Device Type", ToolTip = "Type of device identifier"))
	ELibremidiDeviceType DeviceType = ELibremidiDeviceType::None;

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
	ELibremidiPortType PortType = ELibremidiPortType::Unknown;

	FMidiPortInfo() = default;

	explicit FMidiPortInfo(const libremidi::port_information& port)
		: ClientHandle(static_cast<int64>(port.client))
		, PortHandle(static_cast<int64>(port.port))
		, Manufacturer(UTF8_TO_TCHAR(port.manufacturer.c_str()))
		, DeviceName(UTF8_TO_TCHAR(port.device_name.c_str()))
		, PortName(UTF8_TO_TCHAR(port.port_name.c_str()))
		, DisplayName(UTF8_TO_TCHAR(port.display_name.c_str()))
		, PortType(static_cast<ELibremidiPortType>(port.type))
	{
		ParseContainerIdentifier(port.container);
		ParseDeviceIdentifier(port.device);
	}

	bool operator==(const FMidiPortInfo& Other) const
	{
		return ClientHandle == Other.ClientHandle && PortHandle == Other.PortHandle;
	}

	bool operator!=(const FMidiPortInfo& Other) const
	{
		return !(*this == Other);
	}

	bool IsValid() const
	{
		return ClientHandle != -1 && PortHandle != -1;
	}

private:
	void ParseContainerIdentifier(const libremidi::container_identifier& Container)
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

	void ParseDeviceIdentifier(const libremidi::device_identifier& Device)
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
