// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "LibremidiInput.h"
#include "LibremidiOutput.h"
#include "LibremidiTypes.h"

THIRD_PARTY_INCLUDES_START
#include <libremidi/libremidi.hpp>
THIRD_PARTY_INCLUDES_END

#include "LibremidiEngineSubsystem.generated.h"

class ULibremidiSettings;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLibremidiObserverRestarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLibremidiInputPortChanged, const FLibremidiInputInfo&, PortInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLibremidiOutputPortChanged, const FLibremidiOutputInfo&, PortInfo);

DECLARE_MULTICAST_DELEGATE(FOnLibremidiObserverRestartedNative);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLibremidiInputPortChangedNative, const FLibremidiInputInfo&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLibremidiOutputPortChangedNative, const FLibremidiOutputInfo&);

UCLASS()
class LIBREMIDI4UE_API ULibremidiEngineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "MIDI|Observer")
	TArray<FLibremidiInputInfo> GetInputPorts() const;

	UFUNCTION(BlueprintPure, Category = "MIDI|Observer")
	TArray<FLibremidiOutputInfo> GetOutputPorts() const;

	libremidi::API GetObserverApi() const;

	UFUNCTION(BlueprintCallable, Category = "MIDI|Ports")
	ULibremidiInput* CreateInput(
		ELibremidiMidiProtocol Protocol,
		bool bInIgnoreSysex,
		bool bInIgnoreTiming,
		bool bInIgnoreSensing,
		ELibremidiTimestampMode Mode,
		bool bInMidi1ChannelEventsToMidi2);

	UFUNCTION(BlueprintCallable, Category = "MIDI|Ports")
	void DestroyInput(ULibremidiInput* Input);

	UFUNCTION(BlueprintCallable, Category = "MIDI|Ports")
	ULibremidiOutput* CreateOutput(
		ELibremidiMidiProtocol Protocol,
		ELibremidiTimestampMode Mode);

	UFUNCTION(BlueprintCallable, Category = "MIDI|Ports")
	void DestroyOutput(ULibremidiOutput* Output);

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Observer")
	FOnLibremidiObserverRestarted OnObserverRestarted;

	FOnLibremidiObserverRestartedNative OnObserverRestartedNative;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Observer")
	FOnLibremidiInputPortChanged OnInputPortAdded;

	FOnLibremidiInputPortChangedNative OnInputPortAddedNative;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Observer")
	FOnLibremidiInputPortChanged OnInputPortRemoved;

	FOnLibremidiInputPortChangedNative OnInputPortRemovedNative;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Observer")
	FOnLibremidiOutputPortChanged OnOutputPortAdded;

	FOnLibremidiOutputPortChangedNative OnOutputPortAddedNative;

	UPROPERTY(BlueprintAssignable, Category = "MIDI|Observer")
	FOnLibremidiOutputPortChanged OnOutputPortRemoved;

	FOnLibremidiOutputPortChangedNative OnOutputPortRemovedNative;

private:
	void StartObserver();
	void StopObserver();
	void LogObserverInfo() const;
#if WITH_EDITOR
	void HandleSettingsChanged(UObject* SettingsObject, struct FPropertyChangedEvent& PropertyChangedEvent);
#endif
	void HandleInputPortAdded(const libremidi::input_port& Port);
	void HandleInputPortRemoved(const libremidi::input_port& Port);
	void HandleOutputPortAdded(const libremidi::output_port& Port);
	void HandleOutputPortRemoved(const libremidi::output_port& Port);

private:
	TUniquePtr<libremidi::observer> Observer;
#if WITH_EDITOR
	FDelegateHandle SettingsChangedHandle;
#endif

	UPROPERTY()
	TArray<TObjectPtr<ULibremidiInput>> Inputs;

	UPROPERTY()
	TArray<TObjectPtr<ULibremidiOutput>> Outputs;
};
