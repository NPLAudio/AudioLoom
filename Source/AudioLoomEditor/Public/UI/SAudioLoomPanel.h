// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file SAudioLoomPanel.h
 * @brief **Window → Audio Loom** nomad tab: OSC controls, scrollable list of all `UAudioLoomComponent` in the world.
 *
 * **Refresh model**: `RegisterActiveTimer` (~2s) calls `HasComponentListChanged`; on change, `RebuildComponentList`.
 * While OSC is listening, the timer also calls `UAudioLoomOscSubsystem::RebuildComponentRegistry` so new
 * components get trigger paths.
 *
 * **Rows**: `GenerateRow` builds `SAudioLoomExpandableRow` (friend: calls `OnSelectInViewport`).
 */

#pragma once

#include "CoreMinimal.h"
#include "AudioOutputDeviceInfo.h"
#include "Delegates/Delegate.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class UAudioLoomComponent;
class SAudioLoomExpandableRow;
class SAudioLoomPanel;

/**
 * Audio Loom management window (Slate root for the dock tab).
 */
class AUDIOLOOMEDITOR_API SAudioLoomPanel : public SCompoundWidget
{
	friend class SAudioLoomExpandableRow; // row header calls OnSelectInViewport (private)
public:
	SLATE_BEGIN_ARGS(SAudioLoomPanel) {} // no constructor args; panel is self-contained
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAudioLoomPanel() override;

private:
	/** Editor world or `GEditor->PlayWorld` when PIE is active — same world the list enumerates. */
	UWorld* GetCurrentWorld() const;
	/** Full scan of actors → `UAudioLoomComponent`; refreshes `CachedDevices` for row dropdowns. */
	void RebuildComponentList();
	/** Cheap count/validity check vs `ComponentList` to detect spawn/destroy without polling every property. */
	bool HasComponentListChanged() const;
	/** Periodic: list churn + OSC registry refresh when server is up. */
	EActiveTimerReturnType OnRefreshTimer(double InCurrentTime, float InDeltaTime);
	/** Rebind list to PlayWorld / editor world when PIE starts or stops. */
	void OnPIEWorldChanged(bool bArg);
	FReply OnRefreshClicked();
	FReply OnExportCsvClicked();
	FReply OnImportCsvClicked();

	TArray<TWeakObjectPtr<UAudioLoomComponent>> ComponentList;
	/** `SListView` source: shared pointers so Slate keeps stable row identity. */
	TArray<TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>>> ListViewItems;
	TSharedPtr<SListView<TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>>>> ListView;
	/** Snapshot from `FAudioOutputDeviceEnumerator::GetOutputDevices` for device name resolution in rows. */
	TArray<FAudioOutputDeviceInfo> CachedDevices;
	/** World used when `ComponentList` was last built — must match `GetCurrentWorld()` (editor vs PIE). */
	TWeakObjectPtr<UWorld> ComponentListWorld;

	TSharedRef<ITableRow> GenerateRow(
		TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	/** Select owner actor and frame viewport (invoked from expandable row header). */
	FReply OnSelectInViewport(TWeakObjectPtr<UAudioLoomComponent> Component);
	FReply OnCheckPortClicked();
	FReply OnStartStopOscClicked();

	TSharedPtr<STextBlock> PortStatusText;

	FDelegateHandle PostPIEStartedHandle;
	FDelegateHandle EndPIEHandle;
};
