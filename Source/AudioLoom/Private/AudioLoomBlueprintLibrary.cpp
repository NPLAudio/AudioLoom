// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomBlueprintLibrary.cpp
 * @brief Thin wrappers around `UOSCManager::OSCAddressIsValidPath` / `ConvertStringToOSCAddress`.
 */

#include "AudioLoomBlueprintLibrary.h"
#include "OSCManager.h"
#include "OSCAddress.h"

const TCHAR* UAudioLoomBlueprintLibrary::DefaultOscPrefix = TEXT("/audioloom");

bool UAudioLoomBlueprintLibrary::IsOscAddressValid(const FString& Address)
{
	if (Address.IsEmpty()) return false;
	FOSCAddress Addr = UOSCManager::ConvertStringToOSCAddress(Address);
	return UOSCManager::OSCAddressIsValidPath(Addr); // Engine enforces OSC 1.0 path rules
}

FString UAudioLoomBlueprintLibrary::NormalizeOscAddress(const FString& Address)
{
	FString S = Address.TrimStartAndEnd();
	if (S.IsEmpty()) return S;
	if (!S.StartsWith(TEXT("/"))) S = TEXT("/") + S; // user convenience; OSC paths must be absolute
	if (!IsOscAddressValid(S)) return FString();     // reject bad characters / structure
	return S;
}
