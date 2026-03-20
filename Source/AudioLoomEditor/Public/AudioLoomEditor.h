// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomEditor.h
 * @brief Editor module: property details, nomad tab **Window → Audio Loom**.
 *
 * Registration flow (`AudioLoomEditorModule.cpp`):
 *   - `RegisterCustomClassLayout("AudioLoomComponent", …)` → `FAudioLoomComponentDetails`
 *   - `RegisterNomadTabSpawner` → `SAudioLoomPanel` inside `SDockTab`
 *
 * To add new editor-only UI, prefer new Slate widgets under `Source/AudioLoomEditor/`
 * and hook them from here or from menu extenders.
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

/**
 * AudioLoom editor module (`IMPLEMENT_MODULE` in AudioLoomEditorModule.cpp).
 * Owns the nomad tab spawner and registers `UAudioLoomComponent` detail customization.
 */
class AUDIOLOOMEDITOR_API FAudioLoomEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Brings **Window → Audio Loom** to front (or creates the nomad tab). */
	void OpenManagementWindow();

private:
	void RegisterTab(); // adds spawner to global tab manager + workspace menu
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args); // factory: wraps SAudioLoomPanel
	static const FName AudioLoomTabName; // internal ID; must match unregister on shutdown
};
