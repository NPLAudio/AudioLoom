// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioOutputDeviceEnumerator.h
 * @brief Lists output devices and resolves **DeviceId** strings used by `UAudioLoomComponent`.
 *
 * Windows: `IMMDeviceEnumerator` + property store; channel count may be refined via
 * `IsFormatSupported` probing (see .cpp). macOS: Core Audio device list + stream configuration.
 */

#pragma once

#include "CoreMinimal.h"
#include "AudioOutputDeviceInfo.h"

/**
 * Enumerates audio output devices (Windows WASAPI / macOS Core Audio).
 */
class AUDIOLOOM_API FAudioOutputDeviceEnumerator // all static helpers — no instance state
{
public:
	/**
	 * Get all active audio rendering (output) devices.
	 * Call from game thread; may block briefly for COM enumeration.
	 */
	static TArray<FAudioOutputDeviceInfo> GetOutputDevices();

	/**
	 * Get device info by ID. Returns bIsValid=false if not found.
	 */
	static FAudioOutputDeviceInfo GetDeviceById(const FString& DeviceId);

	/**
	 * Get the system default output device info. Used when DeviceId is empty.
	 */
	static FAudioOutputDeviceInfo GetDefaultOutputDevice();
};
