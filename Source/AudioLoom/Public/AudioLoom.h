// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * AudioLoom runtime module.
 * Provides WASAPI device routing and playback.
 */
class AUDIOLOOM_API FAudioLoomModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
