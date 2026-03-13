// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomWasapiComponentDetails.h"
#include "AudioLoomWasapiComponent.h"
#include "WasapiDeviceEnumerator.h"
#include "WasapiDeviceInfo.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Sound/SoundWave.h"

#define LOCTEXT_NAMESPACE "AudioLoomWasapiComponentDetails"

TSharedRef<IDetailCustomization> FAudioLoomWasapiComponentDetails::MakeInstance()
{
	return MakeShareable(new FAudioLoomWasapiComponentDetails());
}

void FAudioLoomWasapiComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() == 0)
	{
		return;
	}

	UAudioLoomWasapiComponent* Component = nullptr;
	for (TWeakObjectPtr<UObject>& Obj : Objects)
	{
		if (Obj.IsValid())
		{
			Component = Cast<UAudioLoomWasapiComponent>(Obj.Get());
			break;
		}
	}
	if (!Component)
	{
		return;
	}

	TSharedPtr<IPropertyHandle> DeviceIdHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAudioLoomWasapiComponent, DeviceId));
	TSharedPtr<IPropertyHandle> OutputChannelHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAudioLoomWasapiComponent, OutputChannel));

	IDetailCategoryBuilder& RoutingCategory = DetailBuilder.EditCategory("AudioLoom|Routing", LOCTEXT("RoutingCategory", "WASAPI Routing"));
	IDetailCategoryBuilder& PlaybackCategory = DetailBuilder.EditCategory("AudioLoom|Playback", LOCTEXT("PlaybackCategory", "Playback"));

	// Sound Wave - use default property (supports drag-drop from Content Browser)
	TSharedPtr<IPropertyHandle> SoundWaveHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAudioLoomWasapiComponent, SoundWave));
	RoutingCategory.AddProperty(SoundWaveHandle);

	// Device combo - use PropertyHandle so changes propagate and UI refreshes
	TArray<FWasapiDeviceInfo> Devices = FWasapiDeviceEnumerator::GetOutputDevices();

	RoutingCategory.AddProperty(DeviceIdHandle)
		.CustomWidget()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DeviceLabel", "Device"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		[
			SNew(SComboButton)
			.OnGetMenuContent_Lambda([DeviceIdHandle, Devices]()
			{
				FMenuBuilder MenuBuilder(true, nullptr);
				MenuBuilder.AddMenuEntry(
					LOCTEXT("DefaultDevice", "Default Output"),
					LOCTEXT("DefaultDeviceTip", "Use system default audio device"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([DeviceIdHandle]()
					{
						if (DeviceIdHandle.IsValid())
						{
							DeviceIdHandle->SetValue(FString());
							DeviceIdHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
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
						FUIAction(FExecuteAction::CreateLambda([DeviceIdHandle, Id]()
						{
							if (DeviceIdHandle.IsValid())
							{
								DeviceIdHandle->SetValue(Id);
								DeviceIdHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
							}
						}))
					);
				}
				return MenuBuilder.MakeWidget();
			})
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([DeviceIdHandle, Devices]()
				{
					FString CurrentId;
					if (DeviceIdHandle.IsValid() && DeviceIdHandle->GetValue(CurrentId) == FPropertyAccess::Success)
					{
						if (CurrentId.IsEmpty())
						{
							return LOCTEXT("DefaultDevice", "Default Output");
						}
						for (const FWasapiDeviceInfo& D : Devices)
						{
							if (D.DeviceId == CurrentId) return FText::FromString(D.FriendlyName);
						}
						return FText::FromString(CurrentId);
					}
					return LOCTEXT("DefaultDevice", "Default Output");
				})))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];

	// Channel combo - use AddProperty so row is tied to OutputChannel for proper refresh
	RoutingCategory.AddProperty(OutputChannelHandle)
		.CustomWidget()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChannelLabel", "Output Channel"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(150.f)
		[
			SNew(SComboButton)
			.OnGetMenuContent_Lambda([OutputChannelHandle, DeviceIdHandle]()
			{
				int32 CurrentNumCh = 8;
				FString DevId;
				if (DeviceIdHandle.IsValid() && DeviceIdHandle->GetValue(DevId) == FPropertyAccess::Success)
				{
					FWasapiDeviceInfo DevInfo = FWasapiDeviceEnumerator::GetDeviceById(DevId);
					if (DevInfo.bIsValid) CurrentNumCh = DevInfo.NumChannels;
				}
				else
				{
					FWasapiDeviceInfo DefInfo = FWasapiDeviceEnumerator::GetDefaultOutputDevice();
					if (DefInfo.bIsValid) CurrentNumCh = DefInfo.NumChannels;
				}
				FMenuBuilder MenuBuilder(true, nullptr);
				MenuBuilder.AddMenuEntry(
					LOCTEXT("ChannelAll", "All (0)"),
					LOCTEXT("ChannelAllTip", "Play to all device channels"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([OutputChannelHandle]()
					{
						if (OutputChannelHandle.IsValid())
						{
							OutputChannelHandle->SetValue(0);
							OutputChannelHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
						}
					}))
				);
				for (int32 i = 1; i <= CurrentNumCh; ++i)
				{
					int32 Ch = i;
					MenuBuilder.AddMenuEntry(
						FText::FromString(FString::Printf(TEXT("Channel %d"), Ch)),
						FText::FromString(FString::Printf(TEXT("Route to channel %d only"), Ch)),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([OutputChannelHandle, Ch]()
						{
							if (OutputChannelHandle.IsValid())
							{
								OutputChannelHandle->SetValue(Ch);
								OutputChannelHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
							}
						}))
					);
				}
				return MenuBuilder.MakeWidget();
			})
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([OutputChannelHandle]()
				{
					int32 Val = 0;
					if (OutputChannelHandle.IsValid() && OutputChannelHandle->GetValue(Val) == FPropertyAccess::Success)
					{
						return Val == 0 ? LOCTEXT("ChannelAll", "All (0)") : FText::FromString(FString::Printf(TEXT("Channel %d"), Val));
					}
					return LOCTEXT("ChannelAll", "All (0)");
				})))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];

	// Play / Stop buttons (editor preview)
	PlaybackCategory.AddCustomRow(LOCTEXT("PreviewRow", "Preview"))
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 2.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Play", "Play"))
				.OnClicked_Lambda([Component]() -> FReply
				{
					if (Component)
					{
						Component->Play();
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 2.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Stop", "Stop"))
				.OnClicked_Lambda([Component]() -> FReply
				{
					if (Component)
					{
						Component->Stop();
					}
					return FReply::Handled();
				})
			]
		];
}

#undef LOCTEXT_NAMESPACE
