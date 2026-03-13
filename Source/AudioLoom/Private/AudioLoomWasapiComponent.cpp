// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomWasapiComponent.h"
#include "AudioLoomPcmLoader.h"
#include "WasapiAudioBackend.h"
#include "Sound/SoundWave.h"
#include "UObject/UnrealType.h"

UAudioLoomWasapiComponent::UAudioLoomWasapiComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Backend = new FWasapiAudioBackend();
}

void UAudioLoomWasapiComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bPlayOnBeginPlay)
	{
		Play();
	}
}

#if WITH_EDITOR
void UAudioLoomWasapiComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropName = PropertyChangedEvent.GetMemberPropertyName();
	if ((PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomWasapiComponent, OutputChannel) ||
	     PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomWasapiComponent, DeviceId)) &&
	    IsPlaying() && SoundWave)
	{
		Stop();
		Play();
	}
}
#endif

void UAudioLoomWasapiComponent::BeginDestroy()
{
	Stop();
	if (Backend)
	{
		delete Backend;
		Backend = nullptr;
	}
	Super::BeginDestroy();
}

void UAudioLoomWasapiComponent::Play()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (!Backend || !SoundWave)
	{
		return;
	}

	FAudioLoomPcmResult Result = FAudioLoomPcmLoader::LoadFromSoundWave(SoundWave);
	if (!Result.bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("AudioLoom: Failed to load PCM from SoundWave %s"), *GetNameSafe(SoundWave));
		return;
	}

	// Resample to 48kHz if needed (simple linear interpolation for prototype)
	TArray<float> PCM = Result.PCM;
	if (Result.SampleRate != 48000 && Result.SampleRate > 0)
	{
		const float Ratio = 48000.0f / Result.SampleRate;
		const int32 NewNumSamples = FMath::RoundToInt(PCM.Num() * Ratio);
		TArray<float> Resampled;
		Resampled.SetNum(NewNumSamples);
		for (int32 i = 0; i < NewNumSamples; ++i)
		{
			const float SrcIdx = i / Ratio;
			const int32 Idx0 = FMath::Clamp(FMath::FloorToInt(SrcIdx), 0, PCM.Num() - 1);
			const int32 Idx1 = FMath::Clamp(Idx0 + 1, 0, PCM.Num() - 1);
			const float Frac = SrcIdx - Idx0;
			Resampled[i] = FMath::Lerp(PCM[Idx0], PCM[Idx1], Frac);
		}
		PCM = MoveTemp(Resampled);
	}

	Backend->Start(DeviceId, PCM, Result.NumChannels, OutputChannel, bLoop);
#endif
}

void UAudioLoomWasapiComponent::Stop()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (Backend)
	{
		Backend->Stop();
	}
#endif
}

bool UAudioLoomWasapiComponent::IsPlaying() const
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	return Backend && Backend->IsPlaying();
#else
	return false;
#endif
}

