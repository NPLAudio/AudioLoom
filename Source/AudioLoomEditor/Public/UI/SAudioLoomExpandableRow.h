// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"
#include "WasapiDeviceInfo.h"
#include "Widgets/SCompoundWidget.h"

class UAudioLoomComponent;
class SAudioLoomPanel;

/**
 * Expandable row for a single Audio Loom component.
 * Header: actor name, status, sound name (compact, always visible).
 * Body: full controls (sound picker, device, channel, loop, begin, OSC, play/stop).
 */
class AUDIOLOOMEDITOR_API SAudioLoomExpandableRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioLoomExpandableRow) {}
		SLATE_ARGUMENT(TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>>, Item)
		SLATE_ARGUMENT(const TArray<FWasapiDeviceInfo>*, Devices)
		SLATE_ARGUMENT(TWeakPtr<SAudioLoomPanel>, Panel)
		SLATE_ARGUMENT(bool, InitiallyExpanded)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>> Item;
	const TArray<FWasapiDeviceInfo>* Devices = nullptr;
	TWeakPtr<SAudioLoomPanel> Panel;
};
