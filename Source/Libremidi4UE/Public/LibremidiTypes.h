#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

#include "LibremidiTypes.generated.h"

// Forward declarations
class ULibremidiEngineSubsystem;

/**
 * MIDI API backend selection
 */
UENUM(BlueprintType)
enum class ELibremidiAPI : uint8
{
	Unspecified UMETA(DisplayName = "Auto (Unspecified)", ToolTip = "Automatically select the best available API. Prefers MIDI 2.0 (UMP) APIs when available. Legacy APIs will automatically convert between MIDI 1.0 and UMP formats."),
	Dummy UMETA(DisplayName = "Dummy (Testing)", ToolTip = "Dummy API for testing without actual MIDI hardware"),
	AlsaSeq UMETA(DisplayName = "ALSA Sequencer (Linux)", ToolTip = "ALSA Sequencer API for Linux. MIDI 1.0 format with automatic UMP conversion."),
	AlsaRaw UMETA(DisplayName = "ALSA Raw (Linux)", ToolTip = "ALSA Raw MIDI API for Linux. MIDI 1.0 format with automatic UMP conversion."),
	CoreMidi UMETA(DisplayName = "CoreMIDI (macOS/iOS)", ToolTip = "CoreMIDI API for macOS and iOS. MIDI 1.0 format with automatic UMP conversion."),
	WindowsMM UMETA(DisplayName = "Windows Multimedia (Legacy)", ToolTip = "Legacy Windows Multimedia API. MIDI 1.0 format with automatic UMP conversion."),
	WindowsUWP UMETA(DisplayName = "Windows UWP", ToolTip = "Universal Windows Platform MIDI API. MIDI 1.0 format with automatic UMP conversion."),
	Jack UMETA(DisplayName = "JACK Audio", ToolTip = "JACK Audio Connection Kit. MIDI 1.0 format with automatic UMP conversion."),
	PipeWire UMETA(DisplayName = "PipeWire (Linux)", ToolTip = "PipeWire multimedia framework. MIDI 1.0 format with automatic UMP conversion."),
	Emscripten UMETA(DisplayName = "Web MIDI (Emscripten)", ToolTip = "Web MIDI API for browsers"),
	WindowsMidiServices UMETA(DisplayName = "Windows MIDI Services (MIDI 2.0)", ToolTip = "Modern Windows MIDI Services with native MIDI 2.0 support. Requires Windows 11+."),
	AlsaSeqUMP UMETA(DisplayName = "ALSA Sequencer UMP (MIDI 2.0)", ToolTip = "ALSA Sequencer with native Universal MIDI Packet support"),
	AlsaRawUMP UMETA(DisplayName = "ALSA Raw UMP (MIDI 2.0)", ToolTip = "ALSA Raw with native Universal MIDI Packet support"),
	CoreMidiUMP UMETA(DisplayName = "CoreMIDI UMP (MIDI 2.0)", ToolTip = "CoreMIDI with native Universal MIDI Packet support. Requires macOS 11+."),
	JackUMP UMETA(DisplayName = "JACK UMP (MIDI 2.0)", ToolTip = "JACK with native Universal MIDI Packet support. Requires PipeWire v1.4+."),
	PipeWireUMP UMETA(DisplayName = "PipeWire UMP (MIDI 2.0)", ToolTip = "PipeWire with native Universal MIDI Packet support. Requires v1.4+.")
};

/**
 * MIDI port type flags
 */
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

/**
 * Container identifier type
 */
UENUM(BlueprintType)
enum class ELibremidiContainerType : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No container information available"),
	UUID UMETA(DisplayName = "UUID", ToolTip = "Container identified by UUID (e.g., WinMIDI ContainerID GUID)"),
	String UMETA(DisplayName = "String", ToolTip = "Container identified by string (e.g., ALSA device ID)"),
	Integer UMETA(DisplayName = "Integer", ToolTip = "Container identified by integer (e.g., CoreMIDI USBLocationID)")
};

/**
 * Device identifier type
 */
UENUM(BlueprintType)
enum class ELibremidiDeviceType : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No device information available"),
	String UMETA(DisplayName = "String", ToolTip = "Device identified by string (e.g., WinMIDI EndpointDeviceId, ALSA sysfs path)"),
	Integer UMETA(DisplayName = "Integer", ToolTip = "Device identified by integer (e.g., CoreMIDI USBVendorProduct)")
};

/**
 * MIDI message type classification
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
 * Common MIDI Control Change numbers (MIDI CC)
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

/**
 * MIDI port information structure
 * Contains all metadata about a MIDI port including hardware identifiers and descriptive names
 */
USTRUCT(BlueprintType)
struct LIBREMIDI4UE_API FLibremidiPortInfo
{
	GENERATED_BODY()

	/** API-specific client handle identifier */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Handles", meta = (DisplayName = "Client Handle"))
	int64 ClientHandle = -1;

	/** API-specific port handle identifier */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Handles", meta = (DisplayName = "Port Handle"))
	int64 PortHandle = -1;

	/** Type of container identifier */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container Type"))
	ELibremidiContainerType ContainerType = ELibremidiContainerType::None;

	/** Container UUID (WinMIDI ContainerID as hex string) */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container UUID"))
	FString ContainerUUID;

	/** Container string identifier (e.g., ALSA device ID) */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container String"))
	FString ContainerString;

	/** Container integer identifier (e.g., CoreMIDI USBLocationID) */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container", meta = (DisplayName = "Container Integer"))
	int64 ContainerInteger = 0;

	/** Type of device identifier */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device", meta = (DisplayName = "Device Type"))
	ELibremidiDeviceType DeviceType = ELibremidiDeviceType::None;

	/** Device string identifier (e.g., WinMIDI EndpointDeviceId, ALSA sysfs path) */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device", meta = (DisplayName = "Device String"))
	FString DeviceString;

	/** Device integer identifier (e.g., CoreMIDI USBVendorProduct) */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device", meta = (DisplayName = "Device Integer"))
	int64 DeviceInteger = 0;

	/** Device manufacturer name (e.g., Roland, Yamaha) */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Manufacturer"))
	FString Manufacturer;

	/** Physical device name */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Device Name"))
	FString DeviceName;

	/** Specific port name on the device */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Port Name"))
	FString PortName;

	/** User-friendly display name (recommended for UI) */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Display Name"))
	FString DisplayName;

	/** Type of MIDI port (USB, Bluetooth, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Info", meta = (DisplayName = "Port Type"))
	ELibremidiPortType PortType = ELibremidiPortType::Unknown;

	/** Default constructor */
	FLibremidiPortInfo() = default;

	/** Construct from libremidi port_information */
	explicit FLibremidiPortInfo(const libremidi::port_information& InPort);

	/** Equality operator - compares by handle */
	bool operator==(const FLibremidiPortInfo& Other) const
	{
		return ClientHandle == Other.ClientHandle && PortHandle == Other.PortHandle;
	}

	/** Inequality operator */
	bool operator!=(const FLibremidiPortInfo& Other) const
	{
		return !(*this == Other);
	}

	/** Check if port info is valid */
	bool IsValid() const
	{
		return ClientHandle != -1 && PortHandle != -1;
	}

private:
	void ParseContainerIdentifier(const libremidi::container_identifier& Container);
	void ParseDeviceIdentifier(const libremidi::device_identifier& Device);
	static FString BytesToHexString(const uint8_t* Bytes, size_t Length);
};

/**
 * API conversion utilities
 */
namespace LibremidiTypeConversion
{
	/** Convert UE enum to libremidi API */
	LIBREMIDI4UE_API libremidi::API ToLibremidiAPI(ELibremidiAPI API);
	
	/** Convert libremidi API to UE enum */
	LIBREMIDI4UE_API ELibremidiAPI FromLibremidiAPI(libremidi::API API);
}

/**
 * MIDI constants following MIDI 1.0 specification
 */
namespace FLibremidiConstants
{
	// MIDI value ranges
	static constexpr int32 MinChannel = 0;
	static constexpr int32 MaxChannel = 15;
	static constexpr int32 MinNote = 0;
	static constexpr int32 MaxNote = 127;
	static constexpr int32 MinController = 0;
	static constexpr int32 MaxController = 127;
	static constexpr int32 MinProgram = 0;
	static constexpr int32 MaxProgram = 127;
	static constexpr int32 MinData7Bit = 0;
	static constexpr int32 MaxData7Bit = 127;

	// MIDI status bytes (channel voice messages)
	static constexpr uint8 NoteOff = 0x80;
	static constexpr uint8 NoteOn = 0x90;
	static constexpr uint8 PolyPressure = 0xA0;
	static constexpr uint8 ControlChange = 0xB0;
	static constexpr uint8 ProgramChange = 0xC0;
	static constexpr uint8 ChannelPressure = 0xD0;
	static constexpr uint8 PitchBend = 0xE0;
	
	// MIDI status bytes (system common messages)
	static constexpr uint8 SongPosition = 0xF2;
	static constexpr uint8 SongSelect = 0xF3;
	
	// MIDI status bytes (system real-time messages)
	static constexpr uint8 Clock = 0xF8;
	static constexpr uint8 Start = 0xFA;
	static constexpr uint8 Continue = 0xFB;
	static constexpr uint8 Stop = 0xFC;
	
	// System Exclusive
	static constexpr uint8 SysExStart = 0xF0;
	static constexpr uint8 SysExEnd = 0xF7;
}

/**
 * MIDI value clamping utilities
 */
struct LIBREMIDI4UE_API FLibremidiUtils
{
	/** Clamp MIDI channel to valid range (0-15) */
	static FORCEINLINE int32 ClampChannel(int32 Channel)
	{
		return FMath::Clamp(Channel, FLibremidiConstants::MinChannel, FLibremidiConstants::MaxChannel);
	}

	/** Clamp MIDI note number to valid range (0-127) */
	static FORCEINLINE int32 ClampNote(int32 Note)
	{
		return FMath::Clamp(Note, FLibremidiConstants::MinNote, FLibremidiConstants::MaxNote);
	}

	/** Clamp MIDI controller number to valid range (0-127) */
	static FORCEINLINE int32 ClampController(int32 Controller)
	{
		return FMath::Clamp(Controller, FLibremidiConstants::MinController, FLibremidiConstants::MaxController);
	}

	/** Clamp MIDI program number to valid range (0-127) */
	static FORCEINLINE int32 ClampProgram(int32 Program)
	{
		return FMath::Clamp(Program, FLibremidiConstants::MinProgram, FLibremidiConstants::MaxProgram);
	}

	/** Clamp 7-bit MIDI data to valid range (0-127) */
	static FORCEINLINE int32 ClampData7Bit(int32 Data)
	{
		return FMath::Clamp(Data, FLibremidiConstants::MinData7Bit, FLibremidiConstants::MaxData7Bit);
	}
};
