// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoom.cpp
 * @brief Implements `FAudioLoomModule` and defines `LogAudioLoom`.
 *
 * Extend `StartupModule` / `ShutdownModule` here if you need one-time runtime setup
 * (e.g. third-party SDK init). Most AudioLoom logic stays in components and subsystems.
 */

#include "AudioLoom.h"

DEFINE_LOG_CATEGORY(LogAudioLoom); // allocates the actual `LogAudioLoom` category at startup

#define LOCTEXT_NAMESPACE "FAudioLoomModule"

void FAudioLoomModule::StartupModule()
{
	// Intentionally empty: components/subsystems self-register via UObject / module deps
}

void FAudioLoomModule::ShutdownModule()
{
	// Nothing to tear down yet; UObject GC handles component/backend lifetime
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAudioLoomModule, AudioLoom) // links this DLL as Unreal module named "AudioLoom"
