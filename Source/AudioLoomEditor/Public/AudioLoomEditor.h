// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

/**
 * AudioLoom editor module.
 * Provides details customization, management window, and device enumeration UI.
 */
class AUDIOLOOMEDITOR_API FAudioLoomEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void OpenManagementWindow();

private:
	void RegisterTab();
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
	static const FName AudioLoomTabName;
};
