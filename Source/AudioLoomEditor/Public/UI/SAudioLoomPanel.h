// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "WasapiDeviceInfo.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class UAudioLoomComponent;

/**
 * Audio Loom management window.
 * Lists all Audio Loom components/actors in the world with status, routing, and playback controls.
 */
class AUDIOLOOMEDITOR_API SAudioLoomPanel : public SCompoundWidget
{
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

	TSharedRef<ITableRow> GenerateComponentRow(
		TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	FReply OnSelectInViewport(TWeakObjectPtr<UAudioLoomComponent> Component);
	FReply OnCheckPortClicked();
	FReply OnStartStopOscClicked();

	TSharedPtr<STextBlock> PortStatusText;
};
