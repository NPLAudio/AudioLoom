// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file SAudioLoomExpandableRow.cpp
 * @brief Slate lambdas bind to `WeakComp`; `UpdateTickEnabled` after loop/auto-replay toggles.
 */

#include "UI/SAudioLoomExpandableRow.h"
#include "UI/SAudioLoomPanel.h"
#include "AudioLoomComponent.h"
#include "AudioLoomBlueprintLibrary.h"
#include "AudioOutputDeviceEnumerator.h"
#include "AudioOutputDeviceInfo.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/App.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
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

	// Weak pointers: row may outlive quick edits; avoid capturing raw Comp in lambdas
	UAudioLoomComponent* Comp = Item.IsValid() && Item->IsValid() ? Item->Get() : nullptr;
	TWeakObjectPtr<UAudioLoomComponent> WeakComp = Comp;
	const TArray<FAudioOutputDeviceInfo>& Devs = Devices ? *Devices : static_cast<const TArray<FAudioOutputDeviceInfo>&>(TArray<FAudioOutputDeviceInfo>());
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
				.Padding(0.f, 0.f, 6.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(10.f)
					.HeightOverride(10.f)
					.VAlign(VAlign_Center)
					[
						SAssignNew(HeaderActivityLight, SBorder)
						.Padding(0.f)
						.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FSlateColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.f)))
						.ToolTipText(LOCTEXT("ActivityLightTip", "Output activity: lit when this component is playing audio. In PIE, the list must track the play world (see panel refresh)."))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				.VAlign(VAlign_Center)
				[
					// Driven by RegisterActiveTimer (UpdateLiveReadouts): lambdas alone do not repaint when
					// playback starts from Begin Play / PIE — IsPlaying() would stay stale until user input.
					SAssignNew(HeaderStatusText, STextBlock)
					.Text(LOCTEXT("StatusStopped", "Stopped"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
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
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(HeaderLatencyText, STextBlock)
					.Text(FText::FromString(TEXT("—")))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.ToolTipText(LOCTEXT("OutputLatencyTip", "Estimated OS output buffer / stream latency (ms). Not full round-trip input→output delay; see README."))
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

					// Editor FPS / frame time (expanded body only; not DAC buffer latency)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 6.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 6.f, 0.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("EditorPerfLabel", "Editor:"))
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SAssignNew(EditorPerfText, STextBlock)
							.Text(FText::FromString(TEXT("—")))
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.ToolTipText(LOCTEXT("EditorPerfTip", "Slate/editor frame budget (FPS and average frame time). This is not the same as output buffer latency in the row header; low FPS can make these labels update less smoothly."))
						]
					]

					// Sound picker — same asset path string as Details panel object reference
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
							WeakComp->Modify(); // undo + dirty level package
						})
						.AllowedClass(USoundWave::StaticClass())
						.DisplayThumbnail(false)
						.DisplayCompactSize(true)
						.EnableContentPicker(true)
						.AllowClear(true)
						.ToolTipText(LOCTEXT("SoundPickerTip", "Select sound from Content Browser or drag-drop"))
					]

					// Device + Channel row — mirrors Details: DeviceId string + OutputChannel index
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
								for (const FAudioOutputDeviceInfo& D : Devs)
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
									for (const FAudioOutputDeviceInfo& D : Devs)
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
								FAudioOutputDeviceInfo Info = FAudioOutputDeviceEnumerator::GetDeviceById(DevId);
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
									WeakComp->UpdateTickEnabled();
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

					// Auto Replay options
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
								return WeakComp.IsValid() && WeakComp->bAutoReplay ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							})
							.OnCheckStateChanged_Lambda([WeakComp](ECheckBoxState NewState)
							{
								if (WeakComp.IsValid())
								{
									WeakComp->bAutoReplay = (NewState == ECheckBoxState::Checked);
									WeakComp->Modify();
									WeakComp->UpdateTickEnabled();
								}
							})
							.ToolTipText(LOCTEXT("AutoReplayTip", "Automatically replay after sound ends. Delay is from end to next play."))
							[
								SNew(STextBlock).Text(LOCTEXT("AutoReplay", "Auto Replay")).Font(FAppStyle::GetFontStyle("SmallFont"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 8.f, 0.f)
						[
							SNew(SCheckBox)
							.IsChecked_Lambda([WeakComp]()
							{
								return WeakComp.IsValid() && WeakComp->bRandomReplayDelay ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							})
							.OnCheckStateChanged_Lambda([WeakComp](ECheckBoxState NewState)
							{
								if (WeakComp.IsValid())
								{
									WeakComp->bRandomReplayDelay = (NewState == ECheckBoxState::Checked);
									WeakComp->Modify();
								}
							})
							.ToolTipText(LOCTEXT("RandomDelayTip", "Random delay between Min and Max seconds. When off, uses fixed delay."))
							[
								SNew(STextBlock).Text(LOCTEXT("RandomDelay", "Random")).Font(FAppStyle::GetFontStyle("SmallFont"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(STextBlock).Text(LOCTEXT("DelaySec", "Delay (s)")).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 8.f, 0.f)
						[
							SNew(SSpinBox<float>)
							.MinValue(0.0f).MaxValue(3600.0f)
							.MinDesiredWidth(55.f)
							.Value_Lambda([WeakComp]() { return WeakComp.IsValid() ? WeakComp->ReplayDelaySeconds : 1.0f; })
							.OnValueChanged_Lambda([WeakComp](float Val)
							{
								if (WeakComp.IsValid()) { WeakComp->ReplayDelaySeconds = FMath::Max(0.0f, Val); WeakComp->Modify(); }
							})
							.Visibility_Lambda([WeakComp]() { return (WeakComp.IsValid() && WeakComp->bAutoReplay && !WeakComp->bRandomReplayDelay) ? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(LOCTEXT("FixedDelayTip", "Fixed delay in seconds from sound end to next play"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(STextBlock).Text(LOCTEXT("MinSec", "Min")).Font(FAppStyle::GetFontStyle("SmallFont"))
								.Visibility_Lambda([WeakComp]() { return (WeakComp.IsValid() && WeakComp->bAutoReplay && WeakComp->bRandomReplayDelay) ? EVisibility::Visible : EVisibility::Collapsed; })
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 8.f, 0.f)
						[
							SNew(SSpinBox<float>)
							.MinValue(0.0f).MaxValue(3600.0f)
							.MinDesiredWidth(50.f)
							.Value_Lambda([WeakComp]() { return WeakComp.IsValid() ? WeakComp->ReplayDelayMin : 0.5f; })
							.OnValueChanged_Lambda([WeakComp](float Val)
							{
								if (WeakComp.IsValid()) { WeakComp->ReplayDelayMin = FMath::Max(0.0f, Val); WeakComp->Modify(); }
							})
							.Visibility_Lambda([WeakComp]() { return (WeakComp.IsValid() && WeakComp->bAutoReplay && WeakComp->bRandomReplayDelay) ? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(LOCTEXT("MinDelayTip", "Minimum random delay in seconds"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(STextBlock).Text(LOCTEXT("MaxSec", "Max")).Font(FAppStyle::GetFontStyle("SmallFont"))
								.Visibility_Lambda([WeakComp]() { return (WeakComp.IsValid() && WeakComp->bAutoReplay && WeakComp->bRandomReplayDelay) ? EVisibility::Visible : EVisibility::Collapsed; })
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SSpinBox<float>)
							.MinValue(0.0f).MaxValue(3600.0f)
							.MinDesiredWidth(50.f)
							.Value_Lambda([WeakComp]() { return WeakComp.IsValid() ? WeakComp->ReplayDelayMax : 3.0f; })
							.OnValueChanged_Lambda([WeakComp](float Val)
							{
								if (WeakComp.IsValid()) { WeakComp->ReplayDelayMax = FMath::Max(0.0f, Val); WeakComp->Modify(); }
							})
							.Visibility_Lambda([WeakComp]() { return (WeakComp.IsValid() && WeakComp->bAutoReplay && WeakComp->bRandomReplayDelay) ? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(LOCTEXT("MaxDelayTip", "Maximum random delay in seconds"))
						]
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

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Text_Lambda([WeakComp]()
						{
							if (!WeakComp.IsValid()) return FText::GetEmpty();
							const FString Base = WeakComp->GetOscAddress();
							return FText::FromString(FString::Printf(
								TEXT("OSC: %s/play  %s/stop  %s/loop  %s/state"),
								*Base, *Base, *Base, *Base));
						})
						.ToolTipText(LOCTEXT("OscTriggersTip", "Resolved OSC paths for this component (for planning / external tools)."))
					]

					// Transport — same as Details “Preview” row (editor-time hardware playback)
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
								if (WeakComp.IsValid()) WeakComp->Play(); // decodes SoundWave again each click
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

	// UE 5.7: two-arg RegisterActiveTimer (tick period + delegate); updates header status/latency + body editor FPS.
	RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SAudioLoomExpandableRow::UpdateLiveReadouts));
	// Sync immediately so status/latency match Begin Play / PIE without waiting for first tick.
	UpdateLiveReadouts(0.0, 0.0f);
}

EActiveTimerReturnType SAudioLoomExpandableRow::UpdateLiveReadouts(double InCurrentTime, float InDeltaTime)
{
	UAudioLoomComponent* const Comp = Item.IsValid() && Item->IsValid() ? Item->Get() : nullptr;

	if (HeaderActivityLight.IsValid())
	{
		const bool bActive = Comp && Comp->IsPlaying();
		HeaderActivityLight->SetBorderBackgroundColor(FSlateColor(
			bActive ? FLinearColor(0.1f, 0.92f, 0.3f, 1.f) : FLinearColor(0.11f, 0.11f, 0.11f, 1.f)));
	}

	if (HeaderStatusText.IsValid())
	{
		if (!Comp)
		{
			HeaderStatusText->SetText(LOCTEXT("StatusUnknown", "—"));
			HeaderStatusText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		}
		else if (Comp->IsPlaying())
		{
			HeaderStatusText->SetText(LOCTEXT("StatusPlaying", "Playing"));
			HeaderStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 1.f, 0.3f)));
		}
		else
		{
			HeaderStatusText->SetText(LOCTEXT("StatusStopped", "Stopped"));
			HeaderStatusText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		}
	}

	if (HeaderLatencyText.IsValid())
	{
		if (Comp && Comp->IsPlaying())
		{
			const float Ms = Comp->GetOutputLatencyMs();
			HeaderLatencyText->SetText(FText::FromString(FString::Printf(TEXT("Out: %.1f ms"), Ms)));
		}
		else
		{
			HeaderLatencyText->SetText(FText::FromString(TEXT("—")));
		}
	}
	if (EditorPerfText.IsValid())
	{
		float AvgDelta = 0.f;
		if (FSlateApplication::IsInitialized())
		{
			AvgDelta = FSlateApplication::Get().GetAverageDeltaTime();
		}
		if (AvgDelta <= KINDA_SMALL_NUMBER)
		{
			AvgDelta = FApp::GetDeltaTime();
		}
		if (AvgDelta > KINDA_SMALL_NUMBER)
		{
			const float FPS = 1.f / AvgDelta;
			const float FrameMs = AvgDelta * 1000.f;
			EditorPerfText->SetText(FText::FromString(FString::Printf(TEXT("~%.0f FPS (%.1f ms)"), FPS, FrameMs)));
		}
		else
		{
			EditorPerfText->SetText(FText::FromString(TEXT("—")));
		}
	}
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
