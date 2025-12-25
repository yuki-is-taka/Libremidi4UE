#include "LibremidiSettings.h"
#include "Libremidi4UELog.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

ULibremidiSettings::ULibremidiSettings()
	: BackendAPIName(APIToString(GetPlatformDefaultAPI()))
	, bTrackHardware(true)
	, bTrackVirtual(true)
	, bTrackAny(false)
	, CachedBackendAPI(ELibremidiAPI::Unspecified)
	, bCacheValid(false)
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
			return ELibremidiAPI::CoreMidiUMP;
		}
	}
	return ELibremidiAPI::CoreMidi;
#elif PLATFORM_LINUX
	// Check if ALSA Sequencer UMP (MIDI 2.0) is available
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == libremidi::API::ALSA_SEQ_UMP)
		{
			return ELibremidiAPI::AlsaSeqUMP;
		}
	}
	return ELibremidiAPI::AlsaSeq;
#elif PLATFORM_IOS
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == libremidi::API::COREMIDI_UMP)
		{
			return ELibremidiAPI::CoreMidiUMP;
		}
	}
	return ELibremidiAPI::CoreMidi;
#else
	return ELibremidiAPI::Unspecified;
#endif
}

ELibremidiAPI ULibremidiSettings::GetPreferredAPI(ELibremidiAPI RequestedAPI)
{
	// If a specific API is requested (not Unspecified), use it as-is
	if (RequestedAPI != ELibremidiAPI::Unspecified)
	{
		return RequestedAPI;
	}

	// For Unspecified, prefer UMP-capable APIs based on platform
#if PLATFORM_WINDOWS
	// Priority: Windows MIDI Services (MIDI 2.0) > WindowsUWP > WindowsMM
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == libremidi::API::WINDOWS_MIDI_SERVICES)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: Windows MIDI Services (MIDI 2.0 UMP)"));
			return ELibremidiAPI::WindowsMidiServices;
		}
	}
	
	// Fallback to legacy APIs
	for (auto api : libremidi::available_apis())
	{
		if (api == libremidi::API::WINDOWS_UWP)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: Windows UWP"));
			return ELibremidiAPI::WindowsUWP;
		}
	}
	
	UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: Windows MM (Legacy)"));
	return ELibremidiAPI::WindowsMM;

#elif PLATFORM_MAC
	// Priority: CoreMIDI UMP (MIDI 2.0) > CoreMIDI
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == libremidi::API::COREMIDI_UMP)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: CoreMIDI UMP (MIDI 2.0)"));
			return ELibremidiAPI::CoreMidiUMP;
		}
	}
	
	UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: CoreMIDI"));
	return ELibremidiAPI::CoreMidi;

#elif PLATFORM_LINUX
	// Priority: PipeWire UMP > ALSA Seq UMP > ALSA Raw UMP > JACK UMP > legacy versions
	const auto umpApis = libremidi::available_ump_apis();
	
	// Try PipeWire UMP first (most modern)
	for (auto api : umpApis)
	{
		if (api == libremidi::API::PIPEWIRE_UMP)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: PipeWire UMP (MIDI 2.0)"));
			return ELibremidiAPI::PipeWireUMP;
		}
	}
	
	// Try ALSA Sequencer UMP
	for (auto api : umpApis)
	{
		if (api == libremidi::API::ALSA_SEQ_UMP)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: ALSA Sequencer UMP (MIDI 2.0)"));
			return ELibremidiAPI::AlsaSeqUMP;
		}
	}
	
	// Try ALSA Raw UMP
	for (auto api : umpApis)
	{
		if (api == libremidi::API::ALSA_RAW_UMP)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: ALSA Raw UMP (MIDI 2.0)"));
			return ELibremidiAPI::AlsaRawUMP;
		}
	}
	
	// Try JACK UMP
	for (auto api : umpApis)
	{
		if (api == libremidi::API::JACK_UMP)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: JACK UMP (MIDI 2.0)"));
			return ELibremidiAPI::JackUMP;
		}
	}
	
	// Fallback to legacy APIs
	const auto legacyApis = libremidi::available_apis();
	
	// Try PipeWire (legacy)
	for (auto api : legacyApis)
	{
		if (api == libremidi::API::PIPEWIRE)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: PipeWire (MIDI 1.0)"));
			return ELibremidiAPI::PipeWire;
		}
	}
	
	// Try ALSA Sequencer
	for (auto api : legacyApis)
	{
		if (api == libremidi::API::ALSA_SEQ)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: ALSA Sequencer (MIDI 1.0)"));
			return ELibremidiAPI::AlsaSeq;
		}
	}
	
	// Try JACK
	for (auto api : legacyApis)
	{
		if (api == libremidi::API::JACK_MIDI)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: JACK (MIDI 1.0)"));
			return ELibremidiAPI::Jack;
		}
	}
	
	// Last resort: ALSA Raw
	UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: ALSA Raw (MIDI 1.0)"));
	return ELibremidiAPI::AlsaRaw;

#elif PLATFORM_IOS
	// Priority: CoreMIDI UMP (MIDI 2.0) > CoreMIDI
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == libremidi::API::COREMIDI_UMP)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: CoreMIDI UMP (MIDI 2.0)"));
			return ELibremidiAPI::CoreMidiUMP;
		}
	}
	
	UE_LOG(LogLibremidi4UE, Log, TEXT("Auto-selected: CoreMIDI"));
	return ELibremidiAPI::CoreMidi;

#else
	// Unknown platform - let libremidi decide
	UE_LOG(LogLibremidi4UE, Warning, TEXT("Auto-selected: Unspecified (unknown platform)"));
	return ELibremidiAPI::Unspecified;
#endif
}

#if WITH_EDITOR

// ?????????????????????????
TArray<FString> ULibremidiSettings::GetAvailableAPIOptions() const
{
	UE_LOG(LogLibremidi4UE, Warning, TEXT("=== GetAvailableAPIOptions() called ==="));
	
	TArray<FString> Options;
	TArray<ELibremidiAPI> AvailableAPIs = GetAvailableAPIsForPlatform();
	
	UE_LOG(LogLibremidi4UE, Warning, TEXT("Available APIs count: %d"), AvailableAPIs.Num());
	
	for (ELibremidiAPI API : AvailableAPIs)
	{
		// Enum????????UMETA DisplayName??????
		FString DisplayName = UEnum::GetDisplayValueAsText(API).ToString();
		Options.Add(DisplayName);
		UE_LOG(LogLibremidi4UE, Warning, TEXT("  - %s"), *DisplayName);
	}
	
	UE_LOG(LogLibremidi4UE, Warning, TEXT("=== Options list complete ==="));
	
	return Options;
}

// ???????????API??????
TArray<ELibremidiAPI> ULibremidiSettings::GetAvailableAPIsForPlatform()
{
	TArray<ELibremidiAPI> AvailableAPIs;
	
	// ??????
	AvailableAPIs.Add(ELibremidiAPI::Unspecified);
	AvailableAPIs.Add(ELibremidiAPI::Dummy);

	// ?????????????API???
#if PLATFORM_WINDOWS
	TArray<ELibremidiAPI> PlatformAPIs = {
		ELibremidiAPI::WindowsMidiServices,
		ELibremidiAPI::WindowsUWP,
		ELibremidiAPI::WindowsMM
	};
#elif PLATFORM_MAC
	TArray<ELibremidiAPI> PlatformAPIs = {
		ELibremidiAPI::CoreMidiUMP,
		ELibremidiAPI::CoreMidi
	};
#elif PLATFORM_IOS
	TArray<ELibremidiAPI> PlatformAPIs = {
		ELibremidiAPI::CoreMidiUMP,
		ELibremidiAPI::CoreMidi
	};
#elif PLATFORM_LINUX
	TArray<ELibremidiAPI> PlatformAPIs = {
		ELibremidiAPI::PipeWireUMP,
		ELibremidiAPI::AlsaSeqUMP,
		ELibremidiAPI::AlsaRawUMP,
		ELibremidiAPI::JackUMP,
		ELibremidiAPI::PipeWire,
		ELibremidiAPI::AlsaSeq,
		ELibremidiAPI::AlsaRaw,
		ELibremidiAPI::Jack
	};
#else
	TArray<ELibremidiAPI> PlatformAPIs;
#endif

	// ???????????API?????
	for (ELibremidiAPI API : PlatformAPIs)
	{
		if (IsAPICompiledIn(API))
		{
			AvailableAPIs.Add(API);
		}
	}

	return AvailableAPIs;
}

// ???????????????
bool ULibremidiSettings::IsAPIValidForCurrentPlatform(ELibremidiAPI API)
{
	// Unspecified/Dummy?????
	if (API == ELibremidiAPI::Unspecified || API == ELibremidiAPI::Dummy)
		return true;

#if PLATFORM_WINDOWS
	switch (API) {
		case ELibremidiAPI::WindowsMM:
		case ELibremidiAPI::WindowsUWP:
		case ELibremidiAPI::WindowsMidiServices:
			return true;
		default:
			return false;
	}
#elif PLATFORM_MAC
	switch (API) {
		case ELibremidiAPI::CoreMidi:
		case ELibremidiAPI::CoreMidiUMP:
			return true;
		default:
			return false;
	}
#elif PLATFORM_IOS
	switch (API) {
		case ELibremidiAPI::CoreMidi:
		case ELibremidiAPI::CoreMidiUMP:
			return true;
		default:
			return false;
	}
#elif PLATFORM_LINUX
	switch (API) {
		case ELibremidiAPI::AlsaSeq:
		case ELibremidiAPI::AlsaRaw:
		case ELibremidiAPI::AlsaSeqUMP:
		case ELibremidiAPI::AlsaRawUMP:
		case ELibremidiAPI::Jack:
		case ELibremidiAPI::JackUMP:
		case ELibremidiAPI::PipeWire:
		case ELibremidiAPI::PipeWireUMP:
			return true;
		default:
			return false;
	}
#else
	return false;
#endif
}

// ???????API????????
ELibremidiAPI ULibremidiSettings::ValidateAndPreserveAPI(ELibremidiAPI RequestedAPI)
{
	// 1. ???????????????
	if (!IsAPIValidForCurrentPlatform(RequestedAPI))
	{
		UE_LOG(LogLibremidi4UE, Warning, 
			TEXT("Previously selected API '%s' is not valid for this platform. Using default."), 
			*UEnum::GetDisplayValueAsText(RequestedAPI).ToString());
		return GetPlatformDefaultAPI();
	}

	// 2. ???????????
	if (!IsAPICompiledIn(RequestedAPI))
	{
		UE_LOG(LogLibremidi4UE, Warning, 
			TEXT("Previously selected API '%s' is not compiled in. Using default."), 
			*UEnum::GetDisplayValueAsText(RequestedAPI).ToString());
		return GetPlatformDefaultAPI();
	}

	// 3. ???API????????
	if (RequestedAPI != ELibremidiAPI::Unspecified)
	{
		UE_LOG(LogLibremidi4UE, Log, 
			TEXT("Preserving previously selected API: %s"), 
			*UEnum::GetDisplayValueAsText(RequestedAPI).ToString());
	}
	
	return RequestedAPI;
}

// ????????????API??????
void ULibremidiSettings::PostInitProperties()
{
	Super::PostInitProperties();
	
	// FString??Enum???????
	ELibremidiAPI RequestedAPI = StringToAPI(BackendAPIName);
	ELibremidiAPI ValidatedAPI = ValidateAndPreserveAPI(RequestedAPI);
	
	// ????API?FString???
	BackendAPIName = APIToString(ValidatedAPI);
	
	// ????????
	CachedBackendAPI = ValidatedAPI;
	bCacheValid = true;
}

// ????????????
void ULibremidiSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && 
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULibremidiSettings, BackendAPIName))
	{
		// FString??Enum???
		ELibremidiAPI SelectedAPI = StringToAPI(BackendAPIName);
		
		// ??
		ELibremidiAPI ValidatedAPI = ValidateAndPreserveAPI(SelectedAPI);
		
		// ???API???????????
		if (SelectedAPI != ValidatedAPI)
		{
			BackendAPIName = APIToString(ValidatedAPI);
		}
		
		// ??????????????????????
		bCacheValid = false;
	}
}

bool ULibremidiSettings::IsAPICompiledIn(ELibremidiAPI API)
{
	if (API == ELibremidiAPI::Unspecified || API == ELibremidiAPI::Dummy)
		return true;

	// ??????????????????
	if (!IsAPIValidForCurrentPlatform(API))
		return false;

	libremidi::API LibAPI = LibremidiTypeConversion::ToLibremidiAPI(API);

	// MIDI 1.0 API?????
	for (auto api : libremidi::available_apis())
	{
		if (api == LibAPI)
			return true;
	}

	// MIDI 2.0 API?????
	for (auto api : libremidi::available_ump_apis())
	{
		if (api == LibAPI)
			return true;
	}

	return false;
}

// ========================================================================
// Backend API Accessors (FString ? Enum conversion)
// ========================================================================

ELibremidiAPI ULibremidiSettings::GetBackendAPI() const
{
	// ???????????????
	if (bCacheValid)
	{
		return CachedBackendAPI;
	}

	// FString??Enum???
	CachedBackendAPI = StringToAPI(BackendAPIName);
	bCacheValid = true;
	
	return CachedBackendAPI;
}

void ULibremidiSettings::SetBackendAPI(ELibremidiAPI API)
{
	BackendAPIName = APIToString(API);
	CachedBackendAPI = API;
	bCacheValid = true;
}

ELibremidiAPI ULibremidiSettings::StringToAPI(const FString& APIName)
{
	// ????Enum??????DisplayName??????????
	const UEnum* EnumPtr = StaticEnum<ELibremidiAPI>();
	if (!EnumPtr)
	{
		return ELibremidiAPI::Unspecified;
	}

	for (int32 i = 0; i < EnumPtr->NumEnums() - 1; ++i) // -1 to skip _MAX
	{
		FString DisplayName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
		if (DisplayName.Equals(APIName, ESearchCase::IgnoreCase))
		{
			return static_cast<ELibremidiAPI>(EnumPtr->GetValueByIndex(i));
		}
	}

	// ?????????Unspecified
	UE_LOG(LogLibremidi4UE, Warning, TEXT("Unknown API name '%s', defaulting to Unspecified"), *APIName);
	return ELibremidiAPI::Unspecified;
}

FString ULibremidiSettings::APIToString(ELibremidiAPI API)
{
	return UEnum::GetDisplayValueAsText(API).ToString();
}

#endif
