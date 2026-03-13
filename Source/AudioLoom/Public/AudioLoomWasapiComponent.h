// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioLoomWasapiComponent.generated.h"

class USoundWave;

/**
 * Routes sound playback to a specific WASAPI audio device and channel.
 * Attach to any Actor: drop a sound, select device and channel, play.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class AUDIOLOOM_API UAudioLoomWasapiComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAudioLoomWasapiComponent();

	/** Sound to play. Drag from Content Browser. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Routing")
	TObjectPtr<USoundWave> SoundWave;

	/**
	 * WASAPI device ID. Set via details customization dropdown.
	 * Empty = default output device.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Routing")
	FString DeviceId;

	/**
	 * Output channel (1-based). 0 = play to all channels (default).
	 * E.g. 3 = route to channel 3 only on multi-channel devices.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Routing", meta = (ClampMin = "0", UIMin = "0"))
	int32 OutputChannel = 0;

	/** Auto-play when Play In Editor / game starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Playback")
	bool bPlayOnBeginPlay = false;

	/** Loop the sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Playback")
	bool bLoop = false;

	UFUNCTION(BlueprintCallable, Category = "AudioLoom")
	void Play();

	UFUNCTION(BlueprintCallable, Category = "AudioLoom")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "AudioLoom")
	bool IsPlaying() const;

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	class FWasapiAudioBackend* Backend = nullptr;
};
