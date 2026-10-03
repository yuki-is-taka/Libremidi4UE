// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.
//
// LibremidiTimestampTest.cpp
//
// Automation test for ConvertTicksToNs (LibremidiTimestampConversion.h), the tick -> nanosecond
// conversion the Windows MIDI Services timestamp-domain check uses to express QPC-now in
// nanoseconds. Pure-function coverage only: no live Windows MIDI Services session and no
// libremidi::midi_in instance. Whether delivered timestamps really are nanoseconds on the QPC
// timeline is checked at runtime by ULibremidiInput's timestamp-domain check, and on every
// libremidi pin bump by the hardware probe's timestamp row.
//
// Registered under "Libremidi4UE.Timestamp.*".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "LibremidiTimestampConversion.h"

// Compile-time proof that ConvertTicksToNs has no UB negating INT64_MIN: a runtime comparison alone
// cannot show this — MSVC's release codegen happens to produce the mathematically-correct wrapped
// result for a naive `-RawTicks` even though signed overflow is UB, so a runtime test cannot tell a
// correct implementation from a lucky one. A constexpr evaluation cannot: the compiler must hard-error
// on UB in a constant expression, so this static_assert compiling at all is the actual proof.
static_assert(Libremidi4UE::ConvertTicksToNs(TNumericLimits<int64>::Min(), 1ull << 63) == -1'000'000'000);

static constexpr EAutomationTestFlags LibremidiTimestampTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLibremidiTimestampConversionTest,
	"Libremidi4UE.Timestamp.TicksToNs", LibremidiTimestampTestFlags)

bool FLibremidiTimestampConversionTest::RunTest(const FString& Parameters)
{
	using namespace Libremidi4UE;

	// ---- Known-frequency case (10 MHz, QPC's typical value on modern Windows) ----
	{
		constexpr uint64 Freq = 10'000'000ull; // 1e7
		// 123,456,789 ticks / 1e7 Hz = 12.3456789 s; since 1e9/Freq == 100 exactly, Ns == Ticks*100.
		const int64 Result = ConvertTicksToNs(123'456'789, Freq);
		TestEqual(TEXT("Known-frequency conversion"), Result, static_cast<int64>(12'345'678'900));
	}

	// ---- Boundary case: ticks large enough that the naive `ticks * 1e9 / freq` would already have
	//      overflowed int64 (max ~9.22e18), confirming the quotient/remainder split avoids it.
	//      ~2000 s of uptime at 10 MHz, with a non-zero remainder. ----
	{
		constexpr uint64 Freq = 10'000'000ull;
		constexpr int64 RawTicks = 20'000'000'123; // naive Ticks*1'000'000'000 ~= 2e19, overflows int64
		const int64 Result = ConvertTicksToNs(RawTicks, Freq);
		TestEqual(TEXT("Overflow-boundary conversion"), Result, static_cast<int64>(2'000'000'012'300));
	}

	// ---- Freq == 0 -> passthrough (the divide-by-zero guard), both signs ----
	{
		TestEqual(TEXT("Freq==0 passthrough (positive)"), ConvertTicksToNs(12345, 0), static_cast<int64>(12345));
		TestEqual(TEXT("Freq==0 passthrough (negative)"), ConvertTicksToNs(-999, 0), static_cast<int64>(-999));
	}

	// ---- Negative input -> correct sign ----
	{
		constexpr uint64 Freq = 10'000'000ull;
		const int64 Result = ConvertTicksToNs(-50'000'000, Freq); // -5.0 s
		TestEqual(TEXT("Negative input conversion"), Result, static_cast<int64>(-5'000'000'000));
	}

	// ---- INT64_MIN does not trigger UB when negated. Freq == 2^63 keeps Magnitude/Freq == 1
	//      exactly, isolating the negation path from the (separate, frequency-mismatch) multiply
	//      overflow the boundary case above already covers — at a realistic QPC frequency,
	//      |INT64_MIN| ticks represents ~29,000 years of uptime and would hit that second overflow
	//      too, which is not what this case is testing. ----
	{
		constexpr uint64 Freq = 9'223'372'036'854'775'808ull; // 2^63, matches |INT64_MIN| exactly
		const int64 Result = ConvertTicksToNs(TNumericLimits<int64>::Min(), Freq);
		TestEqual(TEXT("INT64_MIN input: no UB negating, correct sign/magnitude"), Result, static_cast<int64>(-1'000'000'000));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
