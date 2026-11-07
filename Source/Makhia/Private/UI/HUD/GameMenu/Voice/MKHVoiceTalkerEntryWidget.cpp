// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/Voice/MKHVoiceTalkerEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Online/MKHFriendsSubsystem.h"
#include "Voice/MKHVoiceChatSubsystem.h"

#define LOCTEXT_NAMESPACE "MKHVoicePanel"

namespace MKHVoiceTalkerEntry
{
	/** Shows or collapses an optional widget. */
	static void SetShown(UWidget* Widget, bool bShown, ESlateVisibility ShownVisibility = ESlateVisibility::Visible)
	{
		if (IsValid(Widget))
		{
			Widget->SetVisibility(bShown ? ShownVisibility : ESlateVisibility::Collapsed);
		}
	}
}

// ============================================================
// Public Interface
// ============================================================

void UMKHVoiceTalkerEntryWidget::SetTalker(const FMKHVoiceTalkerInfo& InInfo)
{
	const bool bPlayerChanged = Info.PlayerId != InInfo.PlayerId;
	Info = InInfo;
	if (IsValid(NameText))
	{
		// Player names come from the platform: shown as they are, never translated.
		NameText->SetText(FText::AsCultureInvariant(Info.DisplayName));
	}
	MKHVoiceTalkerEntry::SetShown(LocalPlayerText, Info.bIsLocalPlayer, ESlateVisibility::HitTestInvisible);
	if (IsValid(TalkingIcon))
	{
		TalkingIcon->SetVisibility(Info.bIsTalking ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	RefreshUnavailableIcon();
	RefreshMuteButton();
	if (bPlayerChanged)
	{
		RefreshAvatar();
	}
}

FText UMKHVoiceTalkerEntryWidget::GetBlockReasonText(EMKHVoiceBlockReason Reason)
{
	switch (Reason)
	{
	case EMKHVoiceBlockReason::VoiceDisabled:             return LOCTEXT("ReasonVoiceDisabled", "Your voice chat is off.");
	case EMKHVoiceBlockReason::OtherVoiceDisabled:        return LOCTEXT("ReasonOtherVoiceDisabled", "This player's voice chat is off.");
	case EMKHVoiceBlockReason::NonFriendsNotAllowed:      return LOCTEXT("ReasonNonFriends", "Not a friend: voice with non-friends is off in your settings.");
	case EMKHVoiceBlockReason::OtherNonFriendsNotAllowed: return LOCTEXT("ReasonOtherNonFriends", "Not a friend: this player does not allow voice with non-friends.");
	case EMKHVoiceBlockReason::MutedByYou:                return LOCTEXT("ReasonMuted", "You muted this player.");
	case EMKHVoiceBlockReason::Unavailable:               return LOCTEXT("ReasonUnavailable", "Voice unavailable with this player.");
	default:                                              return FText::GetEmpty();
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHVoiceTalkerEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (IsValid(AvatarImage))
	{
		PlaceholderAvatarBrush = AvatarImage->GetBrush();
	}
	if (IsValid(MuteButton))
	{
		MuteButton->OnClicked.AddUniqueDynamic(this, &UMKHVoiceTalkerEntryWidget::HandleMuteClicked);
	}
}

void UMKHVoiceTalkerEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->OnFriendAvatarChanged.AddUniqueDynamic(this, &UMKHVoiceTalkerEntryWidget::HandleAvatarChanged);
	}
	RefreshAvatar();
}

void UMKHVoiceTalkerEntryWidget::NativeDestruct()
{
	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->OnFriendAvatarChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHVoiceTalkerEntryWidget::HandleMuteClicked()
{
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat) && !Info.bIsBlocked)
	{
		VoiceChat->SetPlayerMuted(Info.PlayerId, !Info.bIsMutedByLocalPlayer);
	}
}

void UMKHVoiceTalkerEntryWidget::HandleAvatarChanged(const FString& NetId)
{
	if (NetId == Info.PlayerId)
	{
		RefreshAvatar();
	}
}

void UMKHVoiceTalkerEntryWidget::RefreshAvatar()
{
	if (!IsValid(AvatarImage))
	{
		return;
	}
	UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this);
	if (!IsValid(Friends) || !Friends->SupportsAvatars())
	{
		AvatarImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	AvatarImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UTexture2D* Avatar = Info.PlayerId.IsEmpty() ? nullptr : Friends->GetFriendAvatar(Info.PlayerId); IsValid(Avatar))
	{
		AvatarImage->SetBrushFromTexture(Avatar, false);
		return;
	}
	AvatarImage->SetBrush(PlaceholderAvatarBrush);
}

void UMKHVoiceTalkerEntryWidget::RefreshMuteButton()
{
	MKHVoiceTalkerEntry::SetShown(MuteButton, !Info.bIsLocalPlayer);
	if (!IsValid(MuteButton) || Info.bIsLocalPlayer)
	{
		return;
	}
	MuteButton->SetIsEnabled(!Info.bIsBlocked);
	const FText Action = Info.bIsBlocked ? LOCTEXT("BlockedTooltip", "Blocked on the platform: always muted.")
		: Info.bIsMutedByLocalPlayer ? LOCTEXT("UnmuteTooltip", "Unmute this player") : LOCTEXT("MuteTooltip", "Mute this player");
	// The pad shows only the focused widget's tooltip: the reason the voice is closed goes on the button too.
	const FText Reason = Info.BlockReason == EMKHVoiceBlockReason::MutedByYou ? FText::GetEmpty() : GetBlockReasonText(Info.BlockReason);
	MuteButton->SetToolTipText(Reason.IsEmpty() ? Action
		: FText::Format(LOCTEXT("MuteTooltipWithReason", "{Action}\n{Reason}"), FFormatNamedArguments{ { TEXT("Action"), Action }, { TEXT("Reason"), Reason } }));
	if (UTexture2D* Icon = Info.bIsMutedByLocalPlayer ? MutedIcon.Get() : UnmutedIcon.Get(); IsValid(MuteIcon) && IsValid(Icon))
	{
		MuteIcon->SetBrushFromTexture(Icon, false);
	}
}

void UMKHVoiceTalkerEntryWidget::RefreshUnavailableIcon()
{
	// A muted player already shows it on the mute button.
	const bool bUnavailable = !Info.bIsLocalPlayer && Info.BlockReason != EMKHVoiceBlockReason::None
		&& Info.BlockReason != EMKHVoiceBlockReason::MutedByYou;
	MKHVoiceTalkerEntry::SetShown(UnavailableIcon, bUnavailable);
	if (IsValid(UnavailableIcon))
	{
		UnavailableIcon->SetToolTipText(GetBlockReasonText(Info.BlockReason));
	}
}

#undef LOCTEXT_NAMESPACE
