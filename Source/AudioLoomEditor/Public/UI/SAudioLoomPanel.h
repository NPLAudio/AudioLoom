// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class UAudioLoomWasapiComponent;

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

	TArray<TWeakObjectPtr<UAudioLoomWasapiComponent>> ComponentList;
	TArray<TSharedPtr<TWeakObjectPtr<UAudioLoomWasapiComponent>>> ListViewItems;
	TSharedPtr<SListView<TSharedPtr<TWeakObjectPtr<UAudioLoomWasapiComponent>>>> ListView;

	TSharedRef<ITableRow> GenerateComponentRow(
		TSharedPtr<TWeakObjectPtr<UAudioLoomWasapiComponent>> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	FReply OnSelectInViewport(TWeakObjectPtr<UAudioLoomWasapiComponent> Component);
	FReply OnCheckPortClicked();
	FReply OnStartStopOscClicked();

	TSharedPtr<STextBlock> PortStatusText;
};
