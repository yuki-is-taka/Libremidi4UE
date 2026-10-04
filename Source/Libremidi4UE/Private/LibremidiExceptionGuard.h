// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.
//
// LibremidiExceptionGuard.h
//
// libremidi is header-only and compiled into this module; its Windows MIDI Services backend drives
// C++/WinRT, which reports failures (service missing, runtime DLL not loadable, device errors) by
// throwing. Nothing may escape into the engine: every place that constructs or opens a libremidi
// observer / midi_in / midi_out catches, logs through this helper, and leaves MIDI disabled.

#pragma once

#include "CoreMinimal.h"

#include <exception>

namespace Libremidi4UE
{
	/**
	 * Describes the exception currently being handled. Call only from inside a catch block.
	 * std::exception (what()) is reported by message; anything else (e.g. a C++/WinRT hresult_error,
	 * which does not derive from std::exception) as a non-std exception.
	 */
	inline FString DescribeCurrentException()
	{
		try
		{
			throw;
		}
		catch (const std::exception& Exception)
		{
			return FString(UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			return FString(TEXT("non-std exception"));
		}
	}
}
