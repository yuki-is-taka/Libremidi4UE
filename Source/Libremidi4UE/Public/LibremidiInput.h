#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LibremidiTypes.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

#include "LibremidiInput.generated.h"

class ULibremidiEngineSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMidiMessageUMP, const TArray<uint32>&, Data, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMidiInputError, const FString&, ErrorMessage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnLibremidiNoteOn, ULibremidiInput*, Source, int32, Channel, int32, Note, float, Velocity, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnLibremidiNoteOff, ULibremidiInput*, Source, int32, Channel, int32, Note, float, Velocity, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnLibremidiControlChange, ULibremidiInput*, Source, int32, Channel, int32, Controller, float, Value, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnLibremidiProgramChange, ULibremidiInput*, Source, int32, Channel, int32, Program, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnLibremidiPitchBend, ULibremidiInput*, Source, int32, Channel, float, Value, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnLibremidiChannelPressure, ULibremidiInput*, Source, int32, Channel, float, Pressure, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnLibremidiPolyPressure, ULibremidiInput*, Source, int32, Channel, int32, Note, float, Pressure, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLibremidiSysEx, ULibremidiInput*, Source, const TArray<uint8>&, Data, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLibremidiSongPosition, ULibremidiInput*, Source, int32, Beats, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLibremidiSongSelect, ULibremidiInput*, Source, int32, Song, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLibremidiClock, ULibremidiInput*, Source, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLibremidiStart, ULibremidiInput*, Source, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLibremidiContinue, ULibremidiInput*, Source, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLibremidiStop, ULibremidiInput*, Source, int64, Timestamp);

/**
 * Delegate fired when port is successfully opened
 * @param PortInfo Information about the opened port
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPortOpened, const FLibremidiPortInfo&, PortInfo);

/**
 * MIDI Input handler for receiving MIDI 1.0 and MIDI 2.0 (UMP) messages.
 * 
 * ## Unified High-Resolution Processing
 * This class ALWAYS processes MIDI data as high-resolution UMP (MIDI 2.0) format internally,
 * regardless of the port type. libremidi automatically handles format conversion:
 * 
 * - **MIDI 1.0 port**: 7-bit data is automatically upconverted to 16-bit/32-bit
 * - **MIDI 2.0 port**: Native high-resolution data is preserved
 * 
 * ## Data Normalization (Output)
 * All continuous MIDI values are normalized to float for consistent Blueprint/C++ handling:
 * - **Velocity**: 16-bit (0-65535) ? float 0.0-1.0
 * - **Control Change**: 32-bit (0-4.2B) ? float 0.0-1.0
 * - **Pressure**: 32-bit (0-4.2B) ? float 0.0-1.0
 * - **Pitch Bend**: 32-bit signed ? float -1.0 to 1.0
 * 
 * ## Automatic Format Conversion
 * Thanks to libremidi's automatic conversion:
 * - MIDI 1.0 ports automatically upconvert to high-resolution UMP
 * - MIDI 2.0 ports provide native high-resolution data
 * - Your code always receives consistent normalized float values
 * 
 * ## Thread Safety
 * All delegate broadcasts are automatically dispatched to the game thread via AsyncTask.
 * MIDI messages are received on a background thread and safely marshaled to Blueprint.
 * 
 * ## SysEx Handling
 * - Multi-packet SysEx is automatically buffered and concatenated
 * - F0/F7 framing bytes are automatically stripped
 * 
 * ## Usage Example
 * ```cpp
 * // C++
 * ULibremidiInput* Input = ULibremidiInput::CreateMidiInput(this);
 * Input->OnNoteOn.AddDynamic(this, &AMyActor::HandleNoteOn);
 * Input->OpenPort(PortInfo);  // Works with both MIDI 1.0 and 2.0 ports
 * 
 * void AMyActor::HandleNoteOn(ULibremidiInput* Source, int32 Channel, int32 Note, float Velocity, int64 Timestamp)
 * {
 *     // Velocity is always normalized 0.0-1.0, regardless of port type
 *     float Volume = Velocity * MaxVolume;
 * }
 * ```
 * 
 * @see ULibremidiEngineSubsystem for port enumeration and global MIDI API selection
 * @see ULibremidiOutput for sending MIDI messages with automatic normalization
 */
UCLASS(BlueprintType, Category = "MIDI")
class LIBREMIDI4UE_API ULibremidiInput : public UObject
{
	GENERATED_BODY()

public:
	ULibremidiInput(const FObjectInitializer& ObjectInitializer);
	virtual void BeginDestroy() override;

	// ============================================================================
	// Raw MIDI Message Events
	// ============================================================================

	/**
	 * Raw MIDI 2.0 (UMP) message event (32-bit data).
	 * Provides unprocessed UMP stream for custom parsing or logging.
	 * For most use cases, prefer the typed events (OnNoteOn, OnControlChange, etc.).
	 */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Raw", meta = (BlueprintInternalUseOnly = "true"))
	FOnMidiMessageUMP OnMidiMessageUMP;

	/** MIDI input error event */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|System")
	FOnMidiInputError OnError;

	// ============================================================================
	// Typed MIDI Message Events (Normalized Values)
	// ============================================================================

	/**
	 * Note On event.
	 * @param Velocity Normalized velocity (0.0 = silent, 1.0 = maximum)
	 */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Note")
	FOnLibremidiNoteOn OnNoteOn;

	/**
	 * Note Off event.
	 * @param Velocity Normalized release velocity (0.0 to 1.0)
	 */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Note")
	FOnLibremidiNoteOff OnNoteOff;

	/**
	 * Control Change event.
	 * @param Value Normalized control value (0.0 to 1.0)
	 */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Control")
	FOnLibremidiControlChange OnControlChange;

	/** Program Change event (instrument/patch selection) */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Control")
	FOnLibremidiProgramChange OnProgramChange;

	/**
	 * Pitch Bend event.
	 * @param Value Normalized pitch bend (-1.0 = down, 0.0 = center, 1.0 = up)
	 */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Control")
	FOnLibremidiPitchBend OnPitchBend;

	/**
	 * Channel Pressure (Aftertouch) event.
	 * @param Pressure Normalized pressure (0.0 to 1.0)
	 */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Control")
	FOnLibremidiChannelPressure OnChannelPressure;

	/**
	 * Polyphonic Key Pressure (Per-note aftertouch) event.
	 * @param Pressure Normalized pressure (0.0 to 1.0)
	 */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Control")
	FOnLibremidiPolyPressure OnPolyPressure;

	/**
	 * System Exclusive event.
	 * F0/F7 framing bytes are automatically removed.
	 */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|System")
	FOnLibremidiSysEx OnSysEx;

	/** Song Position Pointer event (MIDI synchronization) */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|System")
	FOnLibremidiSongPosition OnSongPosition;

	/** Song Select event (MIDI synchronization) */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|System")
	FOnLibremidiSongSelect OnSongSelect;

	/** MIDI Clock event (timing synchronization, 24 ppqn) */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Timing")
	FOnLibremidiClock OnMidiClock;

	/** MIDI Start event (begin playback) */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Timing")
	FOnLibremidiStart OnMidiStart;

	/** MIDI Continue event (resume playback) */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Timing")
	FOnLibremidiContinue OnMidiContinue;

	/** MIDI Stop event (halt playback) */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|Timing")
	FOnLibremidiStop OnMidiStop;

	// ============================================================================
	// Port Management
	// ============================================================================

	/**
	 * Create a new MIDI Input instance.
	 * @param WorldContextObject Context object (typically 'this' from the caller)
	 * @return New MIDI Input instance, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Create MIDI Input", WorldContext = "WorldContextObject"))
	static ULibremidiInput* CreateMidiInput(UObject* WorldContextObject);

	/**
	 * Open a hardware MIDI input port.
	 * Port type is automatically detected and data is always processed as UMP internally.
	 * @param PortInfo Port information obtained from LibremidiEngineSubsystem
	 * @param ClientName Optional client name visible to other MIDI applications
	 * @return True if port opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Port"))
	bool OpenPort(const FLibremidiPortInfo& PortInfo, const FString& ClientName = TEXT("Unreal Engine MIDI Input"));

	/**
	 * Create and open a virtual MIDI input port.
	 * Data is always processed as UMP internally.
	 * @param PortName Name of the virtual port
	 * @return True if virtual port created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Virtual Port"))
	bool OpenVirtualPort(const FString& PortName);

	/**
	 * Close the currently open port.
	 * @return True if port was closed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Close Port"))
	bool ClosePort();

	// ============================================================================
	// Port Status
	// ============================================================================

	/** Check if a port is currently open */
	UFUNCTION(BlueprintPure, Category = "MIDI", meta = (DisplayName = "Is Port Open"))
	bool IsPortOpen() const;

	/** Check if the port is connected (hardware may be disconnected even if port is open) */
	UFUNCTION(BlueprintPure, Category = "MIDI", meta = (DisplayName = "Is Port Connected"))
	bool IsPortConnected() const;

	/** Get the current MIDI API being used (Windows: WinMM/MIDI2, Mac: CoreMIDI, Linux: ALSA/JACK) */
	UFUNCTION(BlueprintPure, Category = "MIDI", meta = (DisplayName = "Get Current API"))
	ELibremidiAPI GetCurrentAPI() const;

	/** Get information about the currently open port */
	const FLibremidiPortInfo& GetCurrentPortInfo() const { return CurrentPortInfo; }

	// ============================================================================
	// Auto-Reconnection
	// ============================================================================

	/**
	 * Enable or disable automatic reconnection when device is hot-plugged
	 * @param bEnable True to enable auto-reconnection
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Auto-Reconnection", meta = (DisplayName = "Set Auto Reconnect"))
	void SetAutoReconnect(bool bEnable);

	/**
	 * Check if auto-reconnection is enabled
	 * @return True if auto-reconnection is enabled
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Auto-Reconnection", meta = (DisplayName = "Is Auto Reconnect Enabled"))
	bool IsAutoReconnectEnabled() const { return bEnableAutoReconnect; }

	/** Delegate fired when port is successfully opened */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events|System")
	FOnPortOpened OnPortOpened;

private:
	TUniquePtr<libremidi::midi_in> MidiIn;
	FLibremidiPortInfo CurrentPortInfo;
	bool bIsVirtualPort = false;

	// Auto-reconnection state
	bool bEnableAutoReconnect = false;
	FString LastClientName;
	FLibremidiPortInfo LastDisconnectedPort;
	FDelegateHandle HotPlugDelegateHandle;

	TArray<uint8> UMPSysExBuffer;
	bool bUMPSysExInProgress = false;
	int64 UMPSysExStartTimestamp = 0;

	ULibremidiEngineSubsystem* GetSubsystem() const;
	libremidi::input_port ConvertPortInfo(const FLibremidiPortInfo& PortInfo) const;
	bool CreateMidiIn(libremidi::API API);

	libremidi::ump_input_configuration CreateInputConfigurationUMP();
	
	void OnUMPMessageReceived(libremidi::ump&& Message);
	void HandleError(const FString& ErrorMessage);

	void NotifyPortOpened(const FLibremidiPortInfo& PortInfo, bool bVirtual);
	void NotifyPortClosed();

	// Auto-reconnection
	void RegisterHotPlugDelegate();
	void UnregisterHotPlugDelegate();
	void HandleHotPlugEvent(const FLibremidiPortInfo& ReconnectedPort);

	void ProcessUMP(const TArray<uint32>& Data, int64 Timestamp);
	void ProcessUMPSystem(const TArray<uint32>& Data, int64 Timestamp);
	void ProcessUMPMidi1(const TArray<uint32>& Data, int64 Timestamp);
	void ProcessUMPMidi2(const TArray<uint32>& Data, int64 Timestamp);
	void ProcessUMPSysEx(const TArray<uint32>& Data, int64 Timestamp);
};
