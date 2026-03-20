// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file SAudioLoomExpandableRow.h
 * @brief One `SExpandableArea` per component: mirrors Details categories (routing, playback, OSC).
 *
 * Mutates `UAudioLoomComponent` directly (`Modify()`); mirrors **Window** UX with **Details** panel.
 * Low-latency / buffer spin boxes are `PLATFORM_WINDOWS` only (matches component availability).
 */

#pragma once

#include "CoreMinimal.h"
#include "AudioOutputDeviceInfo.h"
#include "Widgets/SCompoundWidget.h"

class UAudioLoomComponent;
class SAudioLoomPanel;

/**
 * Expandable row: header (actor / playing / sound) + body (full editor controls).
 */
class AUDIOLOOMEDITOR_API SAudioLoomExpandableRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioLoomExpandableRow) {}
		SLATE_ARGUMENT(TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>>, Item) // row’s component (from list source)
		SLATE_ARGUMENT(const TArray<FAudioOutputDeviceInfo>*, Devices)       // device list for combo labels
		SLATE_ARGUMENT(TWeakPtr<SAudioLoomPanel>, Panel)                       // for “select in viewport” delegate
		SLATE_ARGUMENT(bool, InitiallyExpanded)                                // first row often expanded for discoverability
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>> Item; // list view item; shared for Slate identity
	const TArray<FAudioOutputDeviceInfo>* Devices = nullptr; // points at panel’s CachedDevices (do not copy)
	TWeakPtr<SAudioLoomPanel> Panel; // avoid circular shared_ptr with parent panel
};
