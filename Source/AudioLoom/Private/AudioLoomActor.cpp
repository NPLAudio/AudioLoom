// Copyright (c) 2026 AudioLoom Contributors.

/** @file AudioLoomActor.cpp — default subobject setup for Audio Loom placeable actor. */

#include "AudioLoomActor.h"
#include "AudioLoomComponent.h"
#include "Components/SceneComponent.h"

AAudioLoomActor::AAudioLoomActor()
{
	PrimaryActorTick.bCanEverTick = false; // actor itself never ticks; component may for auto-replay
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); // minimal transform root
	AudioLoomComponent = CreateDefaultSubobject<UAudioLoomComponent>(TEXT("AudioLoomComponent")); // attached to root by default
}

FString AAudioLoomActor::GetDefaultActorLabel() const
{
	return TEXT("Audio Loom");
}

void AAudioLoomActor::BeginPlay()
{
	Super::BeginPlay();
	// bPlayOnBeginPlay on the component (if set) runs from UAudioLoomComponent::BeginPlay
}
