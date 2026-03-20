// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoom.h
 * @brief Runtime module entry point and log category for the AudioLoom plugin.
 *
 * The runtime module (`AudioLoom`) is intentionally thin: it registers the module and
 * exposes `LogAudioLoom` for diagnostics. Actual behavior lives in:
 *   - `UAudioLoomComponent` — actor component that loads PCM and drives `FAudioLoomPlaybackBackend`
 *   - `FAudioLoomPlaybackBackend` / `FAudioOutputDeviceEnumerator` — OS audio output and device listing
 *   - `UAudioLoomOscSubsystem` — OSC server/client integration (requires Engine OSC plugin)
 *
 * @see FAudioLoomEditorModule for editor UI (details panel, Window → Audio Loom).
 */

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

AUDIOLOOM_API DECLARE_LOG_CATEGORY_EXTERN(LogAudioLoom, Log, All); // use: UE_LOG(LogAudioLoom, Log, TEXT("…"));

/**
 * AudioLoom runtime module (`IMPLEMENT_MODULE` in AudioLoom.cpp).
 * Loads with the game/editor; no custom tick or subsystem registration here.
 */
class AUDIOLOOM_API FAudioLoomModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;  // hook one-time init here if you add global runtime services
	virtual void ShutdownModule() override; // reverse order of anything started in StartupModule
};
