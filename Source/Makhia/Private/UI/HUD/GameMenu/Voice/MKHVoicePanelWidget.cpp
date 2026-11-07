// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/Voice/MKHVoicePanelWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "InputAction.h"
#include "MKHLogChannels.h"
#include "Online/MKHOnlineSettings.h"
#include "UI/Button/MKHButton.h"
#include "UI/Button/MKHButtonStyle.h"
#include "UI/HUD/GameMenu/Voice/MKHVoiceTalkerEntryWidget.h"
#include "UI/HUD/HUDOverlay/MKHKeyHintWidget.h"
#include "Voice/MKHVoiceChatSubsystem.h"

#define LOCTEXT_NAMESPACE "MKHVoicePanel"

namespace MKHVoicePanel
{
	/** Puts a texture on an image when both exist. */
	static void SetIcon(UImage* Image, UTexture2D* Icon)
	{
		if (IsValid(Image) && IsValid(Icon))
		{
			Image->SetBrushFromTexture(Icon, false);
		}
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHVoicePanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (IsValid(VoiceToggleButton))
	{
		VoiceToggleButton->OnClicked.AddUniqueDynamic(this, &UMKHVoicePanelWidget::HandleVoiceToggleClicked);
	}
	if (IsValid(MicToggleButton))
	{
		MicToggleButton->OnClicked.AddUniqueDynamic(this, &UMKHVoicePanelWidget::HandleMicToggleClicked);
	}
	if (IsValid(CollapseButton))
	{
		CollapseButton->OnClicked.AddUniqueDynamic(this, &UMKHVoicePanelWidget::HandleCollapseClicked);
	}
	if (IsValid(PushToTalkKeyCap))
	{
		const UMKHOnlineSettings* Settings = UMKHOnlineSettings::Get();
		PushToTalkKeyCap->SetInputAction(IsValid(Settings) ? Settings->PushToTalkAction.LoadSynchronous() : nullptr);
	}
}

void UMKHVoicePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->OnVoiceStateChanged.AddUniqueDynamic(this, &UMKHVoicePanelWidget::HandleVoiceStateChanged);
		VoiceChat->OnTalkersChanged.AddUniqueDynamic(this, &UMKHVoicePanelWidget::HandleTalkersChanged);
	}
	HandleVoiceStateChanged();
	RefreshTalkers();
	ApplyCollapsed();
}

void UMKHVoicePanelWidget::NativeDestruct()
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

void UMKHVoicePanelWidget::HandleVoiceStateChanged()
{
	RefreshHeader();
	RefreshButtons();
}

void UMKHVoicePanelWidget::HandleTalkersChanged()
{
	RefreshTalkers();
}

void UMKHVoicePanelWidget::HandleVoiceToggleClicked()
{
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->SetVoiceChatEnabled(!VoiceChat->IsVoiceChatEnabled());
	}
}

void UMKHVoicePanelWidget::HandleMicToggleClicked()
{
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat) && VoiceChat->CanMuteMic())
	{
		VoiceChat->SetMicMuted(!VoiceChat->IsMicMuted());
	}
}

void UMKHVoicePanelWidget::HandleCollapseClicked()
{
	bCollapsed = !bCollapsed;
	ApplyCollapsed();
}

void UMKHVoicePanelWidget::RefreshHeader() const
{
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	const bool bSupported = IsValid(VoiceChat) && VoiceChat->IsVoiceChatSupported();
	if (IsValid(UnavailableText))
	{
		UnavailableText->SetVisibility(bSupported ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	ChannelText->SetText(GetChannelText());
	bool bShowKey = false;
	StatusText->SetText(GetStatusText(bShowKey));
	if (IsValid(PushToTalkKeyCap))
	{
		PushToTalkKeyCap->SetVisibility(bShowKey ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMKHVoicePanelWidget::RefreshButtons() const
{
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	const bool bSupported = IsValid(VoiceChat) && VoiceChat->IsVoiceChatSupported();
	const bool bEnabled = IsValid(VoiceChat) && VoiceChat->IsVoiceChatEnabled();
	const bool bCanMuteMic = IsValid(VoiceChat) && VoiceChat->CanMuteMic();
	const bool bMicOpen = bCanMuteMic && !VoiceChat->IsMicMuted();

	VoiceToggleButton->SetIsEnabled(bSupported);
	VoiceToggleButton->SetToolTipText(bEnabled ? LOCTEXT("TurnVoiceOff", "Turn voice chat off") : LOCTEXT("TurnVoiceOn", "Turn voice chat on"));
	MKHVoicePanel::SetIcon(VoiceToggleIcon, bEnabled ? VoiceOnIcon.Get() : VoiceOffIcon.Get());
	ApplyToggleStyle(VoiceToggleButton, !bSupported || bEnabled);

	MicToggleButton->SetIsEnabled(bSupported && bCanMuteMic);
	MicToggleButton->SetToolTipText(!bCanMuteMic ? LOCTEXT("MicUnavailable", "Microphone off (voice chat off or listen only)")
		: bMicOpen ? LOCTEXT("MuteMic", "Mute your microphone") : LOCTEXT("UnmuteMic", "Unmute your microphone"));
	MKHVoicePanel::SetIcon(MicToggleIcon, bMicOpen ? MicOnIcon.Get() : MicOffIcon.Get());
	// Red only for a muted microphone: with voice chat off or listen only the button is disabled, the headset shows why.
	ApplyToggleStyle(MicToggleButton, !bCanMuteMic || bMicOpen);
}

void UMKHVoicePanelWidget::ApplyToggleStyle(UButton* Button, bool bOn) const
{
	// An instance placed in another widget tree (W_FriendList) may hold None: the class defaults are the source.
	const UMKHVoicePanelWidget* Defaults = GetClass()->GetDefaultObject<UMKHVoicePanelWidget>();
	UMKHButton* MKHButton = Cast<UMKHButton>(Button);
	UMKHButtonStyle* WantedStyle = bOn ? ToggleOnStyle.Get() : ToggleOffStyle.Get();
	if (!IsValid(WantedStyle) && IsValid(Defaults))
	{
		WantedStyle = bOn ? Defaults->ToggleOnStyle.Get() : Defaults->ToggleOffStyle.Get();
	}
	if (!IsValid(MKHButton) || !IsValid(WantedStyle))
	{
		UE_CLOG(IsValid(MKHButton), LogMKHVoice, Warning, TEXT("VoicePanel: ToggleOnStyle / ToggleOffStyle are not set."));
		return;
	}
	if (MKHButton->GetButtonStyle() != WantedStyle)
	{
		MKHButton->SetButtonStyle(WantedStyle);
	}
}

void UMKHVoicePanelWidget::RefreshTalkers()
{
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	const TArray<FMKHVoiceTalkerInfo> Talkers = IsValid(VoiceChat) ? VoiceChat->GetTalkers() : TArray<FMKHVoiceTalkerInfo>();
	if (!IsValid(TalkerEntryClass))
	{
		UE_CLOG(!Talkers.IsEmpty(), LogMKHVoice, Warning, TEXT("VoicePanel: TalkerEntryClass is not set."));
		return;
	}
	// The rows are kept and updated in place, so the gamepad focus stays on a mute button while the list changes.
	while (TalkerEntries.Num() > Talkers.Num())
	{
		TalkerEntries.Pop()->RemoveFromParent();
	}
	while (TalkerEntries.Num() < Talkers.Num())
	{
		UMKHVoiceTalkerEntryWidget* Entry = CreateWidget<UMKHVoiceTalkerEntryWidget>(GetOwningPlayer(), TalkerEntryClass);
		if (!IsValid(Entry))
		{
			return;
		}
		if (UVerticalBoxSlot* EntrySlot = Cast<UVerticalBoxSlot>(TalkerList->AddChild(Entry)); EntrySlot != nullptr)
		{
			EntrySlot->SetHorizontalAlignment(HAlign_Fill);
		}
		TalkerEntries.Add(Entry);
	}
	for (int32 Index = 0; Index < Talkers.Num(); ++Index)
	{
		TalkerEntries[Index]->SetTalker(Talkers[Index]);
	}
}

void UMKHVoicePanelWidget::ApplyCollapsed() const
{
	if (IsValid(ContentBox))
	{
		ContentBox->SetVisibility(bCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (IsValid(CollapseButton))
	{
		CollapseButton->SetToolTipText(bCollapsed ? LOCTEXT("ExpandPanel", "Show the voice chat") : LOCTEXT("CollapsePanel", "Hide the voice chat"));
	}
	MKHVoicePanel::SetIcon(CollapseButtonIcon, bCollapsed ? ExpandIcon.Get() : CollapseIcon.Get());
}

FText UMKHVoicePanelWidget::GetChannelText() const
{
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	switch (IsValid(VoiceChat) ? VoiceChat->GetChannel() : EMKHVoiceChannel::None)
	{
	case EMKHVoiceChannel::Lobby: return LOCTEXT("ChannelLobby", "Lobby");
	case EMKHVoiceChannel::Match: return LOCTEXT("ChannelMatch", "Match");
	default:                      return LOCTEXT("ChannelNone", "Not connected");
	}
}

FText UMKHVoicePanelWidget::GetStatusText(bool& bOutShowKey) const
{
	bOutShowKey = false;
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	if (!IsValid(VoiceChat) || !VoiceChat->IsVoiceChatSupported())
	{
		return LOCTEXT("StatusUnavailable", "Voice chat unavailable");
	}
	if (!VoiceChat->IsVoiceChatEnabled())
	{
		return LOCTEXT("StatusOff", "Voice chat off");
	}
	switch (VoiceChat->GetMicMode())
	{
	case EMKHVoiceMicMode::Disabled:
		return LOCTEXT("StatusListenOnly", "Listen only");
	case EMKHVoiceMicMode::OpenMic:
		return VoiceChat->IsMicMuted() ? LOCTEXT("StatusMicMuted", "Microphone muted") : LOCTEXT("StatusOpenMic", "Open mic");
	default:
		bOutShowKey = !VoiceChat->IsMicMuted();
		return VoiceChat->IsMicMuted() ? LOCTEXT("StatusMicMuted", "Microphone muted") : LOCTEXT("StatusPushToTalk", "Push-to-talk");
	}
}

#undef LOCTEXT_NAMESPACE
