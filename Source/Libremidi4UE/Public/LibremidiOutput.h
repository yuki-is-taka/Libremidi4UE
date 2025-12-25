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
 * MIDI Output class for sending MIDI 1.0 and MIDI 2.0 (UMP) messages
 * Supports both hardware and virtual ports
 */
UCLASS(BlueprintType, Category = "MIDI")
class LIBREMIDI4UE_API ULibremidiOutput : public UObject
{
	GENERATED_BODY()

public:
	ULibremidiOutput();
	virtual ~ULibremidiOutput();
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
	 * Open a MIDI output port (MIDI 1.0)
	 * @param PortInfo The port to open
	 * @param ClientName Optional client name
	 * @return True if the port was opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Port (MIDI 1.0)"))
	bool OpenPort(const FMidiPortInfo& PortInfo, const FString& ClientName = TEXT("Unreal Engine MIDI Output"));

	/**
	 * Open a MIDI output port (MIDI 2.0 UMP)
	 * @param PortInfo The port to open
	 * @param ClientName Optional client name
	 * @return True if the port was opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Port (MIDI 2.0 UMP)"))
	bool OpenPortUMP(const FMidiPortInfo& PortInfo, const FString& ClientName = TEXT("Unreal Engine MIDI Output"));

	/**
	 * Open a virtual MIDI output port (MIDI 1.0)
	 * @param PortName The name for the virtual port
	 * @return True if the port was opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Open Virtual Port (MIDI 1.0)"))
	bool OpenVirtualPort(const FString& PortName);

	/**
	 * Open a virtual MIDI output port (MIDI 2.0 UMP)
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
	 * Send a MIDI message (MIDI 1.0)
	 * Automatically converted to UMP if port is opened in MIDI 2.0 mode
	 * @param Data The MIDI message bytes to send
	 * @return True if the message was sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI", meta = (DisplayName = "Send Message (MIDI 1.0)"))
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

	/**
	 * Check if this output is using MIDI 2.0 (UMP)
	 * @return True if using MIDI 2.0
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI", meta = (DisplayName = "Is MIDI 2.0"))
	bool IsMidi2() const { return bIsMidi2; }

private:
	TUniquePtr<libremidi::midi_out> MidiOut;
	bool bIsMidi2 = false;

	/** Get the MIDI Engine Subsystem */
	ULibremidiEngineSubsystem* GetSubsystem() const;
	
	/** Convert FMidiPortInfo to libremidi::output_port */
	libremidi::output_port ConvertPortInfo(const FMidiPortInfo& PortInfo) const;
	
	/** Create MIDI output instance (MIDI 1.0 or 2.0) */
	bool CreateMidiOut(bool bUseMidi2, libremidi::API API);
	
	/** Common port opening logic */
	bool OpenPortInternal(const FMidiPortInfo& PortInfo, const FString& ClientName, bool bUseMidi2);
	
	/** Common virtual port opening logic */
	bool OpenVirtualPortInternal(const FString& PortName, bool bUseMidi2);

	/** Create output configuration */
	libremidi::output_configuration CreateOutputConfiguration();
	
	/** Handle errors */
	void HandleError(const FString& ErrorMessage);
};
