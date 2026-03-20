// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomOscSubsystem.h
 * @brief Per-world OSC **UDP server** (incoming triggers) and **client** (outgoing `/state`).
 *
 * **Registry**: `RebuildComponentRegistry` maps OSC paths (`/base/play`, `/base/stop`, `/base/loop`)
 * to `UAudioLoomComponent` weak pointers. Call `RebuildComponentRegistry` when levels or addresses change.
 *
 * Settings: `UAudioLoomOscSettings` (Project Settings). Requires Engine OSC plugin.
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AudioLoomOscSubsystem.generated.h"

class UAudioLoomComponent;
class UOSCServer;
class UOSCClient;
struct FOSCMessage;

/**
 * World subsystem: OSC UDP listener + monitoring client; editor and PIE worlds each have an instance.
 */
UCLASS()
class AUDIOLOOM_API UAudioLoomOscSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Start the OSC server on the configured listen port. Returns true if listening. */
	UFUNCTION(BlueprintCallable, Category = "AudioLoom|OSC")
	bool StartListening();

	/** Stop the OSC server. */
	UFUNCTION(BlueprintCallable, Category = "AudioLoom|OSC")
	void StopListening();

	/** Returns true if the OSC server is currently listening. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AudioLoom|OSC")
	bool IsListening() const;

	/** Try to bind to the given port. Returns true if port is available. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AudioLoom|OSC")
	static bool IsPortAvailable(int32 Port);

	/** Update monitoring client target (IP and port). Call after changing settings. */
	UFUNCTION(BlueprintCallable, Category = "AudioLoom|OSC")
	void UpdateMonitoringTarget();

	/** Rebuild component-to-address registry. Call when addresses change. */
	UFUNCTION(BlueprintCallable, Category = "AudioLoom|OSC")
	void RebuildComponentRegistry();

	/** Send a play/stop state message for a component. Address format: BaseAddress/state, value 1=playing 0=stopped. */
	void SendStateUpdate(UAudioLoomComponent* Component, bool bPlaying);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	/** Bound to `OSCServer`; parses address and dispatches to components (game thread). */
	void HandleOSCMessage(const FOSCMessage& Message, const FString& IPAddress, uint16 Port);

	UPROPERTY()
	UOSCServer* OSCServer; // UDP listener (incoming triggers)

	UPROPERTY()
	UOSCClient* OSCClient; // UDP sender (outgoing /state); recreated when Send IP/Port changes

	/** Full OSC path string → components that should react to …/play */
	TMap<FString, TArray<TWeakObjectPtr<UAudioLoomComponent>>> PlayTagToComponents;
	TMap<FString, TArray<TWeakObjectPtr<UAudioLoomComponent>>> StopTagToComponents;
	TMap<FString, TArray<TWeakObjectPtr<UAudioLoomComponent>>> LoopTagToComponents;

	/** Reserved for future debounced registry rebuilds (currently timer drives refresh from panel). */
	FTimerHandle RebuildRegistryTimer;
};
