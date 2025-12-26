#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LibremidiTypes.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

#include "LibremidiOutput.generated.h"

class ULibremidiEngineSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMidiOutputError, const FString&, ErrorMessage);
/**
 * Delegate fired when MIDI output port is successfully opened
 * @param PortInfo Information about the opened port
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMidiOutputPortOpened, const FLibremidiPortInfo&, PortInfo);

/**
 * MIDI Output handler for sending MIDI 1.0 and MIDI 2.0 (UMP) messages.
 * 
 * ## Unified High-Resolution Processing
 * This class ALWAYS sends MIDI data as high-resolution UMP (MIDI 2.0) format internally.
 * libremidi automatically handles format conversion:
 * 
 * - **MIDI 1.0 port**: High-resolution data is automatically downsampled (16-bit ? 7-bit)
 * - **MIDI 2.0 port**: Native high-resolution data is preserved
 * 
 * ## Data Normalization (Input)
 * This class accepts normalized float values and converts them to high-resolution MIDI:
 * - **Velocity / CC / Pressure**: float 0.0-1.0 ? 16-bit (0-65535) or 32-bit (0-4.2B)
 * - **Pitch Bend**: float -1.0-1.0 ? 32-bit signed (-2.1B to 2.1B)
 * 
 * ## Automatic Format Conversion
 * Thanks to libremidi's automatic conversion:
 * - MIDI 1.0 ports automatically downsample from high-resolution UMP
 * - MIDI 2.0 ports preserve full high-resolution data
 * - Your code always sends consistent normalized float values
 * 
 * ## Thread Safety
 * Send operations are thread-safe and can be called from any thread.
 * Error broadcasts are dispatched to the game thread via AsyncTask.
 * 
 * ## Usage Example
 * ```cpp
 * // C++
 * ULibremidiOutput* Output = ULibremidiOutput::CreateMidiOutput(this);
 * Output->OpenPort(PortInfo);  // Works with both MIDI 1.0 and 2.0 ports
 * 
 * // Always send high-resolution data - automatic conversion happens internally
 * Output->SendNoteOn(0, 60, 0.8f);  // Channel 0, Note 60, Velocity 0.8
 * Output->SendPitchBend(0, 0.5f);   // Channel 0, Pitch Bend +0.5
 * ```
 * 
 * @see ULibremidiEngineSubsystem for port enumeration and global MIDI API selection
 * @see ULibremidiInput for receiving MIDI messages with automatic normalization
 */
UCLASS(BlueprintType, Category = "MIDI")
class LIBREMIDI4UE_API ULibremidiOutput : public UObject
{
	GENERATED_BODY()

public:
	ULibremidiOutput(const FObjectInitializer& ObjectInitializer);
	virtual void BeginDestroy() override;

	/** Error Event */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events")
	FOnMidiOutputError OnError;

	/**
	 * Create a new MIDI Output instance
	 * @param WorldContextObject Context object (typically 'this' from the caller)
	 * @return New MIDI Output instance
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Create MIDI Output", WorldContext = "WorldContextObject"))
	static ULibremidiOutput* CreateMidiOutput(UObject* WorldContextObject);

	/**
	 * Open a hardware MIDI output port.
	 * Port type is automatically detected and data is always sent as UMP internally.
	 * @param PortInfo Port information obtained from LibremidiEngineSubsystem
	 * @param ClientName Optional client name visible to other MIDI applications
	 * @return True if port opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Port"))
	bool OpenPort(const FLibremidiPortInfo& PortInfo, const FString& ClientName = TEXT("Unreal Engine MIDI Output"));

	/**
	 * Create and open a virtual MIDI output port.
	 * Data is always sent as UMP internally.
	 * @param PortName Name of the virtual port
	 * @return True if virtual port created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Virtual Port"))
	bool OpenVirtualPort(const FString& PortName);

	/**
	 * Close the currently open port
	 * @return True if the port was closed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Close Port"))
	bool ClosePort();

	/**
	 * Send a MIDI message (MIDI 1.0 format) - C++ only
	 * Automatically converted to UMP and then to appropriate port format
	 * @param Data The MIDI message bytes to send
	 * @return True if the message was sent successfully
	 */
	bool SendMessage(const TArray<uint8>& Data);

	/**
	 * Send UMP message (MIDI 2.0) - C++ only
	 * @param Data UMP data (1-4 words)
	 * @return True if the message was sent successfully
	 */
	bool SendMessageUMP(const TArray<uint32>& Data);

	/**
	 * Check if a port is currently open
	 * @return True if a port is open
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI", meta = (DisplayName = "Is Port Open"))
	bool IsPortOpen() const;

	/**
	 * Check if the port is connected
	 * @return True if the port is connected
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI", meta = (DisplayName = "Is Port Connected"))
	bool IsPortConnected() const;

	/**
	 * Get the current MIDI API being used
	 * @return The current API
	 */
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
	FOnMidiOutputPortOpened OnPortOpened;

	// ============================================================================
	// High-Level MIDI Message Sending (Normalized Float Values)
	// ============================================================================

	/**
	 * Send Note On message.
	 * @param Channel MIDI channel (0-15)
	 * @param Note MIDI note number (0-127)
	 * @param Velocity Normalized velocity (0.0 = silent, 1.0 = maximum)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send Note On"))
	bool SendNoteOn(int32 Channel, int32 Note, float Velocity);

	/**
	 * Send Note Off message.
	 * @param Channel MIDI channel (0-15)
	 * @param Note MIDI note number (0-127)
	 * @param Velocity Normalized release velocity (0.0-1.0)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send Note Off"))
	bool SendNoteOff(int32 Channel, int32 Note, float Velocity);

	/**
	 * Send Control Change message.
	 * @param Channel MIDI channel (0-15)
	 * @param Controller Controller number (0-127)
	 * @param Value Normalized control value (0.0-1.0)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send Control Change"))
	bool SendControlChange(int32 Channel, int32 Controller, float Value);

	/**
	 * Send Program Change message.
	 * @param Channel MIDI channel (0-15)
	 * @param Program Program number (0-127)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send Program Change"))
	bool SendProgramChange(int32 Channel, int32 Program);

	/**
	 * Send Pitch Bend message.
	 * @param Channel MIDI channel (0-15)
	 * @param Value Normalized pitch bend (-1.0 = down, 0.0 = center, 1.0 = up)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send Pitch Bend"))
	bool SendPitchBend(int32 Channel, float Value);

	/**
	 * Send Channel Pressure (Aftertouch) message.
	 * @param Channel MIDI channel (0-15)
	 * @param Pressure Normalized pressure (0.0-1.0)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send Channel Pressure"))
	bool SendChannelPressure(int32 Channel, float Pressure);

	/**
	 * Send Polyphonic Key Pressure message.
	 * @param Channel MIDI channel (0-15)
	 * @param Note MIDI note number (0-127)
	 * @param Pressure Normalized pressure (0.0-1.0)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send Poly Pressure"))
	bool SendPolyPressure(int32 Channel, int32 Note, float Pressure);

	/**
	 * Send All Notes Off message (CC 123).
	 * @param Channel MIDI channel (0-15)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send All Notes Off"))
	bool SendAllNotesOff(int32 Channel);

	/**
	 * Send All Sound Off message (CC 120).
	 * @param Channel MIDI channel (0-15)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send All Sound Off"))
	bool SendAllSoundOff(int32 Channel);

	/**
	 * Send Reset All Controllers message (CC 121).
	 * @param Channel MIDI channel (0-15)
	 * @return True if sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Send", meta = (DisplayName = "Send Reset All Controllers"))
	bool SendResetAllControllers(int32 Channel);

private:
	TUniquePtr<libremidi::midi_out> MidiOut;
	FLibremidiPortInfo CurrentPortInfo;
	bool bIsVirtualPort = false;

	// Auto-reconnection state
	bool bEnableAutoReconnect = false;
	FString LastClientName;
	FLibremidiPortInfo LastDisconnectedPort;
	FDelegateHandle HotPlugDelegateHandle;

	ULibremidiEngineSubsystem* GetSubsystem() const;
	libremidi::output_port ConvertPortInfo(const FLibremidiPortInfo& PortInfo) const;
	bool CreateMidiOut(libremidi::API API);

	libremidi::output_configuration CreateOutputConfiguration();
	void HandleError(const FString& ErrorMessage);

	void NotifyPortOpened(const FLibremidiPortInfo& PortInfo, bool bVirtual);
	void NotifyPortClosed();
	
	// Auto-reconnection
	void RegisterHotPlugDelegate();
	void UnregisterHotPlugDelegate();
	void HandleHotPlugEvent(const FLibremidiPortInfo& ReconnectedPort);
};
