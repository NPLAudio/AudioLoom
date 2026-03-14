// Copyright (c) 2026 AudioLoom Contributors.

#include "UI/SAudioLoomPanel.h"
#include "AudioLoomWasapiComponent.h"
#include "AudioLoomBlueprintLibrary.h"
#include "AudioLoomOscSubsystem.h"
#include "AudioLoomOscSettings.h"
#include "WasapiDeviceEnumerator.h"
#include "WasapiDeviceInfo.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "PropertyCustomizationHelpers.h"
#include "Sound/SoundWave.h"

#define LOCTEXT_NAMESPACE "SAudioLoomPanel"

// FillWidth coefficients for proportional column sizing (header and rows must match)
namespace AudioLoomColumnFill
{
	static constexpr float Actor = 1.4f;
	static constexpr float Sound = 1.1f;
	static constexpr float Device = 1.6f;
	static constexpr float Channel = 0.55f;
	static constexpr float LL = 0.4f;
	static constexpr float Buf = 0.5f;
	static constexpr float Loop = 0.45f;
	static constexpr float Begin = 0.5f;
	static constexpr float OscAddr = 1.4f;
	static constexpr float Status = 0.65f;
	static constexpr float Controls = 0.9f;
}

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

			// OSC section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(6.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OscSection", "OSC — Triggers & Monitoring"))
						.Font(FAppStyle::GetFontStyle("SmallFontBold"))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 8.f, 0.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(LOCTEXT("ListenPort", "Listen Port")).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(0.5f)
						.MaxWidth(80.f)
						[
							SNew(SSpinBox<int32>)
							.MinValue(1024).MaxValue(65535)
							.Value_Lambda([]() { return GetDefault<UAudioLoomOscSettings>()->ListenPort; })
							.OnValueCommitted_Lambda([](int32 Val, ETextCommit::Type)
							{
								if (UAudioLoomOscSettings* S = GetMutableDefault<UAudioLoomOscSettings>())
								{
									S->ListenPort = Val;
									S->SaveConfig();
								}
							})
							.ToolTipText(LOCTEXT("ListenPortTip", "UDP port for receiving OSC triggers"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(12.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton)
							.Text(LOCTEXT("CheckPort", "Check Port"))
							.OnClicked(this, &SAudioLoomPanel::OnCheckPortClicked)
							.ToolTipText(LOCTEXT("CheckPortTip", "Check if listen port is available"))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.Padding(4.f, 0.f)
						.VAlign(VAlign_Center)
						[
							SAssignNew(PortStatusText, STextBlock)
							.Text(LOCTEXT("PortUnknown", "—"))
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 8.f, 0.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(LOCTEXT("SendTo", "Send to")).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.MaxWidth(100.f)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([]() { return FText::FromString(GetDefault<UAudioLoomOscSettings>()->SendIP); })
							.OnTextCommitted_Lambda([](const FText& T, ETextCommit::Type)
							{
								if (UAudioLoomOscSettings* S = GetMutableDefault<UAudioLoomOscSettings>())
								{
									S->SendIP = T.ToString();
									S->SaveConfig();
								}
							})
							.ToolTipText(LOCTEXT("SendIPTip", "Target IP for monitoring messages"))
							.Font(FAppStyle::GetFontStyle("SmallFont"))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(0.5f)
						.MaxWidth(60.f)
						[
							SNew(SSpinBox<int32>)
							.MinValue(1).MaxValue(65535)
							.Value_Lambda([]() { return GetDefault<UAudioLoomOscSettings>()->SendPort; })
							.OnValueCommitted_Lambda([](int32 Val, ETextCommit::Type)
							{
								if (UAudioLoomOscSettings* S = GetMutableDefault<UAudioLoomOscSettings>())
								{
									S->SendPort = Val;
									S->SaveConfig();
								}
							})
							.ToolTipText(LOCTEXT("SendPortTip", "Target port for monitoring"))
							.Font(FAppStyle::GetFontStyle("SmallFont"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(8.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text_Lambda([this]()
							{
								UWorld* W = GetCurrentWorld();
								UAudioLoomOscSubsystem* Sub = W ? W->GetSubsystem<UAudioLoomOscSubsystem>() : nullptr;
								return Sub && Sub->IsListening() ? LOCTEXT("StopOsc", "Stop OSC") : LOCTEXT("StartOsc", "Start OSC");
							})
							.OnClicked(this, &SAudioLoomPanel::OnStartStopOscClicked)
							.ToolTipText(LOCTEXT("StartStopOscTip", "Start or stop OSC server for triggers"))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 6.f)
					[
						SNew(SExpandableArea)
						.InitiallyCollapsed(true)
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("OscCommandsTitle", "OSC Commands Reference"))
							.Font(FAppStyle::GetFontStyle("SmallFontBold"))
						]
						.BodyContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("OscCommandsBody",
								"Each component has a base OSC address (e.g. /audioloom/1). Send messages to these addresses:\n\n"
								"• /base/play  — Start playback. Args: float ≥0.5, int ≥1, or none.\n"
								"• /base/stop  — Stop playback. Any message triggers stop.\n"
								"• /base/loop  — Set loop on/off. 1 = loop, 0 = no loop.\n\n"
								"Monitoring (outgoing): When play or stop occurs, AudioLoom sends to Send IP:Port:\n"
								"• /base/state — Float 1 (playing) or 0 (stopped)."))
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.AutoWrapText(true)
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				+ SScrollBox::Slot()
				[
					SNew(SBox)
					.MinDesiredWidth(900.f)
					[
						SNew(SVerticalBox)
						// Header row — same width as list rows, scrolls horizontally with list
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 0.f, 0.f, 4.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::Actor).Padding(4.f, 2.f)
								[SNew(STextBlock).Text(LOCTEXT("ColActor", "Actor")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::Sound).Padding(4.f, 2.f)
								[SNew(STextBlock).Text(LOCTEXT("ColSound", "Sound")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::Device).Padding(4.f, 2.f)
								[SNew(STextBlock).Text(LOCTEXT("ColDevice", "Device")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::Channel).Padding(4.f, 2.f)
								[SNew(STextBlock).Text(LOCTEXT("ColChannel", "Ch")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::LL).Padding(4.f, 2.f)
								[SNew(SBox).Visibility(PLATFORM_WINDOWS ? EVisibility::Visible : EVisibility::Collapsed)[SNew(STextBlock).Text(LOCTEXT("ColLL", "LL")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::Buf).Padding(4.f, 2.f)
								[SNew(SBox).Visibility(PLATFORM_WINDOWS ? EVisibility::Visible : EVisibility::Collapsed)[SNew(STextBlock).Text(LOCTEXT("ColBuf", "Buf")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::Loop).Padding(4.f, 2.f)
								[SNew(STextBlock).Text(LOCTEXT("ColLoop", "Loop")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::Begin).Padding(4.f, 2.f)
								[SNew(STextBlock).Text(LOCTEXT("ColBegin", "Begin")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::OscAddr).Padding(4.f, 2.f)
								[SNew(STextBlock).Text(LOCTEXT("ColOscAddr", "OSC Address")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::Status).Padding(4.f, 2.f)
								[SNew(STextBlock).Text(LOCTEXT("ColStatus", "Status")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
							+ SHorizontalBox::Slot().FillWidth(AudioLoomColumnFill::Controls).Padding(4.f, 2.f)
								[SNew(STextBlock).Text(LOCTEXT("ColControls", "Controls")).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SAssignNew(ListView, SListView<TSharedPtr<TWeakObjectPtr<UAudioLoomWasapiComponent>>>)
							.ListItemsSource(&ListViewItems)
							.OnGenerateRow(this, &SAudioLoomPanel::GenerateComponentRow)
							.SelectionMode(ESelectionMode::None)
						]
					]
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
	CachedDevices = FWasapiDeviceEnumerator::GetOutputDevices();

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
	UWorld* World = GetCurrentWorld();
	if (World)
	{
		if (UAudioLoomOscSubsystem* Sub = World->GetSubsystem<UAudioLoomOscSubsystem>())
		{
			if (Sub->IsListening())
			{
				Sub->RebuildComponentRegistry();
			}
		}
	}
	return EActiveTimerReturnType::Continue;
}

FReply SAudioLoomPanel::OnRefreshClicked()
{
	RebuildComponentList();
	return FReply::Handled();
}

FReply SAudioLoomPanel::OnCheckPortClicked()
{
	const int32 Port = GetDefault<UAudioLoomOscSettings>()->ListenPort;
	const bool bAvailable = UAudioLoomOscSubsystem::IsPortAvailable(Port);
	if (PortStatusText.IsValid())
	{
		PortStatusText->SetText(bAvailable
			? FText::Format(LOCTEXT("PortAvailable", "Port {0} available"), FText::AsNumber(Port))
			: FText::Format(LOCTEXT("PortInUse", "Port {0} in use"), FText::AsNumber(Port)));
		PortStatusText->SetColorAndOpacity(bAvailable
			? FSlateColor(FLinearColor(0.2f, 1.f, 0.3f))
			: FSlateColor(FLinearColor(1.f, 0.3f, 0.2f)));
	}
	return FReply::Handled();
}

FReply SAudioLoomPanel::OnStartStopOscClicked()
{
	UWorld* World = GetCurrentWorld();
	if (!World) return FReply::Handled();

	UAudioLoomOscSubsystem* Sub = World->GetSubsystem<UAudioLoomOscSubsystem>();
	if (!Sub) return FReply::Handled();

	if (Sub->IsListening())
	{
		Sub->StopListening();
		if (PortStatusText.IsValid())
		{
			PortStatusText->SetText(LOCTEXT("OscStopped", "OSC stopped"));
			PortStatusText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		}
	}
	else
	{
		if (Sub->StartListening())
		{
			if (PortStatusText.IsValid())
			{
				PortStatusText->SetText(LOCTEXT("OscListening", "OSC listening"));
				PortStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 1.f, 0.3f)));
			}
		}
		else
		{
			if (PortStatusText.IsValid())
			{
				PortStatusText->SetText(LOCTEXT("OscStartFailed", "Failed to start — check port"));
				PortStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.3f, 0.2f)));
			}
		}
	}
	if (ListView.IsValid()) ListView->Invalidate(EInvalidateWidgetReason::Layout);
	return FReply::Handled();
}

TSharedRef<ITableRow> SAudioLoomPanel::GenerateComponentRow(
	TSharedPtr<TWeakObjectPtr<UAudioLoomWasapiComponent>> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	UAudioLoomWasapiComponent* Comp = Item.IsValid() ? Item->Get() : nullptr;
	TWeakObjectPtr<UAudioLoomWasapiComponent> WeakComp = Comp;
	const TArray<FWasapiDeviceInfo>& Devices = CachedDevices;

	return SNew(STableRow<TSharedPtr<TWeakObjectPtr<UAudioLoomWasapiComponent>>>, OwnerTable)
		.Padding(FMargin(2.f, 2.f))
		[
			SNew(SHorizontalBox)

			// Actor name
			+ SHorizontalBox::Slot()
			.FillWidth(AudioLoomColumnFill::Actor)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
					SNew(SButton)
					.Text_Lambda([WeakComp]()
					{
						if (AActor* Owner = WeakComp.IsValid() ? WeakComp->GetOwner() : nullptr)
						{
							if (!Owner) return FText::FromString(TEXT("—"));
							FString Label = Owner->GetActorLabel();
							// Display "Audio Loom" instead of "AudioLoomWasapi" (class-derived default)
							Label.ReplaceInline(TEXT("AudioLoomWasapi"), TEXT("Audio Loom"), ESearchCase::IgnoreCase);
							return FText::FromString(Label.IsEmpty() ? TEXT("—") : Label);
						}
						return LOCTEXT("Invalid", "—");
					})
					.OnClicked_Lambda([this, WeakComp]() { return OnSelectInViewport(WeakComp); })
					.ToolTipText(LOCTEXT("SelectInViewport", "Select actor in viewport"))
					.HAlign(HAlign_Left)
			]

			// Sound picker
			+ SHorizontalBox::Slot()
			.FillWidth(AudioLoomColumnFill::Sound)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
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

			// Device dropdown
			+ SHorizontalBox::Slot()
			.FillWidth(AudioLoomColumnFill::Device)
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
					.Text_Lambda([WeakComp, Devices]()
					{
						if (!WeakComp.IsValid()) return LOCTEXT("DefaultDevice", "Default Output");
						FString Id = WeakComp->DeviceId;
						if (Id.IsEmpty()) return LOCTEXT("DefaultDevice", "Default Output");
						for (const FWasapiDeviceInfo& D : Devices)
						{
							if (D.DeviceId == Id) return FText::FromString(D.FriendlyName);
						}
						return FText::FromString(Id);
					})
					.Font(FAppStyle::GetFontStyle("SmallFont"))
				]
			]

			// Channel dropdown
			+ SHorizontalBox::Slot()
			.FillWidth(AudioLoomColumnFill::Channel)
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

			// Low latency mode checkbox (Windows only)
			+ SHorizontalBox::Slot()
			.FillWidth(AudioLoomColumnFill::LL)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(SBox)
				.Visibility_Lambda([WeakComp]()
				{
					return (WeakComp.IsValid() && PLATFORM_WINDOWS) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SCheckBox)
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
				]
			]

			// Buffer size (ms) for low latency mode (Windows only)
			+ SHorizontalBox::Slot()
			.FillWidth(AudioLoomColumnFill::Buf)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
			[
				SNew(SBox)
				.Visibility_Lambda([WeakComp]()
				{
					return (WeakComp.IsValid() && PLATFORM_WINDOWS) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SSpinBox<int32>)
					.MinValue(0)
					.MaxValue(100)
					.Value_Lambda([WeakComp]() { return WeakComp.IsValid() ? WeakComp->BufferSizeMs : 0; })
					.OnValueChanged_Lambda([WeakComp](int32 Val)
					{
						if (WeakComp.IsValid()) { WeakComp->BufferSizeMs = FMath::Clamp(Val, 0, 100); WeakComp->Modify(); }
					})
					.MinDesiredWidth(50.f)
					.ToolTipText(LOCTEXT("BufferSizeTip", "Buffer ms (0=default), for low latency mode"))
				]
			]

			// Loop checkbox
			+ SHorizontalBox::Slot()
			.FillWidth(AudioLoomColumnFill::Loop)
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
			.FillWidth(AudioLoomColumnFill::Begin)
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

			// OSC Address (base for /play, /stop, /loop)
			+ SHorizontalBox::Slot()
			.FillWidth(AudioLoomColumnFill::OscAddr)
			.VAlign(VAlign_Center)
			.Padding(4.f, 2.f)
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
						if (S.IsEmpty())
						{
							WeakComp->OscAddress = S;
							WeakComp->Modify();
							return;
						}
						if (WeakComp->SetOscAddress(S))
						{
							WeakComp->Modify();
						}
					})
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ToolTipText(LOCTEXT("OscAddrTip", "Base OSC address. Empty = default. Play=/base/play, Stop=/base/stop, Loop=/base/loop"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f)
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
					.ToolTipText_Lambda([WeakComp]()
					{
						if (!WeakComp.IsValid()) return FText();
						FString Addr = WeakComp->OscAddress;
						if (Addr.IsEmpty()) return LOCTEXT("OscUsingDefault", "Using default address");
						return UAudioLoomBlueprintLibrary::IsOscAddressValid(Addr)
							? LOCTEXT("OscValid", "Valid OSC address")
							: LOCTEXT("OscInvalid", "Invalid — must start with /, use valid path chars");
					})
				]
			]

			// Status
			+ SHorizontalBox::Slot()
			.FillWidth(AudioLoomColumnFill::Status)
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
			.FillWidth(AudioLoomColumnFill::Controls)
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
