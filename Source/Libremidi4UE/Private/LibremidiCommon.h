/**
 * @file LibremidiCommon.h
 * @brief Private implementation helpers for LibremidiInput and LibremidiOutput
 * 
 * This file contains internal helper functions and should not be included by public headers.
 * All public types and constants belong in LibremidiTypes.h.
 */

#pragma once

#include "CoreMinimal.h"
#include "LibremidiTypes.h"
#include "Libremidi4UELog.h"
#include "Engine/Engine.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

// Forward declarations
class ULibremidiEngineSubsystem;

/**
 * Private implementation helpers for MIDI Input/Output
 * These are intentionally not exposed in public headers
 */
namespace UE::MIDI::Private
{
	/**
	 * Get the LibremidiEngineSubsystem instance
	 * @return Pointer to subsystem, or nullptr if not available
	 */
	inline ULibremidiEngineSubsystem* GetEngineSubsystem()
	{
		return GEngine ? GEngine->GetEngineSubsystem<ULibremidiEngineSubsystem>() : nullptr;
	}

	/**
	 * Helper to set device identifier from FLibremidiPortInfo to libremidi port
	 * This is critical for Windows MIDI Services backend which requires the device field
	 */
	inline void SetDeviceIdentifier(libremidi::port_information& Port, const FLibremidiPortInfo& PortInfo)
	{
		switch (PortInfo.DeviceType)
		{
			case ELibremidiDeviceType::String:
				if (!PortInfo.DeviceString.IsEmpty())
				{
					Port.device = std::string(TCHAR_TO_UTF8(*PortInfo.DeviceString));
				}
				break;
			case ELibremidiDeviceType::Integer:
				Port.device = static_cast<uint64_t>(PortInfo.DeviceInteger);
				break;
			case ELibremidiDeviceType::None:
			default:
				// Leave as monostate
				break;
		}
	}

	/**
	 * Convert FMidiPortInfo to libremidi::input_port
	 * @param PortInfo UE port information structure
	 * @return libremidi input port structure
	 */
	inline libremidi::input_port ToLibremidiInputPort(const FLibremidiPortInfo& PortInfo)
	{
		libremidi::input_port Port;
		Port.client = static_cast<std::uintptr_t>(PortInfo.ClientHandle);
		Port.port = static_cast<uint32_t>(PortInfo.PortHandle);
		
		// Convert to std::string to ensure proper lifetime
		Port.display_name = std::string(TCHAR_TO_UTF8(*PortInfo.DisplayName));
		Port.port_name = std::string(TCHAR_TO_UTF8(*PortInfo.PortName));
		Port.device_name = std::string(TCHAR_TO_UTF8(*PortInfo.DeviceName));
		Port.manufacturer = std::string(TCHAR_TO_UTF8(*PortInfo.Manufacturer));
		
		// Set device identifier - required by Windows MIDI Services backend
		SetDeviceIdentifier(Port, PortInfo);
		
		return Port;
	}

	/**
	 * Convert FMidiPortInfo to libremidi::output_port
	 * @param PortInfo UE port information structure
	 * @return libremidi output port structure
	 */
	inline libremidi::output_port ToLibremidiOutputPort(const FLibremidiPortInfo& PortInfo)
	{
		libremidi::output_port Port;
		Port.client = static_cast<std::uintptr_t>(PortInfo.ClientHandle);
		Port.port = static_cast<uint32_t>(PortInfo.PortHandle);
		
		// Convert to std::string to ensure proper lifetime
		Port.display_name = std::string(TCHAR_TO_UTF8(*PortInfo.DisplayName));
		Port.port_name = std::string(TCHAR_TO_UTF8(*PortInfo.PortName));
		Port.device_name = std::string(TCHAR_TO_UTF8(*PortInfo.DeviceName));
		Port.manufacturer = std::string(TCHAR_TO_UTF8(*PortInfo.Manufacturer));
		
		// Set device identifier - required by Windows MIDI Services backend
		SetDeviceIdentifier(Port, PortInfo);
		
		return Port;
	}

	/**
	 * Log backend type information (native MIDI 2.0 or MIDI 1.0 with auto-conversion)
	 * @param CurrentAPI The libremidi API being used
	 * @param bIsInput True for input, false for output
	 */
	inline void LogBackendType(libremidi::API CurrentAPI, bool bIsInput)
	{
		const bool bIsNativeMidi2 = libremidi::is_midi2(CurrentAPI);
		const TCHAR* Direction = bIsInput ? TEXT("Input") : TEXT("Output");
		
		if (bIsNativeMidi2)
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("MIDI %s: Using native MIDI 2.0 (UMP) backend"), Direction);
		}
		else
		{
			UE_LOG(LogLibremidi4UE, Log, TEXT("MIDI %s: Using MIDI 1.0 backend with automatic UMP conversion"), Direction);
			if (bIsInput)
			{
				UE_LOG(LogLibremidi4UE, Log, TEXT("  -> 7-bit data will be upconverted to 16-bit/32-bit"));
			}
			else
			{
				UE_LOG(LogLibremidi4UE, Log, TEXT("  -> High-resolution data will be downsampled to 7-bit/14-bit"));
			}
		}
	}

	/**
	 * Check if a libremidi port is already open
	 * @param Port The libremidi port to check
	 * @param PortTypeName Display name for logging (e.g., "Input", "Output")
	 * @return True if port is open
	 */
	template<typename TPort>
	inline bool IsPortAlreadyOpen(const TPort* Port, const TCHAR* PortTypeName)
	{
		if (Port && Port->is_port_open())
		{
			UE_LOG(LogLibremidi4UE, Warning, TEXT("Midi%s: A port is already open. Close it first."), PortTypeName);
			return true;
		}
		return false;
	}

	/**
	 * Handle error and dispatch to game thread for delegate broadcast
	 * @param Owner The owning UObject
	 * @param ErrorMessage Error message string
	 * @param OnErrorDelegate Delegate to broadcast on game thread
	 */
	template<typename TOwner, typename TDelegate>
	inline void DispatchErrorToGameThread(TOwner* Owner, const FString& ErrorMessage, TDelegate& OnErrorDelegate)
	{
		UE_LOG(LogLibremidi4UE, Error, TEXT("MIDI Error: %s"), *ErrorMessage);
		
		AsyncTask(ENamedThreads::GameThread, [Owner, ErrorMessage, &OnErrorDelegate]()
		{
			if (IsValid(Owner))
			{
				OnErrorDelegate.Broadcast(ErrorMessage);
			}
		});
	}

} // namespace UE::MIDI::Private
