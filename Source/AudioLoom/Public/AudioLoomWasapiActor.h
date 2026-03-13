// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AudioLoomWasapiActor.generated.h"

class UAudioLoomWasapiComponent;

/**
 * Convenience actor for WASAPI routing.
 * Place in level, configure SoundWave + device + channel on the root component, then Play.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Audio Loom WASAPI"))
class AUDIOLOOM_API AAudioLoomWasapiActor : public AActor
{
	GENERATED_BODY()

public:
	AAudioLoomWasapiActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AudioLoom")
	TObjectPtr<UAudioLoomWasapiComponent> WasapiComponent;

protected:
	virtual void BeginPlay() override;
};
