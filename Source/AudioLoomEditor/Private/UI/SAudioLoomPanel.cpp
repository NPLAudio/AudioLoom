// Copyright (c) 2026 AudioLoom Contributors.

#include "UI/SAudioLoomPanel.h"
#include "AudioLoomWasapiComponent.h"
#include "WasapiDeviceEnumerator.h"
#include "WasapiDeviceInfo.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SAudioLoomPanel"

void SAudioLoomPanel::Construct(const FArguments& InArgs)
{
	RegisterActiveTimer(2.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SAudioLoomPanel::OnRefreshTimer));

	RebuildComponentList();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PanelTitle", "Audio Loom — Channel Routing"))
					.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.OnClicked(this, &SAudioLoomPanel::OnRefreshClicked)
					.ToolTipText(LOCTEXT("RefreshTip", "Refresh component list"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.2f).Padding(4.f, 2.f)
					[SNew(STextBlock).Text(LOCTEXT("ColActor", "Actor")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 2.f)
					[SNew(STextBlock).Text(LOCTEXT("ColSound", "Sound")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
				+ SHorizontalBox::Slot().FillWidth(1.2f).Padding(4.f, 2.f)
					[SNew(STextBlock).Text(LOCTEXT("ColDevice", "Device")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
				+ SHorizontalBox::Slot().FillWidth(0.6f).Padding(4.f, 2.f)
					[SNew(STextBlock).Text(LOCTEXT("ColChannel", "Ch")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
				+ SHorizontalBox::Slot().FillWidth(0.4f).Padding(4.f, 2.f)
					[SNew(STextBlock).Text(LOCTEXT("ColLoop", "Loop")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4.f, 2.f)
					[SNew(STextBlock).Text(LOCTEXT("ColBegin", "Begin")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4.f, 2.f)
					[SNew(STextBlock).Text(LOCTEXT("ColStatus", "Status")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
				+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(4.f, 2.f)
					[SNew(STextBlock).Text(LOCTEXT("ColControls", "Controls")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(ListView, SListView<TSharedPtr<TWeakObjectPtr<UAudioLoomWasapiComponent>>>)
					.ListItemsSource(&ListViewItems)
					.OnGenerateRow(this, &SAudioLoomPanel::GenerateComponentRow)
					.ItemHeight(44.f)
					.SelectionMode(ESelectionMode::None)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return FText::Format(
						LOCTEXT("CountFormat", "{0} component(s) in world"),
						FText::AsNumber(ListViewItems.Num()));
				})
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Font(FAppStyle::GetFontStyle("SmallFont"))
			]
		]
	];
}

UWorld* SAudioLoomPanel::GetCurrentWorld() const
{
	if (!GEditor) return nullptr;
	UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
	return World;
}

void SAudioLoomPanel::RebuildComponentList()
{
	ComponentList.Reset();
	ListViewItems.Reset();

	UWorld* World = GetCurrentWorld();
	if (!World) return;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UAudioLoomWasapiComponent*> Comps;
		It->GetComponents(Comps);
		for (UAudioLoomWasapiComponent* Comp : Comps)
		{
			if (IsValid(Comp))
			{
				ComponentList.Add(Comp);
				ListViewItems.Add(MakeShared<TWeakObjectPtr<UAudioLoomWasapiComponent>>(Comp));
			}
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

bool SAudioLoomPanel::HasComponentListChanged() const
{
	UWorld* World = GetCurrentWorld();
	if (!World) return false;

	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UAudioLoomWasapiComponent*> Comps;
		It->GetComponents(Comps);
		for (UAudioLoomWasapiComponent* Comp : Comps)
		{
			if (IsValid(Comp)) ++Count;
		}
	}
	if (Count != ComponentList.Num()) return true;

	int32 ValidCount = 0;
	for (const TWeakObjectPtr<UAudioLoomWasapiComponent>& W : ComponentList)
	{
		if (W.IsValid()) ++ValidCount;
	}
	return ValidCount != Count;
}

EActiveTimerReturnType SAudioLoomPanel::OnRefreshTimer(double InCurrentTime, float InDeltaTime)
{
	if (HasComponentListChanged())
	{
		RebuildComponentList();
	}
	return EActiveTimerReturnType::Continue;
}

FReply SAudioLoomPanel::OnRefreshClicked()
{
	RebuildComponentList();
	return FReply::Handled();
}

TSharedRef<ITableRow> SAudioLoomPanel::GenerateComponentRow(
	TSharedPtr<TWeakObjectPtr<UAudioLoomWasapiComponent>> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	UAudioLoomWasapiComponent* Comp = Item.IsValid() ? Item->Get() : nullptr;
	TWeakObjectPtr<UAudioLoomWasapiComponent> WeakComp = Comp;

	TArray<FWasapiDeviceInfo> Devices = FWasapiDeviceEnumerator::GetOutputDevices();

	return SNew(STableRow<TSharedPtr<TWeakObjectPtr<UAudioLoomWasapiComponent>>>, OwnerTable)
		.Padding(FMargin(2.f, 2.f))
		[
			SNew(SHorizontalBox)

			// Actor name
			+ SHorizontalBox::Slot()
			.FillWidth(1.2f)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text_Lambda([WeakComp]()
					{
						if (AActor* Owner = WeakComp.IsValid() ? WeakComp->GetOwner() : nullptr)
							return FText::FromString(Owner ? Owner->GetActorLabel() : TEXT("—"));
						return LOCTEXT("Invalid", "—");
					})
					.OnClicked_Lambda([this, WeakComp]() { return OnSelectInViewport(WeakComp); })
					.ToolTipText(LOCTEXT("SelectInViewport", "Select actor in viewport"))
				]
			]

			// Sound name
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakComp]()
				{
					if (WeakComp.IsValid() && WeakComp->SoundWave)
						return FText::FromString(WeakComp->SoundWave->GetName());
					return LOCTEXT("NoSound", "(none)");
				})
				.ToolTipText_Lambda([WeakComp]()
				{
					if (WeakComp.IsValid() && WeakComp->SoundWave)
						return FText::FromString(WeakComp->SoundWave->GetPathName());
					return FText();
				})
			]

			// Device dropdown
			+ SHorizontalBox::Slot()
			.FillWidth(1.2f)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(SComboButton)
				.OnGetMenuContent_Lambda([WeakComp, Devices]()
				{
					FMenuBuilder MenuBuilder(true, nullptr);
					MenuBuilder.AddMenuEntry(
						LOCTEXT("DefaultDevice", "Default Output"),
						LOCTEXT("DefaultDeviceTip", "Use system default"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([WeakComp]()
						{
							if (WeakComp.IsValid())
							{
								WeakComp->DeviceId = FString();
								WeakComp->Modify();
							}
						}))
					);
					for (const FWasapiDeviceInfo& D : Devices)
					{
						if (!D.bIsValid) continue;
						FString Id = D.DeviceId;
						FString Name = D.FriendlyName;
						MenuBuilder.AddMenuEntry(
							FText::FromString(Name),
							FText::FromString(Id),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([WeakComp, Id]()
							{
								if (WeakComp.IsValid())
								{
									WeakComp->DeviceId = Id;
									WeakComp->Modify();
								}
							}))
						);
					}
					return MenuBuilder.MakeWidget();
				})
				.ButtonContent()
				[
					SNew(STextBlock)
				.Text_Lambda([WeakComp]()
				{
					if (!WeakComp.IsValid()) return LOCTEXT("DefaultDevice", "Default Output");
					FString Id = WeakComp->DeviceId;
					if (Id.IsEmpty()) return LOCTEXT("DefaultDevice", "Default Output");
					FWasapiDeviceInfo Info = FWasapiDeviceEnumerator::GetDeviceById(Id);
					return Info.bIsValid ? FText::FromString(Info.FriendlyName) : FText::FromString(Id);
				})
					.Font(FAppStyle::GetFontStyle("SmallFont"))
				]
			]

				// Channel dropdown
			+ SHorizontalBox::Slot()
			.FillWidth(0.6f)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(SComboButton)
				.OnGetMenuContent_Lambda([WeakComp]()
				{
					FString DevId = WeakComp.IsValid() ? WeakComp->DeviceId : FString();
					FWasapiDeviceInfo Info = FWasapiDeviceEnumerator::GetDeviceById(DevId);
					int32 NumCh = Info.bIsValid ? Info.NumChannels : 8;
					FMenuBuilder MenuBuilder(true, nullptr);
					MenuBuilder.AddMenuEntry(
						LOCTEXT("ChannelAll", "All (0)"),
						FText(),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([WeakComp]()
						{
							if (WeakComp.IsValid()) { WeakComp->OutputChannel = 0; WeakComp->Modify(); }
						}))
					);
					for (int32 i = 1; i <= NumCh; ++i)
					{
						int32 Ch = i;
						MenuBuilder.AddMenuEntry(
							FText::FromString(FString::Printf(TEXT("Ch %d"), Ch)),
							FText(),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([WeakComp, Ch]()
							{
								if (WeakComp.IsValid()) { WeakComp->OutputChannel = Ch; WeakComp->Modify(); }
							}))
						);
					}
					return MenuBuilder.MakeWidget();
				})
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text_Lambda([WeakComp]()
					{
						if (!WeakComp.IsValid()) return LOCTEXT("ChannelAll", "All");
						int32 Ch = WeakComp->OutputChannel;
						return Ch == 0 ? LOCTEXT("ChannelAll", "All") : FText::FromString(FString::Printf(TEXT("Ch %d"), Ch));
					})
					.Font(FAppStyle::GetFontStyle("SmallFont"))
				]
			]

			// Loop checkbox
			+ SHorizontalBox::Slot()
			.FillWidth(0.4f)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([WeakComp]()
				{
					return WeakComp.IsValid() && WeakComp->bLoop ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([WeakComp](ECheckBoxState NewState)
				{
					if (WeakComp.IsValid())
					{
						WeakComp->bLoop = (NewState == ECheckBoxState::Checked);
						WeakComp->Modify();
					}
				})
				.ToolTipText(LOCTEXT("LoopTip", "Loop playback"))
			]

			// Play on Begin Play checkbox
			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([WeakComp]()
				{
					return WeakComp.IsValid() && WeakComp->bPlayOnBeginPlay ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([WeakComp](ECheckBoxState NewState)
				{
					if (WeakComp.IsValid())
					{
						WeakComp->bPlayOnBeginPlay = (NewState == ECheckBoxState::Checked);
						WeakComp->Modify();
					}
				})
				.ToolTipText(LOCTEXT("BeginPlayTip", "Auto-play when game / PIE starts"))
			]

			// Status
			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakComp]()
				{
					if (!WeakComp.IsValid()) return LOCTEXT("StatusUnknown", "—");
					return WeakComp->IsPlaying() ? LOCTEXT("StatusPlaying", "Playing") : LOCTEXT("StatusStopped", "Stopped");
				})
				.ColorAndOpacity_Lambda([WeakComp]()
				{
					return WeakComp.IsValid() && WeakComp->IsPlaying()
						? FSlateColor(FLinearColor(0.2f, 1.f, 0.3f))
						: FSlateColor::UseSubduedForeground();
				})
				.Font(FAppStyle::GetFontStyle("SmallFont"))
			]

			// Play / Stop
			+ SHorizontalBox::Slot()
			.FillWidth(0.8f)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Play", "Play"))
					.OnClicked_Lambda([WeakComp]() -> FReply
					{
						if (WeakComp.IsValid()) WeakComp->Play();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Stop", "Stop"))
					.OnClicked_Lambda([WeakComp]() -> FReply
					{
						if (WeakComp.IsValid()) WeakComp->Stop();
						return FReply::Handled();
					})
				]
			]
		];
}

FReply SAudioLoomPanel::OnSelectInViewport(TWeakObjectPtr<UAudioLoomWasapiComponent> Component)
{
	if (!GEditor || !Component.IsValid()) return FReply::Handled();
	AActor* Owner = Component->GetOwner();
	if (Owner)
	{
		GEditor->SelectActor(Owner, true, true);
		GEditor->MoveViewportCamerasToActor(*Owner, false);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
