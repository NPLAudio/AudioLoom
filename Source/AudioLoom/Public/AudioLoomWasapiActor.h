// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AudioLoomWasapiActor.generated.h"

class UAudioLoomWasapiComponent;

/**
 * Convenience actor for audio routing (Windows/macOS).
 * Place in level, configure SoundWave + device + channel on the root component, then Play.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Audio Loom"))
class AUDIOLOOM_API AAudioLoomWasapiActor : public AActor
{
	GENERATED_BODY()

public:
	AAudioLoomWasapiActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AudioLoom")
	TObjectPtr<UAudioLoomWasapiComponent> WasapiComponent;

	/** Default label for newly placed actors (avoids "AudioLoomWasapi" in editor) */
	virtual FString GetDefaultActorLabel() const override;

protected:
	virtual void BeginPlay() override;
};
