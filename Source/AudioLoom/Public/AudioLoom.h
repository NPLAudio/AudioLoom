// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

AUDIOLOOM_API DECLARE_LOG_CATEGORY_EXTERN(LogAudioLoom, Log, All);

/**
 * AudioLoom runtime module.
 * Provides audio device routing and playback (Windows/macOS).
 */
class AUDIOLOOM_API FAudioLoomModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
