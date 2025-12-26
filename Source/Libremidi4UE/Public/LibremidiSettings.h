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
	
	/**
	 * Dynamically generates API options for editor display
	 * Filters to platform-specific APIs only
	 */
	UFUNCTION()
	TArray<FString> GetAvailableAPIOptions() const;
#endif

	// ========================================================================
	// Backend API Property (FString for dynamic dropdown, Enum for type safety)
	// ========================================================================
	
	/**
	 * Backend API name (stored as FString internally for dynamic editor filtering)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (
		DisplayName = "Backend API",
		GetOptions = "GetAvailableAPIOptions",
		ToolTip = "The MIDI API backend to use. 'Auto' automatically selects the best API with UMP priority. Invalid selections will be automatically corrected."))
	FString BackendAPIName;

	/**
	 * Gets the backend API as an Enum value
	 * @return Currently selected API (Enum value)
	 */
	UFUNCTION(BlueprintPure, Category = "MIDI|Settings")
	ELibremidiAPI GetBackendAPI() const;

	/**
	 * Sets the backend API using an Enum value
	 * @param API The API to set (Enum value)
	 */
	UFUNCTION(BlueprintCallable, Category = "MIDI|Settings")
	void SetBackendAPI(ELibremidiAPI API);

	// ========================================================================
	// Other Settings
	// ========================================================================

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (
		DisplayName = "Track Hardware Devices", 
		ToolTip = "Enable tracking of hardware MIDI devices"))
	bool bTrackHardware;

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (
		DisplayName = "Track Virtual Devices", 
		ToolTip = "Enable tracking of virtual (software) MIDI devices"))
	bool bTrackVirtual;

	UPROPERTY(Config, EditAnywhere, Category = "Observer", meta = (
		DisplayName = "Track Any Devices", 
		ToolTip = "Enable tracking of any type of MIDI devices"))
	bool bTrackAny;

	// ========================================================================
	// Static Helper Methods
	// ========================================================================

	/**
	 * Gets the platform-specific default API
	 */
	static ELibremidiAPI GetPlatformDefaultAPI();
	
	/**
	 * Gets the preferred API for the current platform
	 * If API is Unspecified, returns the best available API with UMP priority
	 * Otherwise returns the specified API
	 */
	static ELibremidiAPI GetPreferredAPI(ELibremidiAPI RequestedAPI);

#if WITH_EDITOR
	/**
	 * Gets the list of APIs available for the current platform
	 */
	static TArray<ELibremidiAPI> GetAvailableAPIsForPlatform();
	
	/**
	 * Checks if an API is compiled in and available
	 */
	static bool IsAPICompiledIn(ELibremidiAPI API);
	
	/**
	 * Checks if an API is valid for the current platform
	 */
	static bool IsAPIValidForCurrentPlatform(ELibremidiAPI API);
	
	/**
	 * Validates and preserves the previously selected API if possible
	 */
	static ELibremidiAPI ValidateAndPreserveAPI(ELibremidiAPI RequestedAPI);
	
	/**
	 * Converts FString (DisplayName) to Enum value
	 */
	static ELibremidiAPI StringToAPI(const FString& APIName);
	
	/**
	 * Converts Enum value to FString (DisplayName)
	 */
	static FString APIToString(ELibremidiAPI API);
#endif

private:
	/** Internal cache for performance optimization */
	mutable ELibremidiAPI CachedBackendAPI = ELibremidiAPI::Unspecified;
	mutable bool bCacheValid = false;
};