// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#include "LibremidiTypes.h"

#if PLATFORM_MAC
#include <CoreMIDI/CoreMIDI.h>
#endif

// ---------------------------------------------------------------------------
// FLibremidiPortInfo::GetOrdinal
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

int32 FLibremidiPortInfo::GetOrdinal() const
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
		int32 Ordinal = 0;
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
					return Ordinal;
				}
				++Ordinal;
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
