// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#pragma once

#include "Engine/DeveloperSettings.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/api.hpp>
THIRD_PARTY_INCLUDES_END
#include "LibremidiSettings.generated.h"

UENUM(BlueprintType)
enum class ELibremidiMidiProtocol : uint8
{
	Midi1 UMETA(DisplayName = "MIDI 1.0"),
	Midi2 UMETA(DisplayName = "MIDI 2.0")
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Libremidi MIDI 2"))
class LIBREMIDI4UE_API ULibremidiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULibremidiSettings(const FObjectInitializer& ObjectInitializer);

	virtual FName GetCategoryName() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSettingChanged, UObject*, struct FPropertyChangedEvent&);
	FOnSettingChanged& OnSettingChanged() { return SettingChangedDelegate; }
#endif

	libremidi::API GetResolvedBackendAPI() const;

#if WITH_EDITOR
	UFUNCTION()
	TArray<FString> GetAvailableBackendAPIOptions() const;
#endif

	UPROPERTY(Config, EditAnywhere, Category = "Backend", meta = (
		DisplayName = "MIDI Protocol",
		ToolTip = "Select whether to use MIDI 1.0 or MIDI 2.0 APIs."))
	ELibremidiMidiProtocol BackendProtocol = ELibremidiMidiProtocol::Midi2;

	UPROPERTY(Config, EditAnywhere, Category = "Backend", meta = (
		DisplayName = "Backend API",
		GetOptions = "GetAvailableBackendAPIOptions",
		ToolTip = "Backend API selection (string name)."))
	FString BackendAPIName;

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (
		DisplayName = "Track Hardware Devices",
		ToolTip = "Detect physical MIDI devices (USB, Bluetooth, PCI, etc.)."))
	bool bTrackHardware;

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (
		DisplayName = "Track Virtual Devices",
		ToolTip = "Detect virtual/software MIDI ports created by applications."))
	bool bTrackVirtual;

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (
		DisplayName = "Track Any Devices",
		ToolTip = "Detect all port types, including unknown or backend-specific ones."))
	bool bTrackAny;

private:
	void NormalizeBackendAPIName();

#if WITH_EDITOR
	FOnSettingChanged SettingChangedDelegate;
#endif
};
