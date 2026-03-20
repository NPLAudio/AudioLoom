// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomPcmLoader.h
 * @brief WAV parse + `USoundWave` extraction → **interleaved float** PCM for the backend.
 *
 * Editor builds may use `GetImportedSoundWaveData` (16-bit PCM). Otherwise falls back to
 * reading a `.wav` beside the asset. See `AudioLoomPcmLoader.cpp`.
 */

#pragma once

#include "CoreMinimal.h"

class USoundWave;

/**
 * Result of loading PCM: interleaved floats, **NumChannels** interleaved (L,R,…).
 */
struct AUDIOLOOM_API FAudioLoomPcmResult
{
	TArray<float> PCM;       // interleaved samples; length = frameCount * NumChannels
	int32 NumChannels = 0;   // 1 = mono, 2 = stereo, etc.
	int32 SampleRate = 48000; // Hz from WAV fmt or imported asset metadata
	bool bSuccess = false;    // false if parse failed or no PCM could be obtained
};

/**
 * Loads PCM float data from WAV files or USoundWave assets.
 */
class AUDIOLOOM_API FAudioLoomPcmLoader
{
public:
	/**
	 * Load PCM from a WAV file path.
	 * Supports 16-bit and 32-bit float PCM. Converts to normalized float.
	 */
	static FAudioLoomPcmResult LoadFromFile(const FString& FilePath);

	/**
	 * Load PCM from a USoundWave asset.
	 * Uses RawData when available (parse as WAV), or loads source file from asset path.
	 */
	static FAudioLoomPcmResult LoadFromSoundWave(USoundWave* SoundWave);
};
