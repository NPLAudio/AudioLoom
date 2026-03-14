// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AudioLoomActor.generated.h"

class UAudioLoomComponent;

/**
 * Convenience actor for audio routing (Windows/macOS).
 * Place in level, configure SoundWave + device + channel on the root component, then Play.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Audio Loom"))
class AUDIOLOOM_API AAudioLoomActor : public AActor
{
	GENERATED_BODY()

public:
	AAudioLoomActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AudioLoom")
	TObjectPtr<UAudioLoomComponent> AudioLoomComponent;

	/** Default label for newly placed actors */
	virtual FString GetDefaultActorLabel() const override;

protected:
	virtual void BeginPlay() override;
};
