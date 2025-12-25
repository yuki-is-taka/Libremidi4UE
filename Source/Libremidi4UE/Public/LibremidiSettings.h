#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LibremidiTypes.h"
#include "LibremidiSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Libremidi MIDI"))
class LIBREMIDI4UE_API ULibremidiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULibremidiSettings();

	virtual FName GetCategoryName() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;
	
	// Custom property to filter available APIs in details panel
	void GetBackendAPIOptions(TArray<TSharedPtr<FString>>& OutOptions, TArray<TSharedPtr<FString>>& OutToolTips, TArray<bool>& OutRestrictedItems) const;
#endif

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (
		DisplayName = "Backend API", 
		ToolTip = "The MIDI API backend to use. Only APIs available on this platform are shown."))
	ELibremidiAPI BackendAPI;

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (DisplayName = "Track Hardware Devices", ToolTip = "Enable tracking of hardware MIDI devices"))
	bool bTrackHardware;

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (DisplayName = "Track Virtual Devices", ToolTip = "Enable tracking of virtual (software) MIDI devices"))
	bool bTrackVirtual;

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (DisplayName = "Track Any Devices", ToolTip = "Enable tracking of any type of MIDI devices"))
	bool bTrackAny;

	static ELibremidiAPI GetPlatformDefaultAPI();
	
#if WITH_EDITOR
	static TArray<ELibremidiAPI> GetAvailableAPIsForPlatform();
	static bool IsAPICompiledIn(ELibremidiAPI API);
#endif
};
