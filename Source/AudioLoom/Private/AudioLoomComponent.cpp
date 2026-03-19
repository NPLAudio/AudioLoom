// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomComponent.h"
#include "AudioLoomPcmLoader.h"
#include "AudioLoomBlueprintLibrary.h"
#include "AudioLoomOscSubsystem.h"
#include "WasapiAudioBackend.h"
#include "Sound/SoundWave.h"
#include "UObject/UnrealType.h"

UAudioLoomComponent::UAudioLoomComponent()
{
	PrimaryComponentTick.bCanEverTick = true;  // Used when bAutoReplay is on
	PrimaryComponentTick.bStartWithTickEnabled = false;
	AudioBackend = new FWasapiAudioBackend();
}

void UAudioLoomComponent::UpdateTickEnabled()
{
	SetComponentTickEnabled(bAutoReplay && !bLoop && SoundWave != nullptr);
	if (!bAutoReplay)
	{
		ReplayCountdown = 0.0f;
	}
}

void UAudioLoomComponent::BeginPlay()
{
	Super::BeginPlay();
	UpdateTickEnabled();
	bWasPlaying = false;
	if (bPlayOnBeginPlay)
	{
		Play();
	}
}

void UAudioLoomComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReplayCountdown = 0.0f;
	bWasPlaying = false;
	Super::EndPlay(EndPlayReason);
}

void UAudioLoomComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bAutoReplay || bLoop || !SoundWave) return;

	const bool bPlaying = IsPlaying();
	if (bWasPlaying && !bPlaying)
	{
		// Playback ended — start countdown
		if (bRandomReplayDelay)
		{
			ReplayCountdown = FMath::RandRange(FMath::Max(0.0f, ReplayDelayMin), FMath::Max(ReplayDelayMin, ReplayDelayMax));
		}
		else
		{
			ReplayCountdown = FMath::Max(0.0f, ReplayDelaySeconds);
		}
	}
	bWasPlaying = bPlaying;

	if (ReplayCountdown > 0.0f)
	{
		ReplayCountdown -= DeltaTime;
		if (ReplayCountdown <= 0.0f)
		{
			ReplayCountdown = 0.0f;
			Play();
		}
	}
}

#if WITH_EDITOR
void UAudioLoomComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropName = PropertyChangedEvent.GetMemberPropertyName();
	if ((PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomComponent, OutputChannel) ||
	     PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomComponent, DeviceId) ||
	     PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomComponent, bUseExclusiveMode) ||
	     PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomComponent, BufferSizeMs)) &&
	    IsPlaying() && SoundWave)
	{
		Stop();
		Play();
	}
	if (PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomComponent, bAutoReplay) ||
	    PropName == GET_MEMBER_NAME_CHECKED(UAudioLoomComponent, bLoop))
	{
		UpdateTickEnabled();
	}
}
#endif

void UAudioLoomComponent::BeginDestroy()
{
	Stop();
	if (AudioBackend)
	{
		delete AudioBackend;
		AudioBackend = nullptr;
	}
	Super::BeginDestroy();
}

void UAudioLoomComponent::Play()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (!SoundWave)
	{
		return;
	}
	if (!AudioBackend) return;

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

	AudioBackend->Start(DeviceId, PCM, Result.NumChannels, OutputChannel, bLoop, bUseExclusiveMode, BufferSizeMs);

	if (UWorld* W = GetWorld())
	{
		if (UAudioLoomOscSubsystem* Sub = W->GetSubsystem<UAudioLoomOscSubsystem>())
		{
			Sub->SendStateUpdate(this, true);
		}
	}
#endif
}

void UAudioLoomComponent::Stop()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (AudioBackend)
	{
		AudioBackend->Stop();
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

bool UAudioLoomComponent::IsPlaying() const
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	return AudioBackend && AudioBackend->IsPlaying();
#else
	return false;
#endif
}

void UAudioLoomComponent::SetLoop(bool bInLoop)
{
	bLoop = bInLoop;
}

FString UAudioLoomComponent::GetOscAddress() const
{
	if (!OscAddress.IsEmpty())
	{
		FString Normalized = UAudioLoomBlueprintLibrary::NormalizeOscAddress(OscAddress);
		if (!Normalized.IsEmpty()) return Normalized;
	}
	AActor* Owner = GetOwner();
	if (!Owner) return TEXT("/audioloom/unnamed");
	TArray<UAudioLoomComponent*> Comps;
	Owner->GetComponents(Comps);
	int32 Idx = Comps.IndexOfByKey(this);
	return FString::Printf(TEXT("/audioloom/%s/%d"), *Owner->GetName(), FMath::Max(0, Idx));
}

bool UAudioLoomComponent::SetOscAddress(const FString& InAddress)
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
