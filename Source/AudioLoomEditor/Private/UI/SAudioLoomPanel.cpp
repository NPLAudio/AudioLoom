// Copyright (c) 2026 AudioLoom Contributors.

#include "UI/SAudioLoomPanel.h"
#include "UI/SAudioLoomExpandableRow.h"
#include "AudioLoomComponent.h"
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
#include "Widgets/Views/STableRow.h"

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
				.Orientation(Orient_Vertical)
				+ SScrollBox::Slot()
				[
					SAssignNew(ListView, SListView<TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>>>)
					.ListItemsSource(&ListViewItems)
					.OnGenerateRow(this, &SAudioLoomPanel::GenerateRow)
					.SelectionMode(ESelectionMode::None)
					.ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
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
		TArray<UAudioLoomComponent*> Comps;
		It->GetComponents(Comps);
		for (UAudioLoomComponent* Comp : Comps)
		{
			if (IsValid(Comp))
			{
				ComponentList.Add(Comp);
				ListViewItems.Add(MakeShared<TWeakObjectPtr<UAudioLoomComponent>>(Comp));
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
		TArray<UAudioLoomComponent*> Comps;
		It->GetComponents(Comps);
		for (UAudioLoomComponent* Comp : Comps)
		{
			if (IsValid(Comp)) ++Count;
		}
	}
	if (Count != ComponentList.Num()) return true;

	int32 ValidCount = 0;
	for (const TWeakObjectPtr<UAudioLoomComponent>& W : ComponentList)
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

TSharedRef<ITableRow> SAudioLoomPanel::GenerateRow(
	TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>> InItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const int32 Index = ListViewItems.Find(InItem);
	const bool bInitiallyExpanded = (Index == 0);

	return SNew(STableRow<TSharedPtr<TWeakObjectPtr<UAudioLoomComponent>>>, OwnerTable)
		[
			SNew(SAudioLoomExpandableRow)
			.Item(InItem)
			.Devices(&CachedDevices)
			.Panel(SharedThis(this))
			.InitiallyExpanded(bInitiallyExpanded)
		];
}

FReply SAudioLoomPanel::OnSelectInViewport(TWeakObjectPtr<UAudioLoomComponent> Component)
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
