// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioLoomComponent.generated.h"

class USoundWave;

/**
 * Routes sound playback to a specific audio device and channel (Windows/macOS).
 * Attach to any Actor: drop a sound, select device and channel, play.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent, DisplayName = "Audio Loom"))
class AUDIOLOOM_API UAudioLoomComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAudioLoomComponent();

	/** Sound to play. Drag from Content Browser. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Routing")
	TObjectPtr<USoundWave> SoundWave;

	/**
	 * Device ID. Set via details customization dropdown.
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

	/**
	 * Low-latency mode (Windows). Bypasses system mixer for lower latency.
	 * Not all devices support it; falls back to shared if unavailable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Routing")
	bool bUseExclusiveMode = false;

	/**
	 * Buffer size in ms for low-latency mode. 0 = driver default.
	 * Typical range 3-20 ms. Lower = less latency, higher dropout risk.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Routing", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "50"))
	int32 BufferSizeMs = 0;

	/** Auto-play when Play In Editor / game starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Playback")
	bool bPlayOnBeginPlay = false;

	/** Loop the sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Playback")
	bool bLoop = false;

	/** When enabled, automatically replay after playback ends. Delay is from sound end to next play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Playback")
	bool bAutoReplay = false;

	/** Use random delay between replays. When false, uses fixed ReplayDelaySeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Playback", meta = (EditCondition = "bAutoReplay"))
	bool bRandomReplayDelay = false;

	/** Fixed delay in seconds from sound end to next play. Used when bRandomReplayDelay is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Playback", meta = (EditCondition = "bAutoReplay", ClampMin = "0.0", UIMin = "0.0"))
	float ReplayDelaySeconds = 1.0f;

	/** Minimum delay in seconds (random mode). Random delay is between ReplayDelayMin and ReplayDelayMax. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Playback", meta = (EditCondition = "bAutoReplay && bRandomReplayDelay", ClampMin = "0.0", UIMin = "0.0"))
	float ReplayDelayMin = 0.5f;

	/** Maximum delay in seconds (random mode). Random delay is between ReplayDelayMin and ReplayDelayMax. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|Playback", meta = (EditCondition = "bAutoReplay && bRandomReplayDelay", ClampMin = "0.0", UIMin = "0.0"))
	float ReplayDelayMax = 3.0f;

	/**
	 * Base OSC address for this component (e.g. /audioloom/1).
	 * Empty = use default derived from owner and component index.
	 * Play=/base/play, Stop=/base/stop, Loop=/base/loop. Validate override per OSC spec.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioLoom|OSC", meta = (DisplayName = "OSC Address"))
	FString OscAddress;

	UFUNCTION(BlueprintCallable, Category = "AudioLoom")
	void Play();

	UFUNCTION(BlueprintCallable, Category = "AudioLoom")
	void Stop();

	UFUNCTION(BlueprintCallable, Category = "AudioLoom", meta = (DisplayName = "Set Loop"))
	void SetLoop(bool bInLoop);

	UFUNCTION(BlueprintPure, Category = "AudioLoom")
	bool IsPlaying() const;

	UFUNCTION(BlueprintPure, Category = "AudioLoom")
	bool GetLoop() const { return bLoop; }

	/** Get effective OSC base address (resolved default if OscAddress is empty). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AudioLoom|OSC", meta = (DisplayName = "Get OSC Address"))
	FString GetOscAddress() const;

	/** Set OSC address. Validates format. Returns true if valid and set. */
	UFUNCTION(BlueprintCallable, Category = "AudioLoom|OSC", meta = (DisplayName = "Set OSC Address"))
	bool SetOscAddress(const FString& InAddress);

	/** Get device ID. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AudioLoom|Routing")
	FString GetDeviceId() const { return DeviceId; }

	/** Set device ID. Empty = default output. */
	UFUNCTION(BlueprintCallable, Category = "AudioLoom|Routing")
	void SetDeviceId(const FString& InDeviceId) { DeviceId = InDeviceId; }

	/** Get output channel (0 = all). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AudioLoom|Routing")
	int32 GetOutputChannel() const { return OutputChannel; }

	/** Set output channel (0 = all, 1-based for specific channel). */
	UFUNCTION(BlueprintCallable, Category = "AudioLoom|Routing")
	void SetOutputChannel(int32 Channel) { OutputChannel = FMath::Max(0, Channel); }

	/** Called when bAutoReplay or bLoop changes (e.g. from UI). Updates tick state. */
	UFUNCTION(BlueprintCallable, Category = "AudioLoom|Playback")
	void UpdateTickEnabled();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	class FWasapiAudioBackend* AudioBackend = nullptr;
	bool bWasPlaying = false;
	float ReplayCountdown = 0.0f;
};
