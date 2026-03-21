// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomPlaybackBackend.h
 * @brief PIMPL facade for platform-specific output (`FAudioLoomPlaybackBackendImpl` in .cpp).
 *
 * **WASAPI** on Windows and **Core Audio** on macOS. See `AudioLoomPlaybackBackend.cpp` for device
 * open, format, render loop, and channel routing.
 */

#pragma once

#include "CoreMinimal.h"
#include "AudioOutputDeviceInfo.h"

class FAudioLoomPlaybackBackendImpl;

/**
 * Audio playback backend (Windows: WASAPI; macOS: Core Audio `AudioDeviceIOProc`).
 * Float PCM at 48 kHz; maps `OutputChannel` to device buffers (see impl).
 */
class AUDIOLOOM_API FAudioLoomPlaybackBackend
{
public:
	FAudioLoomPlaybackBackend();
	~FAudioLoomPlaybackBackend();

	/**
	 * Initialize and start playback. PCM is interleaved float, 48 kHz.
	 * bLoop: repeat until Stop(). bExclusive / BufferSizeMs: Windows low-latency path only.
	 * Thread: spawns worker; Stop() joins. Safe to call only from game thread.
	 */
	void Start(const FString& DeviceId, const TArray<float>& PCMData, int32 SourceChannels, int32 OutputChannel, bool bLoop = false, bool bExclusive = false, int32 InBufferSizeMs = 0);

	/** Stop playback and release resources. */
	void Stop();

	/** Whether currently playing. */
	bool IsPlaying() const;

	/**
	 * Estimated output path latency (ms) from the last started stream, or 0 if not playing / unknown.
	 * Safe to call from game thread; value is set on the playback thread after the device stream initializes.
	 */
	float GetOutputLatencyMs() const;

private:
	TUniquePtr<FAudioLoomPlaybackBackendImpl> Impl; // platform impl defined only in .cpp (Windows + Mac types differ)
};
