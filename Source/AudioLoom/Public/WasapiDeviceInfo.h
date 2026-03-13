// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"

/**
 * Information about a WASAPI audio output device.
 * Used for device enumeration and selection in the editor.
 */
struct AUDIOLOOM_API FWasapiDeviceInfo
{
	/** Unique device ID from IMMDevice::GetId(). Used for persistence. */
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
