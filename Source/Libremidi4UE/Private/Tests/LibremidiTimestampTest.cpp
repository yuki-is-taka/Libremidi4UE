// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.
//
// LibremidiTimestampTest.cpp
//
// Automation test for the winmidi raw-tick timestamp correction (Libremidi4UE ADR-0002):
// ConvertTicksToNs / NeedsWinmidiTickCorrection (LibremidiTimestampConversion.h). Pure-function
// coverage only — no live WMS session, no libremidi::midi_in instance. What this cannot cover
// (that QueryPerformanceFrequency's returned value is the one really behind a device's timestamp,
// and that WMS's timestamp truly is QPC-ticks-mislabeled in the first place) needs the live Erae;
// see ADR-0002's Confirmation section (the runtime cross-check, RunTimestampCrossCheckOnce).
//
// Registered under "Libremidi4UE.Timestamp.*".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "LibremidiTimestampConversion.h"

// Compile-time proof that ConvertTicksToNs has no UB negating INT64_MIN (ADR-0002 Confirmation): a runtime
// comparison alone cannot show this — MSVC's release codegen happens to produce the mathematically-correct
// wrapped result for a naive `-RawTicks` even though signed overflow is UB, so a runtime test cannot tell a
// correct implementation from a lucky one. A constexpr evaluation cannot: the compiler must hard-error on UB
// in a constant expression, so this static_assert compiling at all is the actual proof.
static_assert(Libremidi4UE::ConvertTicksToNs(TNumericLimits<int64>::Min(), 1ull << 63) == -1'000'000'000);

static constexpr EAutomationTestFlags LibremidiTimestampTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLibremidiTimestampConversionTest,
	"Libremidi4UE.Timestamp.WinmidiTickConversion", LibremidiTimestampTestFlags)

bool FLibremidiTimestampConversionTest::RunTest(const FString& Parameters)
{
	using namespace Libremidi4UE;

	// ---- ConvertTicksToNs: known-frequency case (10 MHz, QPC's typical value on modern Windows) ----
	{
		constexpr uint64 Freq = 10'000'000ull; // 1e7
		// 123,456,789 ticks / 1e7 Hz = 12.3456789 s; since 1e9/Freq == 100 exactly, Ns == Ticks*100.
		const int64 Result = ConvertTicksToNs(123'456'789, Freq);
		TestEqual(TEXT("Known-frequency conversion"), Result, static_cast<int64>(12'345'678'900));
	}

	// ---- ConvertTicksToNs: boundary case — ticks large enough that the naive `ticks * 1e9 / freq`
	//      would already have overflowed int64 (max ~9.22e18), confirming the quotient/remainder
	//      split avoids it. ~2000 s of device-clock uptime at 10 MHz, with a non-zero remainder. ----
	{
		constexpr uint64 Freq = 10'000'000ull;
		constexpr int64 RawTicks = 20'000'000'123; // naive Ticks*1'000'000'000 ~= 2e19, overflows int64
		const int64 Result = ConvertTicksToNs(RawTicks, Freq);
		TestEqual(TEXT("Overflow-boundary conversion"), Result, static_cast<int64>(2'000'000'012'300));
	}

	// ---- ConvertTicksToNs: Freq == 0 -> passthrough (the divide-by-zero guard), both signs ----
	{
		TestEqual(TEXT("Freq==0 passthrough (positive)"), ConvertTicksToNs(12345, 0), static_cast<int64>(12345));
		TestEqual(TEXT("Freq==0 passthrough (negative)"), ConvertTicksToNs(-999, 0), static_cast<int64>(-999));
	}

	// ---- ConvertTicksToNs: negative RawTicks (the Relative-mode delta case) -> correct sign ----
	{
		constexpr uint64 Freq = 10'000'000ull;
		const int64 Result = ConvertTicksToNs(-50'000'000, Freq); // -5.0 s
		TestEqual(TEXT("Negative delta conversion"), Result, static_cast<int64>(-5'000'000'000));
	}

	// ---- ConvertTicksToNs: INT64_MIN does not trigger UB when negated. Freq == 2^63 keeps
	//      Magnitude/Freq == 1 exactly, isolating the negation path from the (separate,
	//      frequency-mismatch) multiply overflow the boundary case above already covers — at a
	//      realistic QPC frequency, |INT64_MIN| ticks represents ~29,000 years of uptime and would
	//      hit that second overflow too, which is not what this case is testing. ----
	{
		constexpr uint64 Freq = 9'223'372'036'854'775'808ull; // 2^63, matches |INT64_MIN| exactly
		const int64 Result = ConvertTicksToNs(TNumericLimits<int64>::Min(), Freq);
		TestEqual(TEXT("INT64_MIN input: no UB negating, correct sign/magnitude"), Result, static_cast<int64>(-1'000'000'000));
	}

	// ---- NeedsWinmidiTickCorrection: false for every non-WINDOWS_MIDI_SERVICES API, any mode ----
	{
		const libremidi::API OtherApis[] = {
			libremidi::API::UNSPECIFIED, libremidi::API::COREMIDI, libremidi::API::ALSA_SEQ,
			libremidi::API::WINDOWS_MM, libremidi::API::JACK_MIDI,
		};
		const libremidi::timestamp_mode AllModes[] = {
			libremidi::timestamp_mode::NoTimestamp, libremidi::timestamp_mode::Relative,
			libremidi::timestamp_mode::Absolute, libremidi::timestamp_mode::SystemMonotonic,
			libremidi::timestamp_mode::AudioFrame, libremidi::timestamp_mode::Custom,
		};
		for (const libremidi::API Api : OtherApis)
		{
			for (const libremidi::timestamp_mode Mode : AllModes)
			{
				TestFalse(FString::Printf(TEXT("Non-WMS API %d never needs correction (mode %d)"),
					static_cast<int32>(Api), static_cast<int32>(Mode)), NeedsWinmidiTickCorrection(Api, Mode));
			}
		}
	}

	// ---- NeedsWinmidiTickCorrection on WINDOWS_MIDI_SERVICES: true only for Absolute/Relative/Custom ----
	{
		constexpr libremidi::API Api = libremidi::API::WINDOWS_MIDI_SERVICES;
		TestTrue(TEXT("WMS Absolute needs correction"), NeedsWinmidiTickCorrection(Api, libremidi::timestamp_mode::Absolute));
		TestTrue(TEXT("WMS Relative needs correction"), NeedsWinmidiTickCorrection(Api, libremidi::timestamp_mode::Relative));
		TestTrue(TEXT("WMS Custom needs correction"), NeedsWinmidiTickCorrection(Api, libremidi::timestamp_mode::Custom));
		TestFalse(TEXT("WMS NoTimestamp never needs correction"), NeedsWinmidiTickCorrection(Api, libremidi::timestamp_mode::NoTimestamp));
		TestFalse(TEXT("WMS AudioFrame never needs correction"), NeedsWinmidiTickCorrection(Api, libremidi::timestamp_mode::AudioFrame));
		TestFalse(TEXT("WMS SystemMonotonic must NOT be double-converted"), NeedsWinmidiTickCorrection(Api, libremidi::timestamp_mode::SystemMonotonic));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
