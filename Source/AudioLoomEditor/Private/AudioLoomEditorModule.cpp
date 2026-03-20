// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomEditorModule.cpp
 * @brief Startup/shutdown: property editor layout + **Audio Loom** nomad tab.
 *
 * - **Shutdown** must unregister customizations and tab spawners to avoid leaks when
 *   hot-reloading or disabling the plugin.
 * - `OpenManagementWindow()` is the public entry used by menus or other code to
 *   focus the Audio Loom panel (`TryInvokeTab`).
 */

#include "AudioLoomEditor.h"
#include "UI/SAudioLoomPanel.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "AudioLoomComponentDetails.h"
#include "LevelEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FAudioLoomEditorModule"

const FName FAudioLoomEditorModule::AudioLoomTabName = TEXT("AudioLoomPanel");

void FAudioLoomEditorModule::StartupModule()
{
	// --- Details panel: device/channel combos and OSC validation for UAudioLoomComponent ---
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		"AudioLoomComponent",
		FOnGetDetailCustomizationInstance::CreateStatic(&FAudioLoomComponentDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged(); // refresh any open Details panels picking up the new layout

	RegisterTab();
}

void FAudioLoomEditorModule::ShutdownModule()
{
	// Hot-reload / disable-plugin: only unregister if the module is still there
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("AudioLoomComponent");
	}

	if (FGlobalTabmanager::Get()->HasTabSpawner(AudioLoomTabName))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AudioLoomTabName); // avoids dangling spawner pointer to `this`
	}
}

void FAudioLoomEditorModule::RegisterTab()
{
	// Nomad tab: lives under Level Editor category; content is SAudioLoomPanel
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		AudioLoomTabName,
		FOnSpawnTab::CreateRaw(this, &FAudioLoomEditorModule::SpawnTab))
		.SetDisplayName(LOCTEXT("AudioLoomTabTitle", "Audio Loom"))
		.SetTooltipText(LOCTEXT("AudioLoomTabTooltip", "Manage audio channel routing for all Audio Loom components in the world"))
		.SetGroup(MenuStructure.GetLevelEditorCategory());
}

void FAudioLoomEditorModule::OpenManagementWindow()
{
	FGlobalTabmanager::Get()->TryInvokeTab(AudioLoomTabName); // creates tab on first use
}

TSharedRef<SDockTab> FAudioLoomEditorModule::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab) // user can dock / float like Content Browser
		[
			SNew(SAudioLoomPanel) // entire UI tree lives inside this widget
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAudioLoomEditorModule, AudioLoomEditor)
