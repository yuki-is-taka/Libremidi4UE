#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LibremidiTypes.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

#include "LibremidiInput.generated.h"

class ULibremidiEngineSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMidiMessage, const TArray<uint8>&, Data, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMidiMessageUMP, const TArray<uint32>&, Data, int64, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMidiInputError, const FString&, ErrorMessage);

/**
 * MIDI Input class for receiving MIDI 1.0 and MIDI 2.0 (UMP) messages
 * Supports both hardware and virtual ports
 */
UCLASS(BlueprintType, Category = "MIDI")
class LIBREMIDI4UE_API ULibremidiInput : public UObject
{
	GENERATED_BODY()

public:
	ULibremidiInput();
	virtual ~ULibremidiInput();
	virtual void BeginDestroy() override;

	/** MIDI 1.0 Message Event */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events")
	FOnMidiMessage OnMidiMessage;

	/** MIDI 2.0 (UMP) Message Event - C++ only */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events", meta = (BlueprintInternalUseOnly = "true"))
	FOnMidiMessageUMP OnMidiMessageUMP;

	/** Error Event */
	UPROPERTY(BlueprintAssignable, Category = "MIDI|Events")
	FOnMidiInputError OnError;

	/**
	 * Create a new MIDI Input instance
	 * @param WorldContextObject Context object (typically 'this' from the caller)
	 * @return New MIDI Input instance
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Create MIDI Input", WorldContext = "WorldContextObject"))
	static ULibremidiInput* CreateMidiInput(UObject* WorldContextObject);

	/**
	 * Open a MIDI input port (MIDI 1.0)
	 * @param PortInfo The port to open
	 * @param ClientName Optional client name
	 * @return True if the port was opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Port (MIDI 1.0)"))
	bool OpenPort(const FMidiPortInfo& PortInfo, const FString& ClientName = TEXT("Unreal Engine MIDI Input"));

	/**
	 * Open a MIDI input port (MIDI 2.0 UMP)
	 * @param PortInfo The port to open
	 * @param ClientName Optional client name
	 * @return True if the port was opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Port (MIDI 2.0 UMP)"))
	bool OpenPortUMP(const FMidiPortInfo& PortInfo, const FString& ClientName = TEXT("Unreal Engine MIDI Input"));

	/**
	 * Open a virtual MIDI input port (MIDI 1.0)
	 * @param PortName The name for the virtual port
	 * @return True if the port was opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Virtual Port (MIDI 1.0)"))
	bool OpenVirtualPort(const FString& PortName);

	/**
	 * Open a virtual MIDI input port (MIDI 2.0 UMP)
	 * @param PortName The name for the virtual port
	 * @return True if the port was opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Virtual Port (MIDI 2.0 UMP)"))
	bool OpenVirtualPortUMP(const FString& PortName);

	/**
	 * Close the currently open port
	 * @return True if the port was closed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Close Port"))
	bool ClosePort();

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

	/**
	 * Check if this input is using MIDI 2.0 (UMP)
	 * @return True if using MIDI 2.0
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI", meta = (DisplayName = "Is MIDI 2.0"))
	bool IsMidi2() const { return bIsMidi2; }

private:
	TUniquePtr<libremidi::midi_in> MidiIn;
	bool bIsMidi2 = false;

	/** Get the MIDI Engine Subsystem */
	ULibremidiEngineSubsystem* GetSubsystem() const;
	
	/** Convert FMidiPortInfo to libremidi::input_port */
	libremidi::input_port ConvertPortInfo(const FMidiPortInfo& PortInfo) const;
	
	/** Create MIDI input instance (MIDI 1.0 or 2.0) */
	bool CreateMidiIn(bool bUseMidi2, libremidi::API API);
	
	/** Common port opening logic */
	bool OpenPortInternal(const FMidiPortInfo& PortInfo, const FString& ClientName, bool bUseMidi2);
	
	/** Common virtual port opening logic */
	bool OpenVirtualPortInternal(const FString& PortName, bool bUseMidi2);

	/** Create MIDI 1.0 input configuration */
	libremidi::input_configuration CreateInputConfiguration();
	
	/** Create MIDI 2.0 (UMP) input configuration */
	libremidi::ump_input_configuration CreateInputConfigurationUMP();
	
	/** Handle incoming MIDI 1.0 messages */
	void HandleMidiMessage(libremidi::message&& Message);
	
	/** Handle incoming MIDI 2.0 (UMP) messages */
	void HandleMidiMessageUMP(libremidi::ump&& Message);
	
	/** Handle errors */
	void HandleError(const FString& ErrorMessage);
};
