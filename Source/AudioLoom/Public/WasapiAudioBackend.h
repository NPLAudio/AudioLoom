// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "WasapiDeviceInfo.h"

class FWasapiAudioBackendImpl;

/**
 * WASAPI audio playback backend.
 * Routes PCM data to a specific Windows audio output device.
 * Windows-only; no-op on other platforms.
 */
class AUDIOLOOM_API FWasapiAudioBackend
{
public:
	FWasapiAudioBackend();
	~FWasapiAudioBackend();

	/** Initialize and start playback. PCM is float mono or stereo, 48kHz. bLoop: repeat indefinitely until Stop(). */
	void Start(const FString& DeviceId, const TArray<float>& PCMData, int32 SourceChannels, int32 OutputChannel, bool bLoop = false);

	/** Stop playback and release resources. */
	void Stop();

	/** Whether currently playing. */
	bool IsPlaying() const;

private:
	TUniquePtr<FWasapiAudioBackendImpl> Impl;
};
