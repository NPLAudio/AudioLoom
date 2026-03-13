// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomBlueprintLibrary.h"
#include "OSCManager.h"
#include "OSCAddress.h"

const TCHAR* UAudioLoomBlueprintLibrary::DefaultOscPrefix = TEXT("/audioloom");

bool UAudioLoomBlueprintLibrary::IsOscAddressValid(const FString& Address)
{
	if (Address.IsEmpty()) return false;
	FOSCAddress Addr = UOSCManager::ConvertStringToOSCAddress(Address);
	return UOSCManager::OSCAddressIsValidPath(Addr);
}

FString UAudioLoomBlueprintLibrary::NormalizeOscAddress(const FString& Address)
{
	FString S = Address.TrimStartAndEnd();
	if (S.IsEmpty()) return S;
	if (!S.StartsWith(TEXT("/"))) S = TEXT("/") + S;
	if (!IsOscAddressValid(S)) return FString();
	return S;
}
