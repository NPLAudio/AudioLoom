// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "WasapiDeviceInfo.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class UAudioLoomComponent;
class SAudioLoomExpandableRow;
class SAudioLoomPanel;

/**
 * Audio Loom management window.
 * Lists all Audio Loom components/actors in the world with status, routing, and playback controls.
 * Uses SListView with SExpandableArea per component (compact header, expandable body).
 */
class AUDIOLOOMEDITOR_API SAudioLoomPanel : public SCompoundWidget
{
	friend class SAudioLoomExpandableRow;
public:
	SLATE_BEGIN_ARGS(SAudioLoomPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	UWorld* GetCurrentWorld() const;
	void RebuildComponentList();
	bool HasComponentListChanged() const;
	EActiveTimerReturnType OnRefreshTimer(double InCurrentTime, float InDeltaTime);
	FReply OnRefreshClicked();

	TArray<TWeakObjectPtr<UAudioLoomComponent>> ComponentList;
	TArray<TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>>> ListViewItems;
	TSharedPtr<SListView<TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>>>> ListView;
	TArray<FWasapiDeviceInfo> CachedDevices;

	TSharedRef<ITableRow> GenerateRow(
		TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	FReply OnSelectInViewport(TWeakObjectPtr<UAudioLoomComponent> Component);
	FReply OnCheckPortClicked();
	FReply OnStartStopOscClicked();

	TSharedPtr<STextBlock> PortStatusText;
};
