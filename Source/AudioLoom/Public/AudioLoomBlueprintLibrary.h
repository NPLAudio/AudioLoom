// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomBlueprintLibrary.h
 * @brief OSC path validation/normalization using Engine **OSC** module (`UOSCManager`).
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AudioLoomBlueprintLibrary.generated.h"

/**
 * Blueprint-accessible utilities (OSC address helpers) for AudioLoom.
 */
UCLASS()
class AUDIOLOOM_API UAudioLoomBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// All methods are static: Blueprint calls them on the class, no instance exists
	/**
	 * Validate an OSC address per OSC 1.0 spec.
	 * Must start with /, use valid path characters. Returns true if valid.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AudioLoom|OSC", meta = (DisplayName = "Is OSC Address Valid"))
	static bool IsOscAddressValid(const FString& Address);

	/**
	 * Normalize an OSC address (ensure leading /, trim). Returns empty if invalid.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AudioLoom|OSC", meta = (DisplayName = "Normalize OSC Address"))
	static FString NormalizeOscAddress(const FString& Address);

	/** Default OSC address prefix for AudioLoom components. */
	static const TCHAR* DefaultOscPrefix;
};
