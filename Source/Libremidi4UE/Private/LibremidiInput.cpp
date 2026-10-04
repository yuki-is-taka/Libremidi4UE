// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#include "LibremidiInput.h"
#include "Libremidi4UELog.h"
#include "LibremidiEngineSubsystem.h"
#include "LibremidiMessage.h"
#include "LibremidiTimestampConversion.h"
#include "LibremidiExceptionGuard.h"

#if PLATFORM_WINDOWS
// Deliberately NOT routed through Windows/AllowWindowsPlatformTypes.h: that wrapper pulls in
// Windows/MinWindows.h, which unconditionally (re)defines WIN32_LEAN_AND_MEAN/NOMINMAX with no
// #ifndef guard — colliding (C4005) with libremidi's own winmm/error_domain.hpp and config.hpp,
// which define the same macros with different token text and are already active in this TU via
// LibremidiInput.h's THIRD_PARTY_INCLUDES block above. A raw <Windows.h> here is a no-op (the real
// windows.h's own include guard skips it) since libremidi already pulled it in first; it just makes
// the LARGE_INTEGER / QueryPerformanceFrequency / QueryPerformanceCounter dependency below explicit.
#include <Windows.h>
#endif

THIRD_PARTY_INCLUDES_START
#if defined(__APPLE__) // see LibremidiEngineSubsystem.h: shadow Carbon's FVector during libremidi include (UE 5.8)
#define FVector FVector_CarbonShadow_DoNotUse
#endif
#include <libremidi/libremidi.hpp>
#include <libremidi/configurations.hpp>
#if defined(__APPLE__)
#undef FVector
#endif
THIRD_PARTY_INCLUDES_END

namespace
{
	// Constructing a midi_in can throw (on the Windows MIDI Services backend: C++/WinRT failures).
	// Nothing may escape; on failure the input stays uninitialized and MIDI input stays disabled.
	template <typename TConfig>
	bool TryConstructMidiIn(
		TUniquePtr<libremidi::midi_in>& OutMidiIn,
		TConfig& Config,
		libremidi::input_api_configuration& ApiConfig)
	{
		try
		{
			OutMidiIn = MakeUnique<libremidi::midi_in>(Config, ApiConfig);
			return true;
		}
		catch (...)
		{
			OutMidiIn.Reset();
			UE_LOG(LogLibremidi4UE, Error,
				TEXT("LibremidiInput: libremidi threw while creating the input: %s. MIDI input is disabled."),
				*Libremidi4UE::DescribeCurrentException());
			return false;
		}
	}
}

ULibremidiEngineSubsystem* ULibremidiInput::GetOwnerSubsystem() const
{
	return Cast<ULibremidiEngineSubsystem>(GetOuter());
}

ULibremidiInput* ULibremidiInput::Create(
	UObject* Outer,
	ELibremidiMidiProtocol Protocol,
	bool bInIgnoreSysex,
	bool bInIgnoreTiming,
	bool bInIgnoreSensing,
	ELibremidiTimestampMode Mode,
	bool bInMidi1ChannelEventsToMidi2)
{
	if (!Outer)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiInput Create failed: Outer is null."));
		return nullptr;
	}

	ULibremidiInput* Input = NewObject<ULibremidiInput>(Outer);
	if (!Input)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiInput Create failed: NewObject returned null."));
		return nullptr;
	}

	if (!Input->Initialize(
		Protocol,
		bInIgnoreSysex,
		bInIgnoreTiming,
		bInIgnoreSensing,
		Mode,
		bInMidi1ChannelEventsToMidi2))
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiInput Create failed: initialization failed."));
		return nullptr;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiInput created and initialized."));

	return Input;
}

bool ULibremidiInput::Initialize(
	ELibremidiMidiProtocol Protocol,
	bool bInIgnoreSysex,
	bool bInIgnoreTiming,
	bool bInIgnoreSensing,
	ELibremidiTimestampMode Mode,
	bool bInMidi1ChannelEventsToMidi2)
{
	if (MidiIn)
	{
		if (MidiIn->is_port_open())
		{
			MidiIn->close_port();
		}
		MidiIn.Reset();
	}

	MidiProtocol = Protocol;
	bIgnoreSysex = bInIgnoreSysex;
	bIgnoreTiming = bInIgnoreTiming;
	bIgnoreSensing = bInIgnoreSensing;
	bMidi1ChannelEventsToMidi2 = bInMidi1ChannelEventsToMidi2;
	SetTimestampMode(Mode);

	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiInput Initialize: Protocol=%s IgnoreSysex=%s IgnoreTiming=%s IgnoreSensing=%s TimestampMode=%d Midi1ToMidi2=%s"),
		MidiProtocol == ELibremidiMidiProtocol::Midi2 ? TEXT("MIDI2") : TEXT("MIDI1"),
		bIgnoreSysex ? TEXT("true") : TEXT("false"),
		bIgnoreTiming ? TEXT("true") : TEXT("false"),
		bIgnoreSensing ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Mode),
		bMidi1ChannelEventsToMidi2 ? TEXT("true") : TEXT("false"));

	const ULibremidiEngineSubsystem* OwnerSubsystem = GetOwnerSubsystem();
	const libremidi::API ObserverApi = OwnerSubsystem ? OwnerSubsystem->GetObserverApi() : libremidi::API::UNSPECIFIED;
	if (ObserverApi == libremidi::API::UNSPECIFIED)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiInput Initialize failed: observer API unspecified."));
		return false;
	}
	libremidi::input_api_configuration ApiConfig = ObserverApi;
	const std::string_view ApiName = libremidi::get_api_display_name(ObserverApi);
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiInput Initialize using API: %s"), UTF8_TO_TCHAR(ApiName.data()));

	if (MidiProtocol == ELibremidiMidiProtocol::Midi2)
	{
		libremidi::ump_input_configuration Config;
		Config.on_message = [this](libremidi::ump&& Message) { HandleUmpMessage(MoveTemp(Message)); };
		Config.on_raw_data = [this](std::span<const uint32_t> Data, libremidi::timestamp Timestamp)
		{
			HandleUmpRawData(Data, Timestamp);
		};
		// Identity transform. The Windows MIDI Services timestamp-domain check treats Custom like
		// Absolute, which holds only while this stays the identity.
		Config.get_timestamp = [](libremidi::timestamp Timestamp) { return Timestamp; };
		Config.on_error = [this](std::string_view ErrorText, const libremidi::source_location& Location)
		{
			HandleError(ErrorText, Location);
		};
		Config.on_warning = [this](std::string_view WarningText, const libremidi::source_location& Location)
		{
			HandleWarning(WarningText, Location);
		};
		Config.ignore_sysex = bIgnoreSysex;
		Config.ignore_timing = bIgnoreTiming;
		Config.ignore_sensing = bIgnoreSensing;
		Config.timestamps = TimestampMode;
		Config.midi1_channel_events_to_midi2 = bMidi1ChannelEventsToMidi2;

		if (!TryConstructMidiIn(MidiIn, Config, ApiConfig))
		{
			return false;
		}
		UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiInput created MIDI2 (UMP) input."));
	}
	else
	{
		libremidi::input_configuration Config;
		Config.on_message = [this](libremidi::message&& Message) { HandleMessage(MoveTemp(Message)); };
		Config.on_raw_data = [this](std::span<const uint8_t> Data, libremidi::timestamp Timestamp)
		{
			HandleRawData(Data, Timestamp);
		};
		// Identity transform, for the same reason as in the MIDI2 configuration above.
		Config.get_timestamp = [](libremidi::timestamp Timestamp) { return Timestamp; };
		Config.on_error = [this](std::string_view ErrorText, const libremidi::source_location& Location)
		{
			HandleError(ErrorText, Location);
		};
		Config.on_warning = [this](std::string_view WarningText, const libremidi::source_location& Location)
		{
			HandleWarning(WarningText, Location);
		};
		Config.ignore_sysex = bIgnoreSysex;
		Config.ignore_timing = bIgnoreTiming;
		Config.ignore_sensing = bIgnoreSensing;
		Config.timestamps = TimestampMode;

		if (!TryConstructMidiIn(MidiIn, Config, ApiConfig))
		{
			return false;
		}
		UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiInput created MIDI1 input."));
	}

#if PLATFORM_WINDOWS
	// Windows MIDI Services timestamp-domain check: decided from the API this MidiIn actually resolved
	// to and the timestamp_mode baked into its Config, both as of construction (the Midi2/UMP and Midi1
	// branches above converge here). Absolute and Custom (whose get_timestamp is the identity) deliver
	// instants; Relative delivers deltas and is not checked.
	bCheckTimestampDomain = false;
	bTimestampDomainChecked = false;
	CachedQpcFrequency = 0;
	if (MidiIn.IsValid()
		&& MidiIn->get_current_api() == libremidi::API::WINDOWS_MIDI_SERVICES
		&& (TimestampMode == libremidi::timestamp_mode::Absolute || TimestampMode == libremidi::timestamp_mode::Custom))
	{
		LARGE_INTEGER Freq;
		CachedQpcFrequency = ::QueryPerformanceFrequency(&Freq) ? static_cast<uint64>(Freq.QuadPart) : 0;
		if (CachedQpcFrequency == 0)
		{
			UE_LOG(LogLibremidi4UE, Error,
				TEXT("LibremidiInput Initialize: QueryPerformanceFrequency failed; the Windows MIDI Services ")
				TEXT("timestamp-domain check is disabled for this input."));
		}
		bCheckTimestampDomain = CachedQpcFrequency != 0;
	}
#endif

	return MidiIn.IsValid();
}

bool ULibremidiInput::OpenInput(const FLibremidiInputInfo& PortInfo)
{
	if (!MidiIn)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiInput OpenInput failed: MIDI input not initialized."));
		return false;
	}

	libremidi::input_port Port;
	static_cast<libremidi::port_information&>(Port) = PortInfo.GetPort();
	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiInput OpenInput: %s"), UTF8_TO_TCHAR(Port.display_name.c_str()));
	try
	{
		const stdx::error Result = MidiIn->open_port(Port);
		if (Result != stdx::error{})
		{
			UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiInput OpenInput failed for port: %s"), UTF8_TO_TCHAR(Port.display_name.c_str()));
			return false;
		}
	}
	catch (...)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("LibremidiInput OpenInput: libremidi threw for port %s: %s. The port stays closed."),
			UTF8_TO_TCHAR(Port.display_name.c_str()), *Libremidi4UE::DescribeCurrentException());
		return false;
	}

	return true;
}

void ULibremidiInput::CloseInput()
{
	if (!MidiIn)
	{
		UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiInput CloseInput skipped: MIDI input not initialized."));
		return;
	}

	UE_LOG(LogLibremidi4UE, Log, TEXT("LibremidiInput CloseInput."));
	try
	{
		MidiIn->close_port();
	}
	catch (...)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("LibremidiInput CloseInput: libremidi threw: %s"),
			*Libremidi4UE::DescribeCurrentException());
	}
}

bool ULibremidiInput::IsPortOpen() const
{
	return MidiIn ? MidiIn->is_port_open() : false;
}

bool ULibremidiInput::IsPortConnected() const
{
	return MidiIn ? MidiIn->is_port_connected() : false;
}

ELibremidiMidiProtocol ULibremidiInput::GetMidiProtocol() const
{
	return MidiProtocol;
}

void ULibremidiInput::SetMidiProtocol(ELibremidiMidiProtocol Protocol)
{
	MidiProtocol = Protocol;
}

bool ULibremidiInput::GetIsUmp() const
{
	return MidiProtocol == ELibremidiMidiProtocol::Midi2;
}

bool ULibremidiInput::GetIgnoreSysex() const
{
	return bIgnoreSysex;
}

void ULibremidiInput::SetIgnoreSysex(bool bIgnore)
{
	bIgnoreSysex = bIgnore;
}

bool ULibremidiInput::GetIgnoreTiming() const
{
	return bIgnoreTiming;
}

void ULibremidiInput::SetIgnoreTiming(bool bIgnore)
{
	bIgnoreTiming = bIgnore;
}

bool ULibremidiInput::GetIgnoreSensing() const
{
	return bIgnoreSensing;
}

void ULibremidiInput::SetIgnoreSensing(bool bIgnore)
{
	bIgnoreSensing = bIgnore;
}

ELibremidiTimestampMode ULibremidiInput::GetTimestampMode() const
{
	switch (TimestampMode)
	{
	case libremidi::timestamp_mode::NoTimestamp:
		return ELibremidiTimestampMode::NoTimestamp;
	case libremidi::timestamp_mode::Relative:
		return ELibremidiTimestampMode::Relative;
	case libremidi::timestamp_mode::Absolute:
		return ELibremidiTimestampMode::Absolute;
	case libremidi::timestamp_mode::SystemMonotonic:
		return ELibremidiTimestampMode::SystemMonotonic;
	case libremidi::timestamp_mode::AudioFrame:
		return ELibremidiTimestampMode::AudioFrame;
	case libremidi::timestamp_mode::Custom:
	default:
		return ELibremidiTimestampMode::Custom;
	}
}

void ULibremidiInput::SetTimestampMode(ELibremidiTimestampMode Mode)
{
	switch (Mode)
	{
	case ELibremidiTimestampMode::NoTimestamp:
		TimestampMode = libremidi::timestamp_mode::NoTimestamp;
		break;
	case ELibremidiTimestampMode::Relative:
		TimestampMode = libremidi::timestamp_mode::Relative;
		break;
	case ELibremidiTimestampMode::Absolute:
		TimestampMode = libremidi::timestamp_mode::Absolute;
		break;
	case ELibremidiTimestampMode::SystemMonotonic:
		TimestampMode = libremidi::timestamp_mode::SystemMonotonic;
		break;
	case ELibremidiTimestampMode::AudioFrame:
		TimestampMode = libremidi::timestamp_mode::AudioFrame;
		break;
	case ELibremidiTimestampMode::Custom:
	default:
		TimestampMode = libremidi::timestamp_mode::Custom;
		break;
	}
}

bool ULibremidiInput::GetMidi1ChannelEventsToMidi2() const
{
	return bMidi1ChannelEventsToMidi2;
}

void ULibremidiInput::SetMidi1ChannelEventsToMidi2(bool bEnabled)
{
	bMidi1ChannelEventsToMidi2 = bEnabled;
}

void ULibremidiInput::HandleMessage(libremidi::message&& Message)
{
#if PLATFORM_WINDOWS
	if (bCheckTimestampDomain)
	{
		CheckTimestampDomainOnce(Message.timestamp);
	}
#endif
	FLibremidiMidi1Message Wrapped(MoveTemp(Message));
	OnMidi1Message.Broadcast(this, Wrapped);
}

void ULibremidiInput::HandleRawData(std::span<const uint8_t> Data, libremidi::timestamp Timestamp)
{
	// Raw callback path — not using on_message packetization.
	// Currently unused; available for future direct-byte consumers.
}

void ULibremidiInput::HandleUmpMessage(libremidi::ump&& Message)
{
#if PLATFORM_WINDOWS
	if (bCheckTimestampDomain)
	{
		CheckTimestampDomainOnce(Message.timestamp);
	}
#endif

	const uint8 MT = static_cast<uint8>((Message.data[0] >> 28) & 0x0F);
	UE_LOG(LogLibremidi4UE, Verbose,
		TEXT("HandleUmpMessage: MT=0x%X Word0=0x%08X port='%s'"),
		MT, Message.data[0], *GetName());

	FLibremidiUmpMessage Wrapped(MoveTemp(Message));
	OnUmpMessage.Broadcast(this, Wrapped);
}

void ULibremidiInput::HandleUmpRawData(std::span<const uint32_t> Data, libremidi::timestamp Timestamp)
{
	UE_LOG(LogLibremidi4UE, Verbose,
		TEXT("HandleUmpRawData: words=%d port='%s'"),
		static_cast<int32>(Data.size()), *GetName());
}

void ULibremidiInput::HandleError(std::string_view ErrorText, const libremidi::source_location& Location)
{
	FString Text = FString::Printf(TEXT("%s [%s:%d]"),
		UTF8_TO_TCHAR(ErrorText.data()),
		UTF8_TO_TCHAR(Location.file_name()),
		Location.line());
	UE_LOG(LogLibremidi4UE, Error, TEXT("LibremidiInput Error: %s"), *Text);
	OnError.Broadcast(this, Text);
}

void ULibremidiInput::HandleWarning(std::string_view WarningText, const libremidi::source_location& Location)
{
	FString Text = FString::Printf(TEXT("%s [%s:%d]"),
		UTF8_TO_TCHAR(WarningText.data()),
		UTF8_TO_TCHAR(Location.file_name()),
		Location.line());
	UE_LOG(LogLibremidi4UE, Warning, TEXT("LibremidiInput Warning: %s"), *Text);
	OnWarning.Broadcast(this, Text);
}

#if PLATFORM_WINDOWS
void ULibremidiInput::CheckTimestampDomainOnce(int64 TimestampNs)
{
	// One check per Initialize. The relaxed load keeps every later message to a plain read; the
	// exchange makes two callbacks racing on the first message log at most once (the Windows MIDI
	// Services callback is not guaranteed to run on one thread).
	if (bTimestampDomainChecked.load(std::memory_order_relaxed) || bTimestampDomainChecked.exchange(true))
	{
		return;
	}

	LARGE_INTEGER NowTicks;
	if (!::QueryPerformanceCounter(&NowTicks))
	{
		return; // cannot check; not itself an error
	}
	const int64 NowNs = Libremidi4UE::ConvertTicksToNs(NowTicks.QuadPart, CachedQpcFrequency);
	const int64 DiffNs = FMath::Abs(NowNs - TimestampNs);
	constexpr int64 ToleranceNs = 5LL * 1'000'000'000LL; // a few seconds: generous against callback latency
	if (DiffNs > ToleranceNs)
	{
		UE_LOG(LogLibremidi4UE, Error,
			TEXT("LibremidiInput: Windows MIDI Services timestamps are not in the expected domain ")
			TEXT("(delivered=%lld ns, QPC-now=%lld ns, diff=%lld ns): this input's message timestamps are ")
			TEXT("not nanoseconds on the QueryPerformanceCounter timeline. Do not trust them for timing."),
			TimestampNs, NowNs, DiffNs);
	}
	else
	{
		UE_LOG(LogLibremidi4UE, Verbose,
			TEXT("LibremidiInput: Windows MIDI Services timestamp-domain check passed (delivered=%lld ns, ")
			TEXT("QPC-now=%lld ns, diff=%lld ns)."),
			TimestampNs, NowNs, DiffNs);
	}
}
#endif
