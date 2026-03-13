// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomEditor.h"
#include "UI/SAudioLoomPanel.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "AudioLoomWasapiComponentDetails.h"
#include "LevelEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FAudioLoomEditorModule"

const FName FAudioLoomEditorModule::AudioLoomTabName = TEXT("AudioLoomPanel");

void FAudioLoomEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		"AudioLoomWasapiComponent",
		FOnGetDetailCustomizationInstance::CreateStatic(&FAudioLoomWasapiComponentDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	RegisterTab();
}

void FAudioLoomEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("AudioLoomWasapiComponent");
	}

	if (FGlobalTabmanager::Get()->HasTabSpawner(AudioLoomTabName))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AudioLoomTabName);
	}
}

void FAudioLoomEditorModule::RegisterTab()
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		AudioLoomTabName,
		FOnSpawnTab::CreateRaw(this, &FAudioLoomEditorModule::SpawnTab))
		.SetDisplayName(LOCTEXT("AudioLoomTabTitle", "Audio Loom"))
		.SetTooltipText(LOCTEXT("AudioLoomTabTooltip", "Manage WASAPI channel routing for all Audio Loom components in the world"))
		.SetGroup(MenuStructure.GetLevelEditorCategory());
}

void FAudioLoomEditorModule::OpenManagementWindow()
{
	FGlobalTabmanager::Get()->TryInvokeTab(AudioLoomTabName);
}

TSharedRef<SDockTab> FAudioLoomEditorModule::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SAudioLoomPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAudioLoomEditorModule, AudioLoomEditor)
