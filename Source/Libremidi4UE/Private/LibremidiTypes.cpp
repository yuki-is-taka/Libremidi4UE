#include "LibremidiTypes.h"

namespace LibremidiTypeConversion
{
	libremidi::API ToLibremidiAPI(ELibremidiAPI API)
	{
		switch (API)
		{
		case ELibremidiAPI::Unspecified: return libremidi::API::UNSPECIFIED;
		case ELibremidiAPI::Dummy: return libremidi::API::DUMMY;
		case ELibremidiAPI::AlsaSeq: return libremidi::API::ALSA_SEQ;
		case ELibremidiAPI::AlsaRaw: return libremidi::API::ALSA_RAW;
		case ELibremidiAPI::CoreMidi: return libremidi::API::COREMIDI;
		case ELibremidiAPI::WindowsMM: return libremidi::API::WINDOWS_MM;
		case ELibremidiAPI::WindowsUWP: return libremidi::API::WINDOWS_UWP;
		case ELibremidiAPI::Jack: return libremidi::API::JACK_MIDI;
		case ELibremidiAPI::PipeWire: return libremidi::API::PIPEWIRE;
		case ELibremidiAPI::Emscripten: return libremidi::API::WEBMIDI;
		case ELibremidiAPI::WindowsMidiServices: return libremidi::API::WINDOWS_MIDI_SERVICES;
		case ELibremidiAPI::AlsaSeqUMP: return libremidi::API::ALSA_SEQ_UMP;
		case ELibremidiAPI::AlsaRawUMP: return libremidi::API::ALSA_RAW_UMP;
		case ELibremidiAPI::CoreMidiUMP: return libremidi::API::COREMIDI_UMP;
		case ELibremidiAPI::JackUMP: return libremidi::API::JACK_UMP;
		case ELibremidiAPI::PipeWireUMP: return libremidi::API::PIPEWIRE_UMP;
		default: return libremidi::API::UNSPECIFIED;
		}
	}

	ELibremidiAPI FromLibremidiAPI(libremidi::API API)
	{
		switch (API)
		{
		case libremidi::API::UNSPECIFIED: return ELibremidiAPI::Unspecified;
		case libremidi::API::DUMMY: return ELibremidiAPI::Dummy;
		case libremidi::API::ALSA_SEQ: return ELibremidiAPI::AlsaSeq;
		case libremidi::API::ALSA_RAW: return ELibremidiAPI::AlsaRaw;
		case libremidi::API::COREMIDI: return ELibremidiAPI::CoreMidi;
		case libremidi::API::WINDOWS_MM: return ELibremidiAPI::WindowsMM;
		case libremidi::API::WINDOWS_UWP: return ELibremidiAPI::WindowsUWP;
		case libremidi::API::JACK_MIDI: return ELibremidiAPI::Jack;
		case libremidi::API::PIPEWIRE: return ELibremidiAPI::PipeWire;
		case libremidi::API::WEBMIDI: return ELibremidiAPI::Emscripten;
		case libremidi::API::WINDOWS_MIDI_SERVICES: return ELibremidiAPI::WindowsMidiServices;
		case libremidi::API::ALSA_SEQ_UMP: return ELibremidiAPI::AlsaSeqUMP;
		case libremidi::API::ALSA_RAW_UMP: return ELibremidiAPI::AlsaRawUMP;
		case libremidi::API::COREMIDI_UMP: return ELibremidiAPI::CoreMidiUMP;
		case libremidi::API::JACK_UMP: return ELibremidiAPI::JackUMP;
		case libremidi::API::PIPEWIRE_UMP: return ELibremidiAPI::PipeWireUMP;
		default: return ELibremidiAPI::Unspecified;
		}
	}
}
