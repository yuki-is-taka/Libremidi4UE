// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.
//
// LibremidiTimestampConversion.h
//
// winmidi (WINDOWS_MIDI_SERVICES) returns raw QPC ticks, mislabeled as nanoseconds, from its
// to_ns() for the Absolute/Relative/Custom timestamp_mode branches (Libremidi4UE ADR-0002). These
// pure functions convert ticks -> true nanoseconds given a cached QPC frequency, and decide which
// (API, timestamp_mode) combination actually needs the correction. Extracted into their own header
// (rather than an anonymous namespace inside LibremidiInput.cpp) so LibremidiTimestampTest.cpp can
// exercise them directly, with no live WMS session and no libremidi::midi_in instance required.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/api.hpp>
#include <libremidi/input_configuration.hpp>
THIRD_PARTY_INCLUDES_END

namespace Libremidi4UE
{
	/**
	 * Converts a winmidi raw-tick value (QPC ticks, mislabeled as ns by the submodule's to_ns())
	 * into true nanoseconds. Freq == 0 means "no cached frequency" (query failed, or not yet run) —
	 * passthrough, never divide by zero. Signed: Relative deltas can be negative; the magnitude is
	 * converted via unsigned quotient/remainder (avoids the overflow a naive
	 * `ticks * 1'000'000'000 / freq` hits above ~15 minutes of device-clock uptime at a 10 MHz tick
	 * rate), then the sign is reapplied.
	 */
	constexpr int64 ConvertTicksToNs(int64 RawTicks, uint64 Freq)
	{
		if (Freq == 0)
		{
			return RawTicks; // no known frequency — cannot convert; leave as-is rather than corrupt
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

	/**
	 * Whether this (API, timestamp_mode) combination needs ConvertTicksToNs applied at all.
	 * Only winmidi's Absolute/Relative/Custom branches route a raw tick value through to_ns()
	 * (midi_stream_decoder.hpp's input_state_machine_base::timestamp<info>); NoTimestamp/AudioFrame
	 * never call it, and SystemMonotonic is already true ns via its system_ns() fallback (winmidi's
	 * absolute_is_monotonic is false) and must NOT be touched — converting it a second time would
	 * corrupt an already-correct value.
	 */
	inline bool NeedsWinmidiTickCorrection(libremidi::API Api, libremidi::timestamp_mode Mode)
	{
		if (Api != libremidi::API::WINDOWS_MIDI_SERVICES)
		{
			return false; // every other backend examined already returns true ns
		}
		switch (Mode)
		{
		case libremidi::timestamp_mode::Absolute:
		case libremidi::timestamp_mode::Relative:
		case libremidi::timestamp_mode::Custom:
			return true; // the only branches that route through to_ns() for winmidi
		default:
			return false; // NoTimestamp / AudioFrame never call to_ns(); SystemMonotonic is already ns
		}
	}
}
