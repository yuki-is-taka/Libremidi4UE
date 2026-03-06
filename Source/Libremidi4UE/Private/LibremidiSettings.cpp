// Copyright Epic Games, Inc. All Rights Reserved.

#include "LibremidiSettings.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

ULibremidiSettings::ULibremidiSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BackendAPIName = TEXT("Auto");
	bTrackHardware = true;
	bTrackVirtual = true;
	bTrackAny = false;
}

FName ULibremidiSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
void ULibremidiSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NormalizeBackendAPIName();
	SettingChangedDelegate.Broadcast(this, PropertyChangedEvent);
}
#endif

libremidi::API ULibremidiSettings::GetResolvedBackendAPI() const
{
	const bool bUseMidi2 = BackendProtocol == ELibremidiMidiProtocol::Midi2;
	const libremidi::API DefaultApi = bUseMidi2 ? libremidi::midi2::default_api() : libremidi::midi1::default_api();

	if (BackendAPIName.IsEmpty() || BackendAPIName.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
	{
		return DefaultApi;
	}

	const std::vector<libremidi::API> Apis = bUseMidi2 ? libremidi::available_ump_apis() : libremidi::available_apis();
	for (const libremidi::API Api : Apis)
	{
		const std::string_view DisplayName = libremidi::get_api_display_name(Api);
		if (BackendAPIName.Equals(UTF8_TO_TCHAR(DisplayName.data()), ESearchCase::IgnoreCase))
		{
			return Api;
		}
	}

	return DefaultApi;
}

#if WITH_EDITOR
TArray<FString> ULibremidiSettings::GetAvailableBackendAPIOptions() const
{
	TArray<FString> Options;
	Options.Add(TEXT("Auto"));

	const bool bUseMidi2 = BackendProtocol == ELibremidiMidiProtocol::Midi2;
	const std::vector<libremidi::API> Apis = bUseMidi2 ? libremidi::available_ump_apis() : libremidi::available_apis();

	for (const libremidi::API Api : Apis)
	{
		const std::string_view DisplayName = libremidi::get_api_display_name(Api);
		Options.Add(UTF8_TO_TCHAR(DisplayName.data()));
	}

	return Options;
}
#endif

void ULibremidiSettings::NormalizeBackendAPIName()
{
	const bool bUseMidi2 = BackendProtocol == ELibremidiMidiProtocol::Midi2;
	const std::vector<libremidi::API> Apis = bUseMidi2 ? libremidi::available_ump_apis() : libremidi::available_apis();

	if (BackendAPIName.IsEmpty() || BackendAPIName.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
	{
		BackendAPIName = TEXT("Auto");
		return;
	}

	for (const libremidi::API Api : Apis)
	{
		const std::string_view DisplayName = libremidi::get_api_display_name(Api);
		if (BackendAPIName.Equals(UTF8_TO_TCHAR(DisplayName.data()), ESearchCase::IgnoreCase))
		{
			return;
		}
	}

	BackendAPIName = TEXT("Auto");
}
