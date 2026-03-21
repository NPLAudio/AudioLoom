// Copyright (c) 2026 AudioLoom Contributors.

#pragma once

#include "CoreMinimal.h"

class UAudioLoomComponent;
class UWorld;

/** Import/export channel routing as CSV (UTF-8). Editor module only. */
struct FAudioLoomRoutingCsv
{
	static FString BuildRoutingCsv(UWorld* World);

	/** Applies rows to matching components. Uses FScopedTransaction when GEditor is non-null. */
	static bool ApplyRoutingCsv(UWorld* World, const FString& CsvText, int32& OutAppliedCount, FString& OutErrorsAndWarnings);
};
