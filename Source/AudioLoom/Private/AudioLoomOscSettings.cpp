// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomOscSettings.h"

UAudioLoomOscSettings::UAudioLoomOscSettings()
{
	CategoryName = TEXT("Audio Loom");
}

FName UAudioLoomOscSettings::GetCategoryName() const
{
	return TEXT("Audio Loom");
}
