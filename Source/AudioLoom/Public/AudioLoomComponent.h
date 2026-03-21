// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomComponent.h
 * @brief Core runtime type: decodes `USoundWave` to PCM, resamples to 48 kHz, plays via OS backend.
 *
 * **Data flow**
 *   1. `Play()` → `FAudioLoomPcmLoader::LoadFromSoundWave` → float PCM, channel count, sample rate
 *   2. Optional linear resampling to 48 kHz in `Play()` (per-channel) for backend contract
 *   3. `FAudioLoomPlaybackBackend::Start` — platform thread pushes samples to WASAPI (Windows) or Core Audio (macOS)
 *
 * **Auto-replay** (when `bAutoReplay` and not `bLoop`): `TickComponent` watches `IsPlaying()`;
 * on transition playing→stopped, starts a delay timer then calls `Play()` again.
 *
 * **OSC**: address resolution in `GetOscAddress` / `SetOscAddress`; state notifications via
 * `UAudioLoomOscSubsystem::SendStateUpdate` (see `AudioLoomOscSubsystem.cpp`).
 *
 * **Threading**: `Play`/`Stop` run on game thread; backend uses a dedicated playback thread.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioLoomComponent.generated.h"

class USoundWave;

/**
 * Routes decoded PCM to a specific audio output device and channel (Windows/macOS).
 * Does not use Unreal's `AudioComponent` graph — uses OS APIs directly for device/channel selection.
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

	// --- Blueprint: transport ---
	UFUNCTION(BlueprintCallable, Category = "AudioLoom")
	void Play();

	UFUNCTION(BlueprintCallable, Category = "AudioLoom")
	void Stop();

	UFUNCTION(BlueprintCallable, Category = "AudioLoom", meta = (DisplayName = "Set Loop"))
	void SetLoop(bool bInLoop);

	UFUNCTION(BlueprintPure, Category = "AudioLoom")
	bool IsPlaying() const;

	/** Estimated OS output buffer / stream latency (ms), or 0 when not playing. Not full round-trip I/O latency. */
	UFUNCTION(BlueprintPure, Category = "AudioLoom|Diagnostics")
	float GetOutputLatencyMs() const;

	UFUNCTION(BlueprintPure, Category = "AudioLoom")
	bool GetLoop() const { return bLoop; }

	// --- Blueprint: OSC base path (triggers are /base/play|stop|loop) ---
	/** Get effective OSC base address (resolved default if OscAddress is empty). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AudioLoom|OSC", meta = (DisplayName = "Get OSC Address"))
	FString GetOscAddress() const;

	/** Set OSC address. Validates format. Returns true if valid and set. */
	UFUNCTION(BlueprintCallable, Category = "AudioLoom|OSC", meta = (DisplayName = "Set OSC Address"))
	bool SetOscAddress(const FString& InAddress);

	// --- Blueprint: routing mirrors (optional; UPROPERTY already exposes same fields) ---
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
	virtual void BeginPlay() override;   // optional auto-play, UpdateTickEnabled
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // clear replay state
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override; // auto-replay only
	virtual void BeginDestroy() override; // Stop + delete backend
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override; // restart play if routing changed while playing
#endif

private:
	/** Non-reflected backend; owned by this component and deleted in `BeginDestroy`. */
	class FAudioLoomPlaybackBackend* AudioBackend = nullptr;
	/** Previous-frame playing state (for auto-replay edge detection). */
	bool bWasPlaying = false;
	/** Seconds remaining before next `Play()` when auto-replay countdown is active. */
	float ReplayCountdown = 0.0f;
};
