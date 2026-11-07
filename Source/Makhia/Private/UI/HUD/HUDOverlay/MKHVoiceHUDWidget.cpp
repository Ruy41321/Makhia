// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/MKHVoiceHUDWidget.h"

#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "InputAction.h"
#include "Online/MKHOnlineSettings.h"
#include "UI/HUD/GameMenu/Voice/MKHVoiceTalkerEntryWidget.h"
#include "UI/HUD/HUDOverlay/MKHKeyHintWidget.h"
#include "Voice/MKHVoiceChatSubsystem.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHVoiceHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (IsValid(PushToTalkKeyCap))
	{
		const UMKHOnlineSettings* Settings = UMKHOnlineSettings::Get();
		PushToTalkKeyCap->SetInputAction(IsValid(Settings) ? Settings->PushToTalkAction.LoadSynchronous() : nullptr);
	}
}

void UMKHVoiceHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->OnVoiceStateChanged.AddUniqueDynamic(this, &UMKHVoiceHUDWidget::HandleVoiceChanged);
		VoiceChat->OnTalkersChanged.AddUniqueDynamic(this, &UMKHVoiceHUDWidget::HandleVoiceChanged);
	}
	HandleVoiceChanged();
}

void UMKHVoiceHUDWidget::NativeDestruct()
{
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->OnVoiceStateChanged.RemoveAll(this);
		VoiceChat->OnTalkersChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHVoiceHUDWidget::HandleVoiceChanged()
{
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	const bool bConnected = IsValid(VoiceChat) && VoiceChat->GetChannel() != EMKHVoiceChannel::None;
	SetVisibility(bConnected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	RefreshTransmitIndicator();
	RefreshTalkingList();
}

void UMKHVoiceHUDWidget::RefreshTransmitIndicator() const
{
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	const bool bTransmitting = IsValid(VoiceChat) && VoiceChat->IsTransmitting();
	TransmitIndicator->SetVisibility(bTransmitting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (IsValid(PushToTalkKeyCap))
	{
		const bool bPushToTalk = IsValid(VoiceChat) && VoiceChat->GetMicMode() == EMKHVoiceMicMode::PushToTalk;
		PushToTalkKeyCap->SetVisibility(bPushToTalk ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMKHVoiceHUDWidget::RefreshTalkingList()
{
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	TArray<FMKHVoiceTalkerInfo> Talking = IsValid(VoiceChat) ? VoiceChat->GetTalkers() : TArray<FMKHVoiceTalkerInfo>();
	Talking.RemoveAll([](const FMKHVoiceTalkerInfo& Info)
	{
		return Info.bIsLocalPlayer || !Info.bIsTalking;
	});
	if (!IsValid(TalkerEntryClass))
	{
		return;
	}
	while (TalkingEntries.Num() > Talking.Num())
	{
		TalkingEntries.Pop()->RemoveFromParent();
	}
	while (TalkingEntries.Num() < Talking.Num())
	{
		UMKHVoiceTalkerEntryWidget* Entry = CreateWidget<UMKHVoiceTalkerEntryWidget>(GetOwningPlayer(), TalkerEntryClass);
		if (!IsValid(Entry))
		{
			return;
		}
		if (UVerticalBoxSlot* EntrySlot = Cast<UVerticalBoxSlot>(TalkingList->AddChild(Entry)); EntrySlot != nullptr)
		{
			EntrySlot->SetHorizontalAlignment(HAlign_Left);
		}
		TalkingEntries.Add(Entry);
	}
	for (int32 Index = 0; Index < Talking.Num(); ++Index)
	{
		TalkingEntries[Index]->SetTalker(Talking[Index]);
	}
}
