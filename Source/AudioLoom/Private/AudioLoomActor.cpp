// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomActor.h"
#include "AudioLoomComponent.h"
#include "Components/SceneComponent.h"

AAudioLoomActor::AAudioLoomActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	AudioLoomComponent = CreateDefaultSubobject<UAudioLoomComponent>(TEXT("AudioLoomComponent"));
}

FString AAudioLoomActor::GetDefaultActorLabel() const
{
	return TEXT("Audio Loom");
}

void AAudioLoomActor::BeginPlay()
{
	Super::BeginPlay();
}
