// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomWasapiActor.h"
#include "AudioLoomWasapiComponent.h"
#include "Components/SceneComponent.h"

AAudioLoomWasapiActor::AAudioLoomWasapiActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	WasapiComponent = CreateDefaultSubobject<UAudioLoomWasapiComponent>(TEXT("WasapiComponent"));
}

void AAudioLoomWasapiActor::BeginPlay()
{
	Super::BeginPlay();
}
