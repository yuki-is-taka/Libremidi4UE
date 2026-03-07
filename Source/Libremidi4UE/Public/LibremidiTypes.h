// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/observer_configuration.hpp>
THIRD_PARTY_INCLUDES_END

#include "LibremidiTypes.generated.h"

UENUM(BlueprintType)
enum class ELibremidiContainerIdentifierType : uint8
{
	None UMETA(DisplayName = "None"),
	UUID UMETA(DisplayName = "UUID"),
	String UMETA(DisplayName = "String"),
	Integer UMETA(DisplayName = "Integer")
};

UENUM(BlueprintType)
enum class ELibremidiDeviceIdentifierType : uint8
{
	None UMETA(DisplayName = "None"),
	String UMETA(DisplayName = "String"),
	Integer UMETA(DisplayName = "Integer")
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ELibremidiPortType : uint8
{
	Unknown = 0 UMETA(DisplayName = "Unknown"),
	Software = (1 << 1) UMETA(DisplayName = "Software"),
	Loopback = (1 << 2) UMETA(DisplayName = "Loopback"),
	Hardware = (1 << 3) UMETA(DisplayName = "Hardware"),
	USB = (1 << 4) UMETA(DisplayName = "USB"),
	Bluetooth = (1 << 5) UMETA(DisplayName = "Bluetooth"),
	PCI = (1 << 6) UMETA(DisplayName = "PCI"),
	Network = (1 << 7) UMETA(DisplayName = "Network")
};

UENUM(BlueprintType)
enum class ELibremidiTimestampMode : uint8
{
	NoTimestamp UMETA(DisplayName = "No Timestamp", ToolTip = "No timestamping; all events report timestamp 0."),
	Relative UMETA(DisplayName = "Relative", ToolTip = "Timestamp is nanoseconds since the previous event (or zero)."),
	Absolute UMETA(DisplayName = "Absolute", ToolTip = "Timestamp is backend-provided absolute time in nanoseconds; maps to SystemMonotonic when unavailable."),
	SystemMonotonic UMETA(DisplayName = "System Monotonic", ToolTip = "Timestamp is std::steady_clock::now() in nanoseconds; library-generated."),
	AudioFrame UMETA(DisplayName = "Audio Frame", ToolTip = "Timestamp is audio-frame offset within the current process cycle (e.g., JACK)."),
	Custom UMETA(DisplayName = "Custom", ToolTip = "Timestamp is provided by a user callback (GetTimestamp).")
};


// ---------------------------------------------------------------------------
// Shared delegate: error / warning diagnostic (Input and Output)
// ---------------------------------------------------------------------------

/** Fired on error or warning from libremidi. Shared by Input and Output. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLibremidiDiagnostic, UObject* /*Source*/, const FString& /*Text*/);

USTRUCT(BlueprintType)
struct LIBREMIDI4UE_API FLibremidiContainerIdentifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container")
	ELibremidiContainerIdentifierType Type = ELibremidiContainerIdentifierType::None;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container")
	TArray<uint8> UUIDBytes;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container")
	FString String;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Container")
	int64 Integer = 0;

	static FLibremidiContainerIdentifier FromLibremidi(const libremidi::container_identifier& Identifier)
	{
		FLibremidiContainerIdentifier Result;
		if (std::holds_alternative<libremidi::uuid>(Identifier))
		{
			Result.Type = ELibremidiContainerIdentifierType::UUID;
			const auto& Value = std::get<libremidi::uuid>(Identifier);
			Result.UUIDBytes.SetNum(Value.bytes.size());
			FMemory::Memcpy(Result.UUIDBytes.GetData(), Value.bytes.data(), Value.bytes.size());
		}
		else if (std::holds_alternative<std::string>(Identifier))
		{
			Result.Type = ELibremidiContainerIdentifierType::String;
			Result.String = UTF8_TO_TCHAR(std::get<std::string>(Identifier).c_str());
		}
		else if (std::holds_alternative<std::uint64_t>(Identifier))
		{
			Result.Type = ELibremidiContainerIdentifierType::Integer;
			Result.Integer = static_cast<int64>(std::get<std::uint64_t>(Identifier));
		}
		return Result;
	}

	libremidi::container_identifier ToLibremidi() const
	{
		switch (Type)
		{
		case ELibremidiContainerIdentifierType::UUID:
			if (UUIDBytes.Num() == 16)
			{
				libremidi::uuid Value{};
				FMemory::Memcpy(Value.bytes.data(), UUIDBytes.GetData(), Value.bytes.size());
				return Value;
			}
			return std::monostate{};
		case ELibremidiContainerIdentifierType::String:
			return std::string(TCHAR_TO_UTF8(*String));
		case ELibremidiContainerIdentifierType::Integer:
			return static_cast<std::uint64_t>(Integer);
		default:
			return std::monostate{};
		}
	}
};

USTRUCT(BlueprintType)
struct LIBREMIDI4UE_API FLibremidiDeviceIdentifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device")
	ELibremidiDeviceIdentifierType Type = ELibremidiDeviceIdentifierType::None;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device")
	FString String;

	UPROPERTY(BlueprintReadOnly, Category = "MIDI|Device")
	int64 Integer = 0;

	static FLibremidiDeviceIdentifier FromLibremidi(const libremidi::device_identifier& Identifier)
	{
		FLibremidiDeviceIdentifier Result;
		if (std::holds_alternative<std::string>(Identifier))
		{
			Result.Type = ELibremidiDeviceIdentifierType::String;
			Result.String = UTF8_TO_TCHAR(std::get<std::string>(Identifier).c_str());
		}
		else if (std::holds_alternative<std::uint64_t>(Identifier))
		{
			Result.Type = ELibremidiDeviceIdentifierType::Integer;
			Result.Integer = static_cast<int64>(std::get<std::uint64_t>(Identifier));
		}
		return Result;
	}

	libremidi::device_identifier ToLibremidi() const
	{
		switch (Type)
		{
		case ELibremidiDeviceIdentifierType::String:
			return std::string(TCHAR_TO_UTF8(*String));
		case ELibremidiDeviceIdentifierType::Integer:
			return static_cast<std::uint64_t>(Integer);
		default:
			return std::monostate{};
		}
	}
};

USTRUCT(BlueprintType)
struct LIBREMIDI4UE_API FLibremidiPortInfo
{
	GENERATED_BODY()

	FLibremidiPortInfo() = default;

	explicit FLibremidiPortInfo(const libremidi::port_information& InPort)
		: Port(InPort)
	{
	}

	explicit FLibremidiPortInfo(libremidi::port_information&& InPort)
		: Port(MoveTemp(InPort))
	{
	}

	const libremidi::port_information& GetPort() const
	{
		return Port;
	}

	libremidi::port_information& GetPort()
	{
		return Port;
	}

	uint64 GetClientHandle() const
	{
		return Port.client;
	}

	uint64 GetPortHandle() const
	{
		return Port.port;
	}

	FLibremidiContainerIdentifier GetContainerIdentifier() const
	{
		return FLibremidiContainerIdentifier::FromLibremidi(Port.container);
	}

	FLibremidiDeviceIdentifier GetDeviceIdentifier() const
	{
		return FLibremidiDeviceIdentifier::FromLibremidi(Port.device);
	}

	FString GetManufacturer() const
	{
		return UTF8_TO_TCHAR(Port.manufacturer.c_str());
	}

	FString GetDeviceName() const
	{
		return UTF8_TO_TCHAR(Port.device_name.c_str());
	}

	FString GetPortName() const
	{
		return UTF8_TO_TCHAR(Port.port_name.c_str());
	}

	FString GetDisplayName() const
	{
		return UTF8_TO_TCHAR(Port.display_name.c_str());
	}

	ELibremidiPortType GetPortType() const
	{
		return static_cast<ELibremidiPortType>(Port.type);
	}

	/**
	 * Returns a deterministic per-device port ordinal for stable merge ordering.
	 * Uses API-specific logic (CoreMIDI entity index, WinMIDI terminal block number,
	 * ALSA packed sub/port, etc.).
	 * CoreMIDI: direction is self-determined via MIDIObjectType from reverse-lookup.
	 */
	int32 GetOrdinal() const;

private:
	libremidi::port_information Port;
};

USTRUCT(BlueprintType)
struct LIBREMIDI4UE_API FLibremidiInputInfo : public FLibremidiPortInfo
{
	GENERATED_BODY()

	FLibremidiInputInfo() = default;

	explicit FLibremidiInputInfo(const libremidi::input_port& InPort)
		: FLibremidiPortInfo(InPort)
	{
	}

	explicit FLibremidiInputInfo(libremidi::input_port&& InPort)
		: FLibremidiPortInfo(MoveTemp(InPort))
	{
	}
};

USTRUCT(BlueprintType)
struct LIBREMIDI4UE_API FLibremidiOutputInfo : public FLibremidiPortInfo
{
	GENERATED_BODY()

	FLibremidiOutputInfo() = default;

	explicit FLibremidiOutputInfo(const libremidi::output_port& InPort)
		: FLibremidiPortInfo(InPort)
	{
	}

	explicit FLibremidiOutputInfo(libremidi::output_port&& InPort)
		: FLibremidiPortInfo(MoveTemp(InPort))
	{
	}
};
