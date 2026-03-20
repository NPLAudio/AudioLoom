// Copyright (c) 2026 AudioLoom Contributors.

/** @file AudioLoomOscSettings.cpp — default category name for settings UI. */

#include "AudioLoomOscSettings.h"

UAudioLoomOscSettings::UAudioLoomOscSettings()
{
	CategoryName = TEXT("Audio Loom"); // top-level group in Project Settings navigation
}

FName UAudioLoomOscSettings::GetCategoryName() const
{
	return TEXT("Audio Loom"); // must match for settings UI to nest this class correctly
}
