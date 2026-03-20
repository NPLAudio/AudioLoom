// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioOutputDeviceInfo.h
 * @brief POD describing one output device row in UI and persisted **DeviceId** strings.
 */

#pragma once

#include "CoreMinimal.h"

/**
 * Information about an audio output device (Windows/macOS).
 * **DeviceId** is the stable string from the OS (Windows: IMMDevice `GetId`; Mac: device UID).
 */
struct AUDIOLOOM_API FAudioOutputDeviceInfo
{
	/** OS-stable ID (Windows: IMMDevice `GetId`; Mac: Core Audio UID string). Stored on the component. */
	FString DeviceId;

	/** Human-readable name (e.g. "Speakers (Realtek)"). */
	FString FriendlyName;

	/** Number of output channels (e.g. 2 for stereo, 8 for surround). */
	int32 NumChannels = 0;

	/** Sample rate in Hz (e.g. 48000). */
	int32 SampleRate = 48000;

	/** Bytes per sample (e.g. 4 for float32). */
	int32 BytesPerSample = 4;

	/** Whether this info is valid. */
	bool bIsValid = false;
};
