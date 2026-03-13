// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AudioLoomOscSettings.generated.h"

/**
 * Persistent OSC configuration for AudioLoom.
 * Access via Project Settings > Audio Loom > OSC.
 */
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "Audio Loom OSC"))
class AUDIOLOOM_API UAudioLoomOscSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAudioLoomOscSettings();

	/** UDP port for OSC server (receiving triggers). Default 9000. */
	UPROPERTY(Config, EditAnywhere, Category = "OSC")
	int32 ListenPort = 9000;

	/** Target IP for OSC monitoring (sending play/stop state). */
	UPROPERTY(Config, EditAnywhere, Category = "OSC")
	FString SendIP = TEXT("127.0.0.1");

	/** Target port for OSC monitoring. */
	UPROPERTY(Config, EditAnywhere, Category = "OSC")
	int32 SendPort = 9001;

	virtual FName GetCategoryName() const override;
};
