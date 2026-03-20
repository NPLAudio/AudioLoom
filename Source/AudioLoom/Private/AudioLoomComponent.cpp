// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomComponent.cpp
 * @brief Implements playback, PCM load/resample, OSC helpers, and editor hot-restart of routing.
 */

#include "AudioLoomComponent.h"
#include "AudioLoomPcmLoader.h"
#include "AudioLoomBlueprintLibrary.h"
#include "AudioLoomOscSubsystem.h"
#include "AudioLoomPlaybackBackend.h"
#include "Sound/SoundWave.h"
#include "UObject/UnrealType.h"

UAudioLoomComponent::UAudioLoomComponent()
{
	PrimaryComponentTick.bCanEverTick = true;  // may enable later for auto-replay countdown
	PrimaryComponentTick.bStartWithTickEnabled = false; // off until UpdateTickEnabled says otherwise
	AudioBackend = new FAudioLoomPlaybackBackend();       // raw pointer: freed in BeginDestroy (not UObject)
}

void UAudioLoomComponent::UpdateTickEnabled()
{
	// Tick only needed for auto-replay: loop mode is handled inside the backend; no SoundWave → nothing to replay
	SetComponentTickEnabled(bAutoReplay && !bLoop && SoundWave != nullptr);
	if (!bAutoReplay)
	{
		ReplayCountdown = 0.0f;
	}
}

void UAudioLoomComponent::BeginPlay()
{
	Super::BeginPlay();
	UpdateTickEnabled(); // sync tick with bAutoReplay / bLoop after load
	bWasPlaying = false; // so first IsPlaying() edge isn’t mistaken for “just stopped”
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
	if (!bAutoReplay || bLoop || !SoundWave) return; // loop: backend repeats; no SoundWave: nothing to replay

	const bool bPlaying = IsPlaying();
	if (bWasPlaying && !bPlaying)
	{
		// Edge: was playing last frame, silent now → treat as clip finished (non-loop)
		if (bRandomReplayDelay)
		{
			ReplayCountdown = FMath::RandRange(FMath::Max(0.0f, ReplayDelayMin), FMath::Max(ReplayDelayMin, ReplayDelayMax));
		}
		else
		{
			ReplayCountdown = FMath::Max(0.0f, ReplayDelaySeconds);
		}
	}
	bWasPlaying = bPlaying; // update for next frame’s edge detect

	if (ReplayCountdown > 0.0f)
	{
		ReplayCountdown -= DeltaTime;
		if (ReplayCountdown <= 0.0f)
		{
			ReplayCountdown = 0.0f;
			Play(); // fire another one-shot play; backend thread starts fresh
		}
	}
}

#if WITH_EDITOR
void UAudioLoomComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropName = PropertyChangedEvent.GetMemberPropertyName();
	// Routing/backend settings require a fresh Start() on the audio device; restart if currently playing
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
	Stop(); // join playback thread before UObject teardown
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

	// Decode asset to float PCM (editor: imported data; else WAV next to uasset if present)
	FAudioLoomPcmResult Result = FAudioLoomPcmLoader::LoadFromSoundWave(SoundWave);
	if (!Result.bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("AudioLoom: Failed to load PCM from SoundWave %s"), *GetNameSafe(SoundWave));
		return;
	}

	// Backend expects 48 kHz float PCM; linear interpolation per channel if source rate differs
	TArray<float> PCM = Result.PCM;
	if (Result.SampleRate != 48000 && Result.SampleRate > 0 && Result.NumChannels > 0)
	{
		const float Ratio = 48000.0f / Result.SampleRate; // dst_time = src_time * Ratio in frames
		const int32 NumCh = FMath::Max(1, Result.NumChannels);
		const int32 SrcFrames = PCM.Num() / NumCh;
		const int32 DstFrames = FMath::RoundToInt(SrcFrames * Ratio);
		TArray<float> Resampled;
		Resampled.SetNum(DstFrames * NumCh);
		for (int32 ch = 0; ch < NumCh; ++ch)
		{
			for (int32 i = 0; i < DstFrames; ++i)
			{
				const float SrcIdx = i / Ratio; // fractional source frame index
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

	// DeviceId empty → backend opens default output; OutputChannel 0 → all device channels get source
	AudioBackend->Start(DeviceId, PCM, Result.NumChannels, OutputChannel, bLoop, bUseExclusiveMode, BufferSizeMs);

	// Optional: tell external OSC monitors we started (Send IP:Port from project settings)
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
		AudioBackend->Stop(); // blocks until playback thread finishes
	}
	if (UWorld* W = GetWorld())
	{
		if (UAudioLoomOscSubsystem* Sub = W->GetSubsystem<UAudioLoomOscSubsystem>())
		{
			Sub->SendStateUpdate(this, false); // optional outbound OSC (same guard as Play)
		}
	}
#endif
}

bool UAudioLoomComponent::IsPlaying() const
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	return AudioBackend && AudioBackend->IsPlaying(); // false after thread exits or before first Play
#else
	return false; // no backend on other platforms
#endif
}

void UAudioLoomComponent::SetLoop(bool bInLoop)
{
	bLoop = bInLoop; // next Start() picks this up; if already playing, loop flag is in backend — restart play to apply
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
	Owner->GetComponents(Comps);              // order matches editor “array of components”
	int32 Idx = Comps.IndexOfByKey(this);     // stable index for default path
	return FString::Printf(TEXT("/audioloom/%s/%d"), *Owner->GetName(), FMath::Max(0, Idx));
}

bool UAudioLoomComponent::SetOscAddress(const FString& InAddress)
{
	if (InAddress.IsEmpty())
	{
		OscAddress = InAddress; // clear → GetOscAddress falls back to default path
		return true;
	}
	FString Normalized = UAudioLoomBlueprintLibrary::NormalizeOscAddress(InAddress);
	if (Normalized.IsEmpty()) return false; // illegal OSC path per engine rules
	OscAddress = Normalized;
	return true;
}
