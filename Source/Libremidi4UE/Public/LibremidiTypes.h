// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/observer_configuration.hpp>
#include <libremidi/port_comparison.hpp>
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
	Integer UMETA(DisplayName = "Integer"),
	USBDevice UMETA(DisplayName = "USB Device")
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Container")
	ELibremidiContainerIdentifierType Type = ELibremidiContainerIdentifierType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Container")
	TArray<uint8> UUIDBytes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Container")
	FString String;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Container")
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Device")
	ELibremidiDeviceIdentifierType Type = ELibremidiDeviceIdentifierType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Device")
	FString String;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Device")
	int64 Integer = 0;

	/** USB vendor/product IDs (valid when Type == USBDevice). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Device")
	int32 USBVendorId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Device")
	int32 USBProductId = 0;

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
		else if (std::holds_alternative<libremidi::usb_device_identifier>(Identifier))
		{
			Result.Type = ELibremidiDeviceIdentifierType::USBDevice;
			const auto& USB = std::get<libremidi::usb_device_identifier>(Identifier);
			Result.USBVendorId = USB.vendor_id;
			Result.USBProductId = USB.product_id;
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
		case ELibremidiDeviceIdentifierType::USBDevice:
			return libremidi::usb_device_identifier{
				static_cast<uint16_t>(USBVendorId),
				static_cast<uint16_t>(USBProductId)
			};
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

	explicit FLibremidiPortInfo(const libremidi::port_information& InPort);
	explicit FLibremidiPortInfo(libremidi::port_information&& InPort);

	// ===== Serializable fields (UPROPERTY) =====

	/** Backend API that produced this port (stored as int for serialization; cast to/from libremidi::API). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	int32 Api = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	FLibremidiContainerIdentifier ContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	FLibremidiDeviceIdentifier DeviceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	int64 PortHandle = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	int64 ClientHandle = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	FString Manufacturer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	FString Product;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	FString Serial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	FString DeviceName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	FString PortName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	ELibremidiPortType PortType = ELibremidiPortType::Unknown;

	/** Cached ordinal computed at construction from API-specific logic. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MIDI|Port")
	int32 Ordinal = 0;

	// ===== Native access =====

	/** Returns the cached native port_information. Only fully valid when constructed from a live port. */
	const libremidi::port_information& GetPort() const { return CachedNative; }
	libremidi::port_information& GetPort() { return CachedNative; }

	/** Reconstructs a native port_information from the serialized UPROPERTY fields. */
	libremidi::port_information ToPortInformation() const;

	// ===== Convenience getters =====

	uint64 GetClientHandle() const { return static_cast<uint64>(ClientHandle); }
	uint64 GetPortHandle() const { return static_cast<uint64>(PortHandle); }
	FLibremidiContainerIdentifier GetContainerIdentifier() const { return ContainerId; }
	FLibremidiDeviceIdentifier GetDeviceIdentifier() const { return DeviceId; }
	FString GetManufacturer() const { return Manufacturer; }
	FString GetDeviceName() const { return DeviceName; }
	FString GetPortName() const { return PortName; }
	FString GetDisplayName() const { return DisplayName; }
	ELibremidiPortType GetPortType() const { return PortType; }

	/**
	 * Returns a deterministic per-device port ordinal for stable merge ordering.
	 * When constructed from a live port, this is computed via API-specific logic
	 * (CoreMIDI entity index, WinMIDI terminal block number, ALSA packed sub/port, etc.).
	 * When deserialized, returns the cached value.
	 */
	int32 GetOrdinal() const { return Ordinal; }

	// ===== Port matching =====

	/** Result of FindClosestPort(). */
	struct FMatchResult
	{
		/** Index into the candidates array, or INDEX_NONE if not found. */
		int32 Index = INDEX_NONE;

		/** Heuristic match score (higher = better). Only meaningful when Index != INDEX_NONE. */
		int32 Score = 0;

		bool IsValid() const { return Index != INDEX_NONE; }
	};

	/**
	 * Finds the candidate port that most closely matches this port info,
	 * using libremidi's heuristic matcher (hardware IDs, serial, names, handles).
	 * Works with both live ports and deserialized port info.
	 *
	 * @param Candidates  Array of ports to search through.
	 * @return  Match result with index into Candidates and score, or INDEX_NONE if no match.
	 */
	FMatchResult FindClosestPort(TArrayView<const FLibremidiPortInfo> Candidates) const;

	/** Convenience overload for FLibremidiInputInfo arrays. */
	FMatchResult FindClosestPort(TArrayView<const FLibremidiInputInfo> Candidates) const;

	/** Convenience overload for FLibremidiOutputInfo arrays. */
	FMatchResult FindClosestPort(TArrayView<const FLibremidiOutputInfo> Candidates) const;

private:
	/** Native port_information. Populated by constructors; empty after deserialization. */
	libremidi::port_information CachedNative;

	/** Populate UPROPERTY fields from CachedNative. */
	void SyncPropertiesFromNative();

	/** Compute ordinal from native port_information using API-specific logic. */
	static int32 ComputeOrdinalFromNative(const libremidi::port_information& Port);
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
