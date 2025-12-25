#include "LibremidiSettings.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

ULibremidiSettings::ULibremidiSettings()
	: BackendAPI(GetPlatformDefaultAPI())
	, bTrackHardware(true)
	, bTrackVirtual(true)
	, bTrackAny(false)
{
}

FName ULibremidiSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

ELibremidiAPI ULibremidiSettings::GetPlatformDefaultAPI()
{
#if PLATFORM_WINDOWS
	// Check if Windows MIDI Services (MIDI 2.0) is available, fallback to WinMM
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == libremidi::API::WINDOWS_MIDI_SERVICES)
		{
			return ELibremidiAPI::WindowsMidiServices;
		}
	}
	return ELibremidiAPI::WindowsMM;
#elif PLATFORM_MAC
	// Check if CoreMIDI UMP (MIDI 2.0) is available
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == libremidi::API::COREMIDI_UMP)
		{
			return EMidiAPI::CoreMidiUMP;
		}
	}
	return EMidiAPI::CoreMidi;
#elif PLATFORM_LINUX
	// Check if ALSA Sequencer UMP (MIDI 2.0) is available
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == libremidi::API::ALSA_SEQ_UMP)
		{
			return EMidiAPI::AlsaSeqUMP;
		}
	}
	return EMidiAPI::AlsaSeq;
#elif PLATFORM_IOS
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == libremidi::API::COREMIDI_UMP)
		{
			return EMidiAPI::CoreMidiUMP;
		}
	}
	return EMidiAPI::CoreMidi;
#else
	return EMidiAPI::Unspecified;
#endif
}

#if WITH_EDITOR
void ULibremidiSettings::PostInitProperties()
{
	Super::PostInitProperties();
	
	// Ensure the selected API is actually available
	if (!IsAPICompiledIn(BackendAPI))
	{
		BackendAPI = GetPlatformDefaultAPI();
	}
}

void ULibremidiSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULibremidiSettings, BackendAPI))
	{
		if (!IsAPICompiledIn(BackendAPI))
		{
			BackendAPI = GetPlatformDefaultAPI();
		}
	}
}

TArray<ELibremidiAPI> ULibremidiSettings::GetAvailableAPIsForPlatform()
{
	TArray<ELibremidiAPI> AvailableAPIs;
	
	// Always available
	AvailableAPIs.Add(ELibremidiAPI::Unspecified);
	AvailableAPIs.Add(ELibremidiAPI::Dummy);

	// Query libremidi for MIDI 1.0 APIs
	for (auto api : libremidi::available_apis())
	{
		ELibremidiAPI ueAPI = LibremidiTypeConversion::FromLibremidiAPI(api);
		if (ueAPI != ELibremidiAPI::Unspecified && ueAPI != ELibremidiAPI::Dummy)
		{
			AvailableAPIs.AddUnique(ueAPI);
		}
	}

	// Query libremidi for MIDI 2.0 APIs
	for (auto api : libremidi::available_ump_apis())
	{
		ELibremidiAPI ueAPI = LibremidiTypeConversion::FromLibremidiAPI(api);
		if (ueAPI != ELibremidiAPI::Unspecified && ueAPI != ELibremidiAPI::Dummy)
		{
			AvailableAPIs.AddUnique(ueAPI);
		}
	}

	return AvailableAPIs;
}

bool ULibremidiSettings::IsAPICompiledIn(ELibremidiAPI API)
{
	if (API == ELibremidiAPI::Unspecified || API == ELibremidiAPI::Dummy)
	{
		return true;
	}

	libremidi::API LibAPI = LibremidiTypeConversion::ToLibremidiAPI(API);

	// Check MIDI 1.0 APIs
	for (auto api : libremidi::available_apis())
	{
		if (api == LibAPI)
		{
			return true;
		}
	}

	// Check MIDI 2.0 APIs
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == LibAPI)
		{
			return true;
		}
	}

	return false;
}
#endif
