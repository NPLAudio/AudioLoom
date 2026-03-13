// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "WasapiDeviceInfo.h"

/**
 * Enumerates WASAPI audio output devices.
 * Windows-only; returns empty array on other platforms.
 */
class AUDIOLOOM_API FWasapiDeviceEnumerator
{
public:
	/**
	 * Get all active audio rendering (output) devices.
	 * Call from game thread; may block briefly for COM enumeration.
	 */
	static TArray<FWasapiDeviceInfo> GetOutputDevices();

	/**
	 * Get device info by ID. Returns bIsValid=false if not found.
	 */
	static FWasapiDeviceInfo GetDeviceById(const FString& DeviceId);

	/**
	 * Get the system default output device info. Used when DeviceId is empty.
	 */
	static FWasapiDeviceInfo GetDefaultOutputDevice();
};
