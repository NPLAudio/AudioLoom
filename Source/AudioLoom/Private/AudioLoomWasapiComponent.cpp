// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomWasapiComponent.h"
#include "AudioLoomPcmLoader.h"
#include "AudioLoomBlueprintLibrary.h"
#include "AudioLoomOscSubsystem.h"
#include "WasapiAudioBackend.h"
#include "Sound/SoundWave.h"
#include "UObject/UnrealType.h"

UAudioLoomWasapiComponent::UAudioLoomWasapiComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	WasapiBackend = new FWasapiAudioBackend();
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
	     PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomWasapiComponent, DeviceId) ||
	     PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomWasapiComponent, bUseExclusiveMode) ||
	     PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomWasapiComponent, BufferSizeMs)) &&
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
	if (WasapiBackend)
	{
		delete WasapiBackend;
		WasapiBackend = nullptr;
	}
	Super::BeginDestroy();
}

void UAudioLoomWasapiComponent::Play()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (!SoundWave)
	{
		return;
	}
	if (!WasapiBackend) return;

	FAudioLoomPcmResult Result = FAudioLoomPcmLoader::LoadFromSoundWave(SoundWave);
	if (!Result.bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("AudioLoom: Failed to load PCM from SoundWave %s"), *GetNameSafe(SoundWave));
		return;
	}

	// Resample to 48kHz if needed (per-channel to preserve stereo/multi-channel)
	TArray<float> PCM = Result.PCM;
	if (Result.SampleRate != 48000 && Result.SampleRate > 0 && Result.NumChannels > 0)
	{
		const float Ratio = 48000.0f / Result.SampleRate;
		const int32 NumCh = FMath::Max(1, Result.NumChannels);
		const int32 SrcFrames = PCM.Num() / NumCh;
		const int32 DstFrames = FMath::RoundToInt(SrcFrames * Ratio);
		TArray<float> Resampled;
		Resampled.SetNum(DstFrames * NumCh);
		for (int32 ch = 0; ch < NumCh; ++ch)
		{
			for (int32 i = 0; i < DstFrames; ++i)
			{
				const float SrcIdx = i / Ratio;
				const int32 F0 = FMath::Clamp(FMath::FloorToInt(SrcIdx), 0, SrcFrames - 1);
				const int32 F1 = FMath::Clamp(F0 + 1, 0, SrcFrames - 1);
				const float Frac = SrcIdx - F0;
				const float V0 = PCM[F0 * NumCh + ch];
				const float V1 = PCM[F1 * NumCh + ch];
				Resampled[i * NumCh + ch] = FMath::Lerp(V0, V1, Frac);
			}
		}
		PCM = MoveTemp(Resampled);
	}

	WasapiBackend->Start(DeviceId, PCM, Result.NumChannels, OutputChannel, bLoop, bUseExclusiveMode, BufferSizeMs);

	if (UWorld* W = GetWorld())
	{
		if (UAudioLoomOscSubsystem* Sub = W->GetSubsystem<UAudioLoomOscSubsystem>())
		{
			Sub->SendStateUpdate(this, true);
		}
	}
#endif
}

void UAudioLoomWasapiComponent::Stop()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (WasapiBackend)
	{
		WasapiBackend->Stop();
	}
	if (UWorld* W = GetWorld())
	{
		if (UAudioLoomOscSubsystem* Sub = W->GetSubsystem<UAudioLoomOscSubsystem>())
		{
			Sub->SendStateUpdate(this, false);
		}
	}
#endif
}

bool UAudioLoomWasapiComponent::IsPlaying() const
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	return WasapiBackend && WasapiBackend->IsPlaying();
#else
	return false;
#endif
}

void UAudioLoomWasapiComponent::SetLoop(bool bInLoop)
{
	bLoop = bInLoop;
}

FString UAudioLoomWasapiComponent::GetOscAddress() const
{
	if (!OscAddress.IsEmpty())
	{
		FString Normalized = UAudioLoomBlueprintLibrary::NormalizeOscAddress(OscAddress);
		if (!Normalized.IsEmpty()) return Normalized;
	}
	AActor* Owner = GetOwner();
	if (!Owner) return TEXT("/audioloom/unnamed");
	TArray<UAudioLoomWasapiComponent*> Comps;
	Owner->GetComponents(Comps);
	int32 Idx = Comps.IndexOfByKey(this);
	return FString::Printf(TEXT("/audioloom/%s/%d"), *Owner->GetName(), FMath::Max(0, Idx));
}

bool UAudioLoomWasapiComponent::SetOscAddress(const FString& InAddress)
{
	if (InAddress.IsEmpty())
	{
		OscAddress = InAddress;
		return true;
	}
	FString Normalized = UAudioLoomBlueprintLibrary::NormalizeOscAddress(InAddress);
	if (Normalized.IsEmpty()) return false;
	OscAddress = Normalized;
	return true;
}

