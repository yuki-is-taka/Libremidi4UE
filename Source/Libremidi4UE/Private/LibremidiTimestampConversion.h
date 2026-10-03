// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.
//
// LibremidiTimestampConversion.h
//
// Overflow-safe conversion of a performance-counter tick count to nanoseconds. The Windows MIDI
// Services timestamp-domain check (ULibremidiInput) uses it to express QueryPerformanceCounter-now
// in nanoseconds before comparing it with a delivered message timestamp. Kept in its own header,
// free of platform and libremidi dependencies, so LibremidiTimestampTest.cpp can exercise it
// directly on every platform.

#pragma once

#include "CoreMinimal.h"

namespace Libremidi4UE
{
	/**
	 * Converts a tick count of a counter running at Freq Hz (QueryPerformanceFrequency) into
	 * nanoseconds. Freq == 0 means "no known frequency" (query failed, or not yet run): passthrough,
	 * never divide by zero. Signed: negative inputs keep their sign. The magnitude is converted via an
	 * unsigned quotient/remainder split, which avoids the overflow a naive
	 * `ticks * 1'000'000'000 / freq` hits above ~15 minutes of uptime at a 10 MHz counter.
	 */
	constexpr int64 ConvertTicksToNs(int64 RawTicks, uint64 Freq)
	{
		if (Freq == 0)
		{
			return RawTicks; // no known frequency: cannot convert; leave as-is rather than corrupt
		}
		const bool bNegative = RawTicks < 0;
		const uint64 Magnitude = bNegative
			? static_cast<uint64>(-(RawTicks + 1)) + 1 // avoids UB negating INT64_MIN
			: static_cast<uint64>(RawTicks);
		const uint64 Whole = Magnitude / Freq;
		const uint64 Rem   = Magnitude % Freq; // < Freq (~1e7 for QPC), so Rem * 1e9 (~1e16) fits in uint64
		const uint64 Ns = Whole * 1'000'000'000ull + (Rem * 1'000'000'000ull) / Freq;
		return bNegative ? -static_cast<int64>(Ns) : static_cast<int64>(Ns);
	}
}
