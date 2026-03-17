// Copyright (c) 2026 AudioLoom Contributors.

#include "UI/SAudioLoomExpandableRow.h"
#include "UI/SAudioLoomPanel.h"
#include "AudioLoomComponent.h"
#include "AudioLoomBlueprintLibrary.h"
#include "WasapiDeviceEnumerator.h"
#include "WasapiDeviceInfo.h"

#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "Sound/SoundWave.h"

#define LOCTEXT_NAMESPACE "SAudioLoomExpandableRow"

void SAudioLoomExpandableRow::Construct(const FArguments& InArgs)
{
	Item = InArgs._Item;
	Devices = InArgs._Devices;
	Panel = InArgs._Panel;

	UAudioLoomComponent* Comp = Item.IsValid() && Item->IsValid() ? Item->Get() : nullptr;
	TWeakObjectPtr<UAudioLoomComponent> WeakComp = Comp;
	const TArray<FWasapiDeviceInfo>& Devs = Devices ? *Devices : static_cast<const TArray<FWasapiDeviceInfo>&>(TArray<FWasapiDeviceInfo>());
	SAudioLoomPanel* PanelPtr = Panel.Pin().Get();

	auto SelectInViewport = [PanelPtr, WeakComp]()
	{
		if (PanelPtr && WeakComp.IsValid())
		{
			PanelPtr->OnSelectInViewport(WeakComp);
		}
		return FReply::Handled();
	};

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(4.f, 2.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(!InArgs._InitiallyExpanded)
			.HeaderContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text_Lambda([WeakComp]()
					{
						if (AActor* Owner = WeakComp.IsValid() ? WeakComp->GetOwner() : nullptr)
						{
							if (!Owner) return FText::FromString(TEXT("—"));
							FString Label = Owner->GetActorLabel();
							Label.ReplaceInline(TEXT("AudioLoomComponent"), TEXT("Audio Loom"), ESearchCase::IgnoreCase);
							return FText::FromString(Label.IsEmpty() ? TEXT("—") : Label);
						}
						return LOCTEXT("Invalid", "—");
					})
					.OnClicked_Lambda(SelectInViewport)
					.ToolTipText(LOCTEXT("SelectInViewport", "Select actor in viewport"))
					.HAlign(HAlign_Left)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT(" — ")))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				.VAlign(VAlign_Center)
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
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT(" | ")))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([WeakComp]()
					{
						if (!WeakComp.IsValid() || !WeakComp->SoundWave) return LOCTEXT("NoSound", "—");
						return FText::FromString(WeakComp->SoundWave->GetName());
					})
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ToolTipText_Lambda([WeakComp]()
					{
						if (!WeakComp.IsValid() || !WeakComp->SoundWave) return FText();
						return FText::FromString(WeakComp->SoundWave->GetPathName());
					})
				]
			]
			.BodyContent()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8.f)
				[
					SNew(SVerticalBox)

					// Sound picker
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SObjectPropertyEntryBox)
						.ObjectPath(TAttribute<FString>::CreateLambda([WeakComp]()
						{
							if (!WeakComp.IsValid() || !WeakComp->SoundWave) return FString();
							return WeakComp->SoundWave->GetPathName();
						}))
						.OnObjectChanged_Lambda([WeakComp](const FAssetData& AssetData)
						{
							if (!WeakComp.IsValid()) return;
							UObject* Obj = AssetData.GetAsset();
							WeakComp->SoundWave = Cast<USoundWave>(Obj);
							WeakComp->Modify();
						})
						.AllowedClass(USoundWave::StaticClass())
						.DisplayThumbnail(false)
						.DisplayCompactSize(true)
						.EnableContentPicker(true)
						.AllowClear(true)
						.ToolTipText(LOCTEXT("SoundPickerTip", "Select sound from Content Browser or drag-drop"))
					]

					// Device + Channel row
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNew(SComboButton)
							.OnGetMenuContent_Lambda([WeakComp, Devs]()
							{
								FMenuBuilder MenuBuilder(true, nullptr);
								MenuBuilder.AddMenuEntry(
									LOCTEXT("DefaultDevice", "Default Output"),
									LOCTEXT("DefaultDeviceTip", "Use system default"),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([WeakComp]()
									{
										if (WeakComp.IsValid()) { WeakComp->DeviceId = FString(); WeakComp->Modify(); }
									}))
								);
								for (const FWasapiDeviceInfo& D : Devs)
								{
									if (!D.bIsValid) continue;
									FString Id = D.DeviceId;
									MenuBuilder.AddMenuEntry(
										FText::FromString(D.FriendlyName),
										FText::FromString(Id),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateLambda([WeakComp, Id]()
										{
											if (WeakComp.IsValid()) { WeakComp->DeviceId = Id; WeakComp->Modify(); }
										}))
									);
								}
								return MenuBuilder.MakeWidget();
							})
							.ButtonContent()
							[
								SNew(STextBlock)
								.Text_Lambda([WeakComp, Devs]()
								{
									if (!WeakComp.IsValid()) return LOCTEXT("DefaultDevice", "Default Output");
									FString Id = WeakComp->DeviceId;
									if (Id.IsEmpty()) return LOCTEXT("DefaultDevice", "Default Output");
									for (const FWasapiDeviceInfo& D : Devs)
									{
										if (D.DeviceId == Id) return FText::FromString(D.FriendlyName);
									}
									return FText::FromString(Id);
								})
								.Font(FAppStyle::GetFontStyle("SmallFont"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SComboButton)
							.OnGetMenuContent_Lambda([WeakComp]()
							{
								FString DevId = WeakComp.IsValid() ? WeakComp->DeviceId : FString();
								FWasapiDeviceInfo Info = FWasapiDeviceEnumerator::GetDeviceById(DevId);
								int32 NumCh = Info.bIsValid ? Info.NumChannels : 8;
								FMenuBuilder MenuBuilder(true, nullptr);
								MenuBuilder.AddMenuEntry(
									LOCTEXT("ChannelAll", "All (0)"), FText(), FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([WeakComp]()
									{
										if (WeakComp.IsValid()) { WeakComp->OutputChannel = 0; WeakComp->Modify(); }
									}))
								);
								for (int32 i = 1; i <= NumCh; ++i)
								{
									int32 Ch = i;
									MenuBuilder.AddMenuEntry(
										FText::FromString(FString::Printf(TEXT("Ch %d"), Ch)), FText(), FSlateIcon(),
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
					]

					// Loop, Begin (and LL, Buf on Windows)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 8.f, 0.f)
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
							[
								SNew(STextBlock).Text(LOCTEXT("Loop", "Loop")).Font(FAppStyle::GetFontStyle("SmallFont"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 8.f, 0.f)
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
							[
								SNew(STextBlock).Text(LOCTEXT("Begin", "Begin")).Font(FAppStyle::GetFontStyle("SmallFont"))
							]
						]
#if PLATFORM_WINDOWS
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 8.f, 0.f)
						[
							SNew(SCheckBox)
							.HAlign(HAlign_Center)
							.IsChecked_Lambda([WeakComp]()
							{
								return WeakComp.IsValid() && WeakComp->bUseExclusiveMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							})
							.OnCheckStateChanged_Lambda([WeakComp](ECheckBoxState NewState)
							{
								if (WeakComp.IsValid())
								{
									WeakComp->bUseExclusiveMode = (NewState == ECheckBoxState::Checked);
									WeakComp->Modify();
								}
							})
							.ToolTipText(LOCTEXT("LowLatencyTip", "Low latency mode (Windows)"))
							[
								SNew(STextBlock).Text(LOCTEXT("LL", "LL")).Font(FAppStyle::GetFontStyle("SmallFont"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SSpinBox<int32>)
							.MinValue(0).MaxValue(100)
							.MinDesiredWidth(45.f)
							.Value_Lambda([WeakComp]() { return WeakComp.IsValid() ? WeakComp->BufferSizeMs : 0; })
							.OnValueChanged_Lambda([WeakComp](int32 Val)
							{
								if (WeakComp.IsValid()) { WeakComp->BufferSizeMs = FMath::Clamp(Val, 0, 100); WeakComp->Modify(); }
							})
							.ToolTipText(LOCTEXT("BufferSizeTip", "Buffer ms (0=default), for low latency mode"))
						]
#endif
					]

					// OSC address
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([WeakComp]()
							{
								if (!WeakComp.IsValid()) return FText();
								FString Addr = WeakComp->OscAddress;
								return FText::FromString(Addr.IsEmpty() ? WeakComp->GetOscAddress() : Addr);
							})
							.OnTextCommitted_Lambda([WeakComp](const FText& T, ETextCommit::Type)
							{
								if (!WeakComp.IsValid()) return;
								FString S = T.ToString().TrimStartAndEnd();
								if (S.IsEmpty()) { WeakComp->OscAddress = S; WeakComp->Modify(); return; }
								if (WeakComp->SetOscAddress(S)) { WeakComp->Modify(); }
							})
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.ToolTipText(LOCTEXT("OscAddrTip", "Base OSC address. Empty = default."))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.f, 0.f, 0.f, 0.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([WeakComp]()
							{
								if (!WeakComp.IsValid()) return FText();
								FString Addr = WeakComp->OscAddress;
								if (Addr.IsEmpty()) return FText();
								return UAudioLoomBlueprintLibrary::IsOscAddressValid(Addr)
									? FText::FromString(TEXT("\u2713"))
									: FText::FromString(TEXT("\u2717"));
							})
							.ColorAndOpacity_Lambda([WeakComp]()
							{
								if (!WeakComp.IsValid()) return FSlateColor::UseSubduedForeground();
								FString Addr = WeakComp->OscAddress;
								if (Addr.IsEmpty()) return FSlateColor::UseSubduedForeground();
								return UAudioLoomBlueprintLibrary::IsOscAddressValid(Addr)
									? FSlateColor(FLinearColor(0.2f, 1.f, 0.3f))
									: FSlateColor(FLinearColor(1.f, 0.3f, 0.2f));
							})
							.Font(FAppStyle::GetFontStyle("SmallFontBold"))
						]
					]

					// Play/Stop
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton)
							.Text(LOCTEXT("Play", "Play"))
							.OnClicked_Lambda([WeakComp]()
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
							.OnClicked_Lambda([WeakComp]()
							{
								if (WeakComp.IsValid()) WeakComp->Stop();
								return FReply::Handled();
							})
						]
					]
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
