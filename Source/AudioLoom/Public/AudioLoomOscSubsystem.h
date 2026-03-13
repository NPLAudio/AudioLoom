// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AudioLoomOscSubsystem.generated.h"

class UAudioLoomWasapiComponent;
class UOSCServer;
class UOSCClient;
struct FOSCMessage;

/**
 * World subsystem that runs the AudioLoom OSC server (triggers) and client (monitoring).
 * Exists in both editor and PIE worlds.
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
	void SendStateUpdate(UAudioLoomWasapiComponent* Component, bool bPlaying);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleOSCMessage(const FOSCMessage& Message, const FString& IPAddress, uint16 Port);

	UPROPERTY()
	UOSCServer* OSCServer;

	UPROPERTY()
	UOSCClient* OSCClient;

	TMap<FString, TArray<TWeakObjectPtr<UAudioLoomWasapiComponent>>> PlayTagToComponents;
	TMap<FString, TArray<TWeakObjectPtr<UAudioLoomWasapiComponent>>> StopTagToComponents;
	TMap<FString, TArray<TWeakObjectPtr<UAudioLoomWasapiComponent>>> LoopTagToComponents;

	FTimerHandle RebuildRegistryTimer;
};
