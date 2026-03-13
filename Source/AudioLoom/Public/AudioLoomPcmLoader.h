// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"

class USoundWave;

/**
 * Result of loading PCM audio data.
 */
struct AUDIOLOOM_API FAudioLoomPcmResult
{
	TArray<float> PCM;
	int32 NumChannels = 0;
	int32 SampleRate = 48000;
	bool bSuccess = false;
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
