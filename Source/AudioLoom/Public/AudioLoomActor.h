// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomActor.h
 * @brief Placeable actor wrapping a single `UAudioLoomComponent` (same features as adding the component).
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AudioLoomActor.generated.h"

class UAudioLoomComponent;

/**
 * Convenience actor: one **AudioLoomComponent** child for quick level placement.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Audio Loom"))
class AUDIOLOOM_API AAudioLoomActor : public AActor
{
	GENERATED_BODY()

public:
	AAudioLoomActor();

	/** Subobject created in ctor; same type as **Add Component → Audio Loom** on any actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AudioLoom")
	TObjectPtr<UAudioLoomComponent> AudioLoomComponent;

	/** Shown in World Outliner when the actor is first placed (editable per-instance later). */
	virtual FString GetDefaultActorLabel() const override;

protected:
	virtual void BeginPlay() override; // Super only; playback is opt-in via component flags / Blueprint
};
