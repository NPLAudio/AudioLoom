// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioLoomWasapiComponent.generated.h"

class USoundWave;

/**
 * Routes sound playback to a specific audio device and channel (Windows/macOS).
 * Attach to any Actor: drop a sound, select device and channel, play.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent, DisplayName = "Audio Loom"))
class AUDIOLOOM_API UAudioLoomWasapiComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAudioLoomWasapiComponent();

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

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	class FWasapiAudioBackend* WasapiBackend = nullptr;
};
