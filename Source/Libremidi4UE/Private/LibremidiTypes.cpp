// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#include "LibremidiTypes.h"

#if PLATFORM_MAC
#include <CoreMIDI/CoreMIDI.h>
#endif

// ---------------------------------------------------------------------------
// FLibremidiPortInfo constructors
// ---------------------------------------------------------------------------

FLibremidiPortInfo::FLibremidiPortInfo(const libremidi::port_information& InPort)
	: CachedNative(InPort)
{
	SyncPropertiesFromNative();
}

FLibremidiPortInfo::FLibremidiPortInfo(libremidi::port_information&& InPort)
	: CachedNative(MoveTemp(InPort))
{
	SyncPropertiesFromNative();
}

// ---------------------------------------------------------------------------
// FLibremidiPortInfo::SyncPropertiesFromNative
// ---------------------------------------------------------------------------

void FLibremidiPortInfo::SyncPropertiesFromNative()
{
	Api = static_cast<int32>(CachedNative.api);
	ContainerId = FLibremidiContainerIdentifier::FromLibremidi(CachedNative.container);
	DeviceId = FLibremidiDeviceIdentifier::FromLibremidi(CachedNative.device);
	PortHandle = static_cast<int64>(CachedNative.port);
	ClientHandle = static_cast<int64>(CachedNative.client);
	Manufacturer = UTF8_TO_TCHAR(CachedNative.manufacturer.c_str());
	Product = UTF8_TO_TCHAR(CachedNative.product.c_str());
	Serial = UTF8_TO_TCHAR(CachedNative.serial.c_str());
	DeviceName = UTF8_TO_TCHAR(CachedNative.device_name.c_str());
	PortName = UTF8_TO_TCHAR(CachedNative.port_name.c_str());
	DisplayName = UTF8_TO_TCHAR(CachedNative.display_name.c_str());
	PortType = static_cast<ELibremidiPortType>(CachedNative.type);
	Ordinal = ComputeOrdinalFromNative(CachedNative);
}

// ---------------------------------------------------------------------------
// FLibremidiPortInfo::ToPortInformation
// ---------------------------------------------------------------------------

libremidi::port_information FLibremidiPortInfo::ToPortInformation() const
{
	libremidi::port_information Result;
	Result.api = static_cast<libremidi::API>(Api);
	Result.container = ContainerId.ToLibremidi();
	Result.device = DeviceId.ToLibremidi();
	Result.port = static_cast<libremidi::port_handle>(PortHandle);
	Result.client = static_cast<libremidi::client_handle>(ClientHandle);
	Result.manufacturer = TCHAR_TO_UTF8(*Manufacturer);
	Result.product = TCHAR_TO_UTF8(*Product);
	Result.serial = TCHAR_TO_UTF8(*Serial);
	Result.device_name = TCHAR_TO_UTF8(*DeviceName);
	Result.port_name = TCHAR_TO_UTF8(*PortName);
	Result.display_name = TCHAR_TO_UTF8(*DisplayName);
	Result.type = static_cast<libremidi::transport_type>(PortType);
	return Result;
}

// ---------------------------------------------------------------------------
// FLibremidiPortInfo::FindClosestPort
// ---------------------------------------------------------------------------

FLibremidiPortInfo::FMatchResult FLibremidiPortInfo::FindClosestPort(
	TArrayView<const FLibremidiPortInfo> Candidates) const
{
	if (Candidates.IsEmpty())
	{
		return {};
	}

	// Build native array from candidates.
	// Use CachedNative for live ports (non-default api), ToPortInformation() for deserialized.
	TArray<libremidi::port_information> NativeCandidates;
	NativeCandidates.Reserve(Candidates.Num());
	for (const FLibremidiPortInfo& C : Candidates)
	{
		NativeCandidates.Add(
			C.CachedNative.api != libremidi::API::UNSPECIFIED
				? C.CachedNative
				: C.ToPortInformation());
	}

	const libremidi::port_information Target =
		CachedNative.api != libremidi::API::UNSPECIFIED
			? CachedNative
			: ToPortInformation();

	const auto Result = libremidi::find_closest_port(
		Target,
		std::span<const libremidi::port_information>{NativeCandidates.GetData(),
			static_cast<size_t>(NativeCandidates.Num())});

	if (!Result.found)
	{
		return {};
	}

	const int32 Index = static_cast<int32>(Result.port - NativeCandidates.GetData());
	return {Index, Result.score};
}

FLibremidiPortInfo::FMatchResult FLibremidiPortInfo::FindClosestPort(
	TArrayView<const FLibremidiInputInfo> Candidates) const
{
	// FLibremidiInputInfo layout-inherits FLibremidiPortInfo with no added fields,
	// so reinterpret is safe.
	return FindClosestPort(TArrayView<const FLibremidiPortInfo>{
		reinterpret_cast<const FLibremidiPortInfo*>(Candidates.GetData()),
		Candidates.Num()});
}

FLibremidiPortInfo::FMatchResult FLibremidiPortInfo::FindClosestPort(
	TArrayView<const FLibremidiOutputInfo> Candidates) const
{
	return FindClosestPort(TArrayView<const FLibremidiPortInfo>{
		reinterpret_cast<const FLibremidiPortInfo*>(Candidates.GetData()),
		Candidates.Num()});
}

// ---------------------------------------------------------------------------
// FLibremidiPortInfo::ComputeOrdinalFromNative
// ---------------------------------------------------------------------------
//
// Returns a deterministic per-device port ordinal based on the backend API.
// This ordinal represents the physical position of a port within its parent
// device, suitable for stable merge ordering.
//
// Per-backend strategy:
//   CoreMIDI:     Reverse-lookup endpoint → device, then flat ordinal
//                 across all entities (handles multi-entity devices like
//                 Launchpad Pro MK3: 3 entities × 1 port each)
//   WinMIDI:      MidiGroupTerminalBlock::Number() (1-based, hardware-defined)
//   ALSA Raw:     Subdevice number extracted from packed port handle
//   ALSA Seq:     Seq port number extracted from packed port handle
//   Others:       Fall back to raw port handle value

int32 FLibremidiPortInfo::ComputeOrdinalFromNative(const libremidi::port_information& Port)
{
	switch (Port.api)
	{
#if PLATFORM_MAC
	case COREMIDI:
	case COREMIDI_UMP:
	{
		// PortHandle is kMIDIPropertyUniqueID (SInt32 bit-cast to uint32).
		// Reverse-lookup the MIDIEndpointRef, determine direction from ObjType,
		// then walk the entire MIDIDevice to compute a flat ordinal across all
		// entities. This correctly handles both multi-entity devices (e.g.
		// Launchpad Pro MK3: 3 entities × 1 source/destination each) and
		// multi-port entities (1 entity × N sources/destinations).
		MIDIUniqueID UniqueId = static_cast<MIDIUniqueID>(static_cast<uint32_t>(Port.port));

		MIDIObjectRef Obj{};
		MIDIObjectType ObjType{};
		OSStatus Err = MIDIObjectFindByUniqueID(UniqueId, &Obj, &ObjType);
		if (Err != noErr)
		{
			return 0;
		}

		const bool bIsSource = (ObjType == kMIDIObjectType_Source
			|| ObjType == kMIDIObjectType_ExternalSource);

		// Walk up: endpoint → entity → device
		MIDIEntityRef Entity{};
		MIDIEndpointGetEntity(Obj, &Entity);
		if (!Entity)
		{
			return 0;
		}

		MIDIDeviceRef Device{};
		MIDIEntityGetDevice(Entity, &Device);
		if (!Device)
		{
			// Orphan entity (no parent device) — fall back to index within entity
			ItemCount Count = bIsSource
				? MIDIEntityGetNumberOfSources(Entity)
				: MIDIEntityGetNumberOfDestinations(Entity);
			for (ItemCount i = 0; i < Count; ++i)
			{
				MIDIEndpointRef Ref = bIsSource
					? MIDIEntityGetSource(Entity, i)
					: MIDIEntityGetDestination(Entity, i);
				if (Ref == Obj)
				{
					return static_cast<int32>(i);
				}
			}
			return 0;
		}

		// Iterate all entities in the device, accumulating a flat ordinal
		// across all sources (or destinations) to get a global position.
		int32 ComputedOrdinal = 0;
		ItemCount EntityCount = MIDIDeviceGetNumberOfEntities(Device);
		for (ItemCount e = 0; e < EntityCount; ++e)
		{
			MIDIEntityRef E = MIDIDeviceGetEntity(Device, e);
			ItemCount EndpointCount = bIsSource
				? MIDIEntityGetNumberOfSources(E)
				: MIDIEntityGetNumberOfDestinations(E);
			for (ItemCount p = 0; p < EndpointCount; ++p)
			{
				MIDIEndpointRef Ref = bIsSource
					? MIDIEntityGetSource(E, p)
					: MIDIEntityGetDestination(E, p);
				if (Ref == Obj)
				{
					return ComputedOrdinal;
				}
				++ComputedOrdinal;
			}
		}
		return 0;
	}
#endif

	case WINDOWS_MIDI_SERVICES:
		// MidiGroupTerminalBlock::Number() — 1-based terminal block ordinal
		return static_cast<int32>(Port.port);

	case ALSA_RAW:
	case ALSA_RAW_UMP:
		// Packed as: (sub << 32) | (dev << 16) | card
		// Extract subdevice number (the per-device ordinal)
		return static_cast<int32>(Port.port >> 32);

	case ALSA_SEQ:
	case ALSA_SEQ_UMP:
		// Packed as: (port << 32) | client
		// Extract seq port number (the per-client ordinal)
		return static_cast<int32>(Port.port >> 32);

	default:
		// WinMM, JACK, PipeWire, etc.: raw port handle as best-effort
		return static_cast<int32>(Port.port);
	}
}
