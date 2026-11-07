// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/Friends/MKHLobbyInviteEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Online/MKHFriendsSubsystem.h"

#define LOCTEXT_NAMESPACE "MKHLobbyInviteEntry"

// ============================================================
// Public Interface
// ============================================================

void UMKHLobbyInviteEntryWidget::SetInvite(const FMKHLobbyInvite& InInvite)
{
	Invite = InInvite;

	if (IsValid(MessageText))
	{
		FFormatNamedArguments MessageArgs;
		MessageArgs.Add(TEXT("Name"), FText::AsCultureInvariant(Invite.SenderDisplayName));
		MessageText->SetText(Invite.SenderDisplayName.IsEmpty()
			? LOCTEXT("InviteFromUnknown", "A player invited you to their lobby")
			: FText::Format(LOCTEXT("InviteFromFriend", "{Name} invited you to their lobby"), MessageArgs));
	}

	if (IsValid(JoinButton))
	{
		const UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this);
		JoinButton->SetIsEnabled(IsValid(Friends) && Friends->CanAcceptLobbyInvites());
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHLobbyInviteEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(JoinButton))
	{
		JoinButton->OnClicked.AddUniqueDynamic(this, &UMKHLobbyInviteEntryWidget::HandleJoinClicked);
	}

	if (IsValid(DeclineButton))
	{
		DeclineButton->OnClicked.AddUniqueDynamic(this, &UMKHLobbyInviteEntryWidget::HandleDeclineClicked);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHLobbyInviteEntryWidget::HandleJoinClicked()
{
	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->AcceptLobbyInvite(Invite.InviteId);
	}
}

void UMKHLobbyInviteEntryWidget::HandleDeclineClicked()
{
	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->DeclineLobbyInvite(Invite.InviteId);
	}
}

#undef LOCTEXT_NAMESPACE
