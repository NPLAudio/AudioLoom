// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomComponentDetails.h
 * @brief Details panel customization for `UAudioLoomComponent` — device/channel combos, OSC validation, preview buttons.
 *
 * Registered in `FAudioLoomEditorModule::StartupModule` via `RegisterCustomClassLayout("AudioLoomComponent", …)`.
 */

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/** Property editor customization: replaces raw **DeviceId** string with device dropdown + channel menu. */
class FAudioLoomComponentDetails : public IDetailCustomization
{
public:
	/** Factory passed to `RegisterCustomClassLayout` — PropertyEditor owns the shared ref lifetime. */
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
