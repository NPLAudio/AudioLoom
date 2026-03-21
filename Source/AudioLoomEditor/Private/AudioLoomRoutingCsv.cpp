// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomRoutingCsv.h"
#include "AudioLoomComponent.h"
#include "AudioLoomOscSubsystem.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace AudioLoomRoutingCsvInternal
{
	/** ComponentName = UObject name (e.g. AudioLoomComponent_1); disambiguates multiple Audio Loom on one actor. */
	static const TCHAR* HeaderRow = TEXT(
		"ActorName,ActorLabel,ComponentName,ComponentIndex,SoundWave,DeviceId,OutputChannel,OscAddress,OscBase,OscPlay,OscStop,OscLoop,OscState");

	static FString TrimTrailingSlashes(FString In)
	{
		while (In.Len() > 0 && In[In.Len() - 1] == TEXT('/'))
		{
			In.RemoveAt(In.Len() - 1);
		}
		return In;
	}

	static FString OscJoin(const FString& Base, const TCHAR* Suffix)
	{
		if (Base.IsEmpty())
		{
			return FString();
		}
		return TrimTrailingSlashes(Base) + FString(Suffix);
	}

	static FString EscapeField(const FString& S)
	{
		const bool bNeedQuote = S.Contains(TEXT(",")) || S.Contains(TEXT("\"")) || S.Contains(TEXT("\n")) || S.Contains(TEXT("\r"));
		if (!bNeedQuote)
		{
			return S;
		}
		FString Out = TEXT("\"");
		for (int32 i = 0; i < S.Len(); ++i)
		{
			const TCHAR C = S[i];
			if (C == TEXT('"'))
			{
				Out += TEXT("\"\"");
			}
			else
			{
				Out.AppendChar(C);
			}
		}
		Out += TEXT("\"");
		return Out;
	}

	static TArray<FString> ParseCsvLine(const FString& Line)
	{
		TArray<FString> Fields;
		FString Current;
		bool bInQuotes = false;
		for (int32 i = 0; i < Line.Len(); ++i)
		{
			const TCHAR C = Line[i];
			if (bInQuotes)
			{
				if (C == TEXT('"'))
				{
					if (i + 1 < Line.Len() && Line[i + 1] == TEXT('"'))
					{
						Current.AppendChar(TEXT('"'));
						++i;
					}
					else
					{
						bInQuotes = false;
					}
				}
				else
				{
					Current.AppendChar(C);
				}
			}
			else
			{
				if (C == TEXT(','))
				{
					Fields.Add(Current);
					Current.Reset();
				}
				else if (C == TEXT('"'))
				{
					bInQuotes = true;
				}
				else
				{
					Current.AppendChar(C);
				}
			}
		}
		Fields.Add(Current);
		return Fields;
	}

	static void SortComponentsForActor(TArray<UAudioLoomComponent*>& Comps)
	{
		Comps.Sort([](const UAudioLoomComponent& A, const UAudioLoomComponent& B)
		{
			return A.GetFName().ToString() < B.GetFName().ToString();
		});
	}

	static void GatherSortedComponents(AActor& Actor, TArray<UAudioLoomComponent*>& OutSorted)
	{
		OutSorted.Reset();
		// TInlineComponentArray is the supported way to collect all instances of a component class on an actor.
		TInlineComponentArray<UAudioLoomComponent*> Inline(&Actor);
		OutSorted.Reserve(Inline.Num());
		for (UAudioLoomComponent* C : Inline)
		{
			if (IsValid(C))
			{
				OutSorted.Add(C);
			}
		}
		SortComponentsForActor(OutSorted);
	}

	static UAudioLoomComponent* FindComponentByObjectName(AActor& Actor, const FString& ComponentObjectName)
	{
		TInlineComponentArray<UAudioLoomComponent*> Inline(&Actor);
		for (UAudioLoomComponent* C : Inline)
		{
			if (IsValid(C) && C->GetName() == ComponentObjectName)
			{
				return C;
			}
		}
		return nullptr;
	}

	static TMap<FName, int32> BuildColumnIndexMap(const TArray<FString>& HeaderFields)
	{
		TMap<FName, int32> Map;
		for (int32 i = 0; i < HeaderFields.Num(); ++i)
		{
			const FString Trimmed = HeaderFields[i].TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				Map.Add(FName(*Trimmed), i);
			}
		}
		return Map;
	}

	static FString GetColumn(const TArray<FString>& Fields, const TMap<FName, int32>& Cols, const TCHAR* ColumnName)
	{
		if (const int32* Idx = Cols.Find(FName(ColumnName)))
		{
			if (*Idx >= 0 && *Idx < Fields.Num())
			{
				return Fields[*Idx];
			}
		}
		return FString();
	}

	static bool HeaderHasColumn(const TMap<FName, int32>& Cols, const TCHAR* ColumnName)
	{
		return Cols.Contains(FName(ColumnName));
	}

	/** True if the row has enough fields for this column (avoids treating short rows as empty OSC override). */
	static bool TryGetColumn(const TArray<FString>& Fields, const TMap<FName, int32>& Cols, const TCHAR* ColumnName, FString& OutValue)
	{
		if (const int32* Idx = Cols.Find(FName(ColumnName)))
		{
			if (*Idx >= 0 && *Idx < Fields.Num())
			{
				OutValue = Fields[*Idx];
				return true;
			}
		}
		return false;
	}
} // namespace

FString FAudioLoomRoutingCsv::BuildRoutingCsv(UWorld* World)
{
	using namespace AudioLoomRoutingCsvInternal;
	if (!World)
	{
		return FString();
	}

	FString Lines;
	Lines.Reserve(4096);
	Lines += HeaderRow;
	Lines += LINE_TERMINATOR;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}
		TArray<UAudioLoomComponent*> Comps;
		GatherSortedComponents(*Actor, Comps);
		const FString ActorName = Actor->GetName();
		const FString ActorLabel = Actor->GetActorLabel();
		for (int32 Idx = 0; Idx < Comps.Num(); ++Idx)
		{
			UAudioLoomComponent* Comp = Comps[Idx];
			if (!IsValid(Comp))
			{
				continue;
			}
			const FString SoundPath = Comp->SoundWave ? Comp->SoundWave->GetPathName() : FString();
			Lines += EscapeField(ActorName);
			Lines += TEXT(",");
			Lines += EscapeField(ActorLabel);
			Lines += TEXT(",");
			Lines += EscapeField(Comp->GetName());
			Lines += TEXT(",");
			Lines += FString::FromInt(Idx);
			Lines += TEXT(",");
			Lines += EscapeField(SoundPath);
			Lines += TEXT(",");
			Lines += EscapeField(Comp->DeviceId);
			Lines += TEXT(",");
			Lines += FString::FromInt(Comp->OutputChannel);
			{
				const FString OscBase = Comp->GetOscAddress();
				const FString OscOverride = Comp->OscAddress;
				Lines += TEXT(",");
				Lines += EscapeField(OscOverride);
				Lines += TEXT(",");
				Lines += EscapeField(OscBase);
				Lines += TEXT(",");
				Lines += EscapeField(OscJoin(OscBase, TEXT("/play")));
				Lines += TEXT(",");
				Lines += EscapeField(OscJoin(OscBase, TEXT("/stop")));
				Lines += TEXT(",");
				Lines += EscapeField(OscJoin(OscBase, TEXT("/loop")));
				Lines += TEXT(",");
				Lines += EscapeField(OscJoin(OscBase, TEXT("/state")));
			}
			Lines += LINE_TERMINATOR;
		}
	}
	return Lines;
}

bool FAudioLoomRoutingCsv::ApplyRoutingCsv(UWorld* World, const FString& CsvText, int32& OutAppliedCount, FString& OutErrorsAndWarnings)
{
	using namespace AudioLoomRoutingCsvInternal;
	OutAppliedCount = 0;
	OutErrorsAndWarnings.Reset();
	bool bAnyOscAddressChanged = false;

	if (!World)
	{
		OutErrorsAndWarnings = TEXT("No world.");
		return false;
	}

	TArray<FString> Lines;
	CsvText.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);

	if (Lines.Num() < 2)
	{
		OutErrorsAndWarnings = TEXT("CSV has no data rows.");
		return false;
	}

	const TArray<FString> HeaderFields = ParseCsvLine(Lines[0]);
	const TMap<FName, int32> Cols = BuildColumnIndexMap(HeaderFields);
	const bool bHasComponentName = HeaderHasColumn(Cols, TEXT("ComponentName"));
	const bool bLegacy6 = !bHasComponentName && HeaderHasColumn(Cols, TEXT("ComponentIndex")) && HeaderFields.Num() >= 6;

	if (!HeaderHasColumn(Cols, TEXT("ActorName")) || !HeaderHasColumn(Cols, TEXT("SoundWave"))
		|| !HeaderHasColumn(Cols, TEXT("DeviceId")) || !HeaderHasColumn(Cols, TEXT("OutputChannel")))
	{
		OutErrorsAndWarnings = TEXT("Invalid header: need ActorName, SoundWave, DeviceId, OutputChannel, and either ComponentName+ComponentIndex (v2) or ComponentIndex only (v1).");
		return false;
	}
	if (!bLegacy6 && !bHasComponentName)
	{
		OutErrorsAndWarnings = TEXT("Invalid header: missing ComponentName column (re-export with current plugin) or use a v1 CSV with ComponentIndex.");
		return false;
	}
	if (!HeaderHasColumn(Cols, TEXT("ComponentIndex")))
	{
		OutErrorsAndWarnings = TEXT("Invalid header: missing ComponentIndex column.");
		return false;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("AudioLoomRoutingCsv", "ImportRouting", "Import AudioLoom routing CSV"));

	for (int32 LineIdx = 1; LineIdx < Lines.Num(); ++LineIdx)
	{
		const FString& Line = Lines[LineIdx];
		if (Line.IsEmpty())
		{
			continue;
		}

		const TArray<FString> Fields = ParseCsvLine(Line);
		const int32 MinCols = bLegacy6 ? 6 : 7;
		if (Fields.Num() < MinCols)
		{
			OutErrorsAndWarnings += FString::Printf(TEXT("Line %d: expected at least %d columns, got %d.\n"), LineIdx + 1, MinCols, Fields.Num());
			continue;
		}

		const FString ActorName = GetColumn(Fields, Cols, TEXT("ActorName"));
		const FString ActorLabel = GetColumn(Fields, Cols, TEXT("ActorLabel"));
		const FString ComponentName = bHasComponentName ? GetColumn(Fields, Cols, TEXT("ComponentName")) : FString();
		const int32 ComponentIndex = FCString::Atoi(*GetColumn(Fields, Cols, TEXT("ComponentIndex")));
		const FString SoundWavePath = GetColumn(Fields, Cols, TEXT("SoundWave"));
		const FString DeviceId = GetColumn(Fields, Cols, TEXT("DeviceId"));
		const int32 OutputChannel = FCString::Atoi(*GetColumn(Fields, Cols, TEXT("OutputChannel")));

		if (ActorName.IsEmpty())
		{
			OutErrorsAndWarnings += FString::Printf(TEXT("Line %d: ActorName is empty.\n"), LineIdx + 1);
			continue;
		}

		TArray<AActor*> Matches;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!IsValid(A) || A->GetName() != ActorName)
			{
				continue;
			}
			if (!ActorLabel.IsEmpty() && A->GetActorLabel() != ActorLabel)
			{
				continue;
			}
			Matches.Add(A);
		}

		if (Matches.Num() == 0)
		{
			OutErrorsAndWarnings += FString::Printf(TEXT("Line %d: no actor matched Name=\"%s\" Label=\"%s\".\n"),
				LineIdx + 1, *ActorName, *ActorLabel);
			continue;
		}
		if (Matches.Num() > 1)
		{
			OutErrorsAndWarnings += FString::Printf(TEXT("Line %d: ambiguous: %d actors named \"%s\" (add unique ActorLabel in level and CSV).\n"),
				LineIdx + 1, Matches.Num(), *ActorName);
			continue;
		}

		AActor* MatchedActor = Matches[0];
		TArray<UAudioLoomComponent*> Comps;
		GatherSortedComponents(*MatchedActor, Comps);

		UAudioLoomComponent* Comp = nullptr;
		if (!ComponentName.IsEmpty())
		{
			Comp = FindComponentByObjectName(*MatchedActor, ComponentName);
			if (!Comp)
			{
				OutErrorsAndWarnings += FString::Printf(TEXT("Line %d: no Audio Loom component named \"%s\" on actor (check spelling or re-export).\n"),
					LineIdx + 1, *ComponentName);
				continue;
			}
		}
		else
		{
			if (ComponentIndex < 0 || ComponentIndex >= Comps.Num())
			{
				OutErrorsAndWarnings += FString::Printf(TEXT("Line %d: ComponentIndex %d out of range (actor has %d Audio Loom component(s)).\n"),
					LineIdx + 1, ComponentIndex, Comps.Num());
				continue;
			}
			Comp = Comps[ComponentIndex];
		}
		if (!IsValid(Comp))
		{
			continue;
		}

		USoundWave* NewWave = nullptr;
		if (!SoundWavePath.IsEmpty())
		{
			NewWave = LoadObject<USoundWave>(nullptr, *SoundWavePath);
			if (!NewWave)
			{
				NewWave = Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, *SoundWavePath));
			}
			if (!NewWave)
			{
				OutErrorsAndWarnings += FString::Printf(TEXT("Line %d: could not load SoundWave \"%s\" (skipped row).\n"), LineIdx + 1, *SoundWavePath);
				continue;
			}
		}

		const bool bWasPlaying = Comp->IsPlaying();
		const FString OldDeviceId = Comp->DeviceId;
		const int32 OldOutputChannel = Comp->OutputChannel;
		TObjectPtr<USoundWave> OldSoundWave = Comp->SoundWave;

		Comp->Modify();
		if (UPackage* Pkg = Comp->GetOutermost())
		{
			Pkg->SetDirtyFlag(true);
		}

		Comp->SoundWave = NewWave;
		Comp->DeviceId = DeviceId;
		Comp->OutputChannel = FMath::Max(0, OutputChannel);

		if (HeaderHasColumn(Cols, TEXT("OscAddress")))
		{
			FString OscIn;
			if (TryGetColumn(Fields, Cols, TEXT("OscAddress"), OscIn))
			{
				OscIn.TrimStartAndEndInline();
				if (OscIn.IsEmpty())
				{
					if (!Comp->OscAddress.IsEmpty())
					{
						Comp->SetOscAddress(TEXT(""));
						bAnyOscAddressChanged = true;
					}
				}
				else if (!Comp->SetOscAddress(OscIn))
				{
					OutErrorsAndWarnings += FString::Printf(TEXT("Line %d: invalid OscAddress \"%s\" (OSC override not applied; routing applied).\n"),
						LineIdx + 1, *OscIn);
				}
				else
				{
					bAnyOscAddressChanged = true;
				}
			}
		}

#if WITH_EDITOR
		// Match PostEditChangeProperty: restart backend when routing changes while playing.
		const bool bRoutingChanged = (OldDeviceId != DeviceId) || (OldOutputChannel != OutputChannel) || (OldSoundWave != NewWave);
		if (bWasPlaying && Comp->SoundWave && bRoutingChanged)
		{
			Comp->Stop();
			Comp->Play();
		}
#endif

		++OutAppliedCount;
	}

	if (bAnyOscAddressChanged && World)
	{
		if (UAudioLoomOscSubsystem* Sub = World->GetSubsystem<UAudioLoomOscSubsystem>())
		{
			if (Sub->IsListening())
			{
				Sub->RebuildComponentRegistry();
			}
		}
	}

	return OutAppliedCount > 0;
}
