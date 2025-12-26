#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "LibremidiTypes.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

#include "LibremidiEngineSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMidiInputDeviceChanged, const FLibremidiPortInfo&, PortInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMidiOutputDeviceChanged, const FLibremidiPortInfo&, PortInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMidiError, const FString&, ErrorMessage, const FString&, FileName, int32, LineNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMidiWarning, const FString&, WarningMessage, const FString&, FileName, int32, LineNumber);

// ========================================================================
// Hot-plug delegates for auto-reconnection
// ========================================================================

/**
 * Delegate for input port hot-plug events (for auto-reconnection)
 * @param PortInfo Information about the reconnected port
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInputPortConnected, const FLibremidiPortInfo&);

/**
 * Delegate for output port hot-plug events (for auto-reconnection)
 * @param PortInfo Information about the reconnected port
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOutputPortConnected, const FLibremidiPortInfo&);

UCLASS()
class LIBREMIDI4UE_API ULibremidiEngineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events")
	FOnMidiInputDeviceChanged OnInputDeviceAdded;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events")
	FOnMidiInputDeviceChanged OnInputDeviceRemoved;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events")
	FOnMidiOutputDeviceChanged OnOutputDeviceAdded;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events")
	FOnMidiOutputDeviceChanged OnOutputDeviceRemoved;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events")
	FOnMidiError OnError;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events")
	FOnMidiWarning OnWarning;

	// ========================================================================
	// Hot-plug Events (for auto-reconnection)
	// ========================================================================

	/**
	 * Native delegate for input port reconnection (not exposed to Blueprint)
	 * Used by ULibremidiInput instances for auto-reconnection logic
	 */
	FOnInputPortConnected OnInputPortConnected;

	/**
	 * Native delegate for output port reconnection (not exposed to Blueprint)
	 * Used by ULibremidiOutput instances for auto-reconnection logic
	 */
	FOnOutputPortConnected OnOutputPortReconnected;

	UFUNCTION(BlueprintCallable, Category = "MIDI|Configuration", meta = (DisplayName = "Set MIDI API", ToolTip = "Set the MIDI API to use. Observer will be restarted."))
	void SetMidiAPI(ELibremidiAPI API);

	UFUNCTION(BlueprintCallable, Category = "MIDI|Configuration", meta = (DisplayName = "Set Track Hardware Devices", ToolTip = "Enable/disable tracking of hardware MIDI devices. Observer will be restarted."))
	void SetTrackHardware(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "MIDI|Configuration", meta = (DisplayName = "Set Track Virtual Devices", ToolTip = "Enable/disable tracking of virtual MIDI devices. Observer will be restarted."))
	void SetTrackVirtual(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "MIDI|Configuration", meta = (DisplayName = "Set Track Any Devices", ToolTip = "Enable/disable tracking of any MIDI devices. Observer will be restarted."))
	void SetTrackAny(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "MIDI|Configuration", meta = (DisplayName = "Get MIDI API"))
	ELibremidiAPI GetMidiAPI() const { return MidiAPI; }

	UFUNCTION(BlueprintPure, Category = "MIDI|Configuration", meta = (DisplayName = "Get Track Hardware Devices"))
	bool GetTrackHardware() const { return bTrackHardware; }

	UFUNCTION(BlueprintPure, Category = "MIDI|Configuration", meta = (DisplayName = "Get Track Virtual Devices"))
	bool GetTrackVirtual() const { return bTrackVirtual; }

	UFUNCTION(BlueprintPure, Category = "MIDI|Configuration", meta = (DisplayName = "Get Track Any Devices"))
	bool GetTrackAny() const { return bTrackAny; }

	UFUNCTION(BlueprintPure, Category = "MIDI|Configuration", meta = (DisplayName = "Get Current API"))
	ELibremidiAPI GetCurrentAPI() const;

	UFUNCTION(BlueprintCallable, Category = "MIDI|Ports", meta = (DisplayName = "Get Available Input Ports"))
	TArray<FLibremidiPortInfo> GetAvailableInputPorts() const;

	UFUNCTION(BlueprintCallable, Category = "MIDI|Ports", meta = (DisplayName = "Get Available Output Ports"))
	TArray<FLibremidiPortInfo> GetAvailableOutputPorts() const;

	// ============================================================================
	// Port Management
	// ============================================================================

	/**
	 * Get all active input ports
	 * @return Array of active input ports
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Ports", meta = (DisplayName = "Get Active Input Ports"))
	TArray<ULibremidiInput*> GetActiveInputPorts() const;

	/**
	 * Get all active output ports
	 * @return Array of active output ports
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Ports", meta = (DisplayName = "Get Active Output Ports"))
	TArray<ULibremidiOutput*> GetActiveOutputPorts() const;

	/**
	 * Find an active input port by port info
	 * @param PortInfo The port info to search for
	 * @return The active input port, or nullptr if not found
	 */
	ULibremidiInput* FindActiveInputPort(const FLibremidiPortInfo& PortInfo) const;

	/**
	 * Find an active output port by port info
	 * @param PortInfo The port info to search for
	 * @return The active output port, or nullptr if not found
	 */
	ULibremidiOutput* FindActiveOutputPort(const FLibremidiPortInfo& PortInfo) const;

private:
	TUniquePtr<libremidi::observer> Observer;

	ELibremidiAPI MidiAPI = ELibremidiAPI::Unspecified;
	bool bTrackHardware = true;
	bool bTrackVirtual = true;
	bool bTrackAny = false;
	bool bLoggedInitialDevices = false;

	void StartObserver();
	void StopObserver();
	void RestartObserver();
	
	void LogAvailableAPIs() const;
	void LogInitialDevices();
	
	libremidi::observer_configuration CreateObserverConfiguration() const;
	libremidi::observer_api_configuration CreateObserverAPIConfiguration() const;

	void HandleError(std::string_view ErrorText, const libremidi::source_location& Location) const;
	void HandleWarning(std::string_view WarningText, const libremidi::source_location& Location) const;
	void HandleInputDeviceAdded(const libremidi::input_port& Port) const;
	void HandleInputDeviceRemoved(const libremidi::input_port& Port) const;
	void HandleOutputDeviceAdded(const libremidi::output_port& Port) const;
	void HandleOutputDeviceRemoved(const libremidi::output_port& Port) const;

	// ============================================================================
	// Internal Port Management
	// ============================================================================

	/**
	 * Register an input port (called by ULibremidiInput)
	 * @param Port The input port to register
	 */
	void RegisterInputPort(ULibremidiInput* Port);

	/**
	 * Unregister an input port (called by ULibremidiInput)
	 * @param Port The input port to unregister
	 */
	void UnregisterInputPort(ULibremidiInput* Port);

	/**
	 * Register an output port (called by ULibremidiOutput)
	 * @param Port The output port to register
	 */
	void RegisterOutputPort(ULibremidiOutput* Port);

	/**
	 * Unregister an output port (called by ULibremidiOutput)
	 * @param Port The output port to unregister
	 */
	void UnregisterOutputPort(ULibremidiOutput* Port);

	// Active ports (managed by GC via UPROPERTY)
	UPROPERTY()
	TArray<TObjectPtr<ULibremidiInput>> ActiveInputPorts;

	UPROPERTY()
	TArray<TObjectPtr<ULibremidiOutput>> ActiveOutputPorts;

	// Friend classes for internal access
	friend class ULibremidiInput;
	friend class ULibremidiOutput;
};
