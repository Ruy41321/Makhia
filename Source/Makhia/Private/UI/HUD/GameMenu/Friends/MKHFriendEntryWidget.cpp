// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/Friends/MKHFriendEntryWidget.h"

#include "Career/MKHCareerSubsystem.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "MKHLogChannels.h"
#include "Online/MKHFriendsSubsystem.h"

#define LOCTEXT_NAMESPACE "MKHFriendEntry"

namespace MKHFriendEntry
{
	/** Shows or collapses an optional widget. */
	static void SetShown(UWidget* Widget, const bool bShown)
	{
		if (IsValid(Widget))
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHFriendEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(InviteButton))
	{
		InviteButton->OnClicked.AddUniqueDynamic(this, &UMKHFriendEntryWidget::HandleInviteClicked);
	}

	if (IsValid(AcceptButton))
	{
		AcceptButton->OnClicked.AddUniqueDynamic(this, &UMKHFriendEntryWidget::HandleAcceptClicked);
	}

	if (IsValid(RejectButton))
	{
		RejectButton->OnClicked.AddUniqueDynamic(this, &UMKHFriendEntryWidget::HandleRejectClicked);
	}

	if (IsValid(PlatformButton))
	{
		PlatformButton->OnClicked.AddUniqueDynamic(this, &UMKHFriendEntryWidget::HandlePlatformClicked);
	}

	if (IsValid(AvatarImage))
	{
		PlaceholderAvatarBrush = AvatarImage->GetBrush();
	}
}

void UMKHFriendEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->OnFriendAvatarChanged.AddUniqueDynamic(this, &UMKHFriendEntryWidget::HandleAvatarChanged);
	}
}

void UMKHFriendEntryWidget::NativeDestruct()
{
	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->OnFriendAvatarChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMKHFriendEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	const UMKHFriendListItem* Item = Cast<UMKHFriendListItem>(ListItemObject);
	if (!IsValid(Item))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHFriendEntryWidget: list item is not a UMKHFriendListItem."));
		return;
	}

	Info = Item->Info;
	RefreshEntry();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHFriendEntryWidget::RefreshEntry()
{
	if (IsValid(NameText))
	{
		NameText->SetText(FText::AsCultureInvariant(Info.DisplayName));
	}

	if (IsValid(StatusText))
	{
		StatusText->SetText(GetStatusText());
	}

	if (IsValid(PresenceIndicator))
	{
		PresenceIndicator->SetColorAndOpacity(GetPresenceColor());
	}

	const bool bDimmed = Info.Relation == EMKHFriendRelation::Friend && Info.Presence == EMKHFriendPresence::Offline;
	SetRenderOpacity(bDimmed ? OfflineOpacity : 1.0f);

	RefreshAvatar();
	RefreshActions();
}

void UMKHFriendEntryWidget::RefreshActions()
{
	const UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this);
	// Requests are answered in the game only when the backend allows it (Steam: in its overlay).
	const bool bIsRequest = Info.Relation == EMKHFriendRelation::RequestReceived && IsValid(Friends) && Friends->SupportsInGameFriendRequests();
	const bool bCanInvite = Info.Relation == EMKHFriendRelation::Friend && Info.Presence == EMKHFriendPresence::InMakhia
		&& Info.bHasPlayedMakhia && IsValid(Friends) && Friends->CanInviteToLobby();

	// Otherwise the platform overlay takes over: a received request is answered there, a recent player added there.
	const bool bHasOverlay = IsValid(Friends) && Friends->HasPlatformOverlay();
	const bool bRespondOnPlatform = bHasOverlay && Info.Relation == EMKHFriendRelation::RequestReceived && !bIsRequest;
	const bool bAddOnPlatform = bHasOverlay && Info.Relation == EMKHFriendRelation::RecentPlayer;

	MKHFriendEntry::SetShown(InviteButton, bCanInvite);
	MKHFriendEntry::SetShown(AcceptButton, bIsRequest);
	MKHFriendEntry::SetShown(RejectButton, bIsRequest);
	MKHFriendEntry::SetShown(PlatformButton, bRespondOnPlatform || bAddOnPlatform);

	if (IsValid(PlatformButtonText) && (bRespondOnPlatform || bAddOnPlatform))
	{
		PlatformButtonText->SetText(bAddOnPlatform
			? LOCTEXT("AddFriend", "Add friend")
			: FText::Format(LOCTEXT("RespondOnPlatform", "Respond on {Platform}"),
				FFormatNamedArguments{ { TEXT("Platform"), Friends->GetPlatformDisplayName() } }));
	}

	if (IsValid(InviteButton))
	{
		InviteButton->SetIsEnabled(!Info.bInvitedToLobby);
	}
}

FText UMKHFriendEntryWidget::GetStatusText() const
{
	switch (Info.Relation)
	{
	case EMKHFriendRelation::RequestReceived:
		return LOCTEXT("RequestReceived", "Wants to be your friend");
	case EMKHFriendRelation::RequestSent:
		return LOCTEXT("RequestSent", "Friend request sent");
	case EMKHFriendRelation::RecentPlayer:
		return Info.LastPlayedTogether.GetTicks() > 0
			? FText::Format(LOCTEXT("PlayedTogetherOn", "Played together on {Date}"),
				FFormatNamedArguments{ { TEXT("Date"), FText::AsDate(Info.LastPlayedTogether, EDateTimeStyle::Medium) } })
			: LOCTEXT("PlayedTogether", "Played together recently");
	default:
		break;
	}

	if (Info.bInvitedToLobby)
	{
		return LOCTEXT("InvitedToLobby", "Invited to your lobby");
	}

	switch (Info.Presence)
	{
	case EMKHFriendPresence::InYourParty:
		return AddCareerTitle(LOCTEXT("PresenceInYourParty", "In your party"));
	case EMKHFriendPresence::InMakhia:
		return AddCareerTitle(GetActivityText());
	case EMKHFriendPresence::Online:
		return LOCTEXT("PresenceOnline", "Online");
	case EMKHFriendPresence::Away:
		return LOCTEXT("PresenceAway", "Away");
	default:
		return LOCTEXT("PresenceOffline", "Offline");
	}
}

FText UMKHFriendEntryWidget::AddCareerTitle(const FText& Status) const
{
	const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
	if (!IsValid(Career) || Career->GetLevelCount() == 0)
	{
		return Status;
	}

	return FText::Format(LOCTEXT("StatusWithCareerTitle", "{Title} - {Status}"), FFormatNamedArguments{
		{ TEXT("Title"), Career->GetTitleForCompletedLevels(Info.Activity.CareerLevels) }, { TEXT("Status"), Status } });
}

FText UMKHFriendEntryWidget::GetActivityText() const
{
	const FMKHPlayerActivity& Activity = Info.Activity;
	if (Activity.Activity == EMKHPlayerActivity::InLobby)
	{
		return FText::Format(LOCTEXT("ActivityLobby", "Makhia: In lobby {Players}/{MaxPlayers}"), FFormatNamedArguments{
			{ TEXT("Players"), FText::AsNumber(Activity.LobbyPlayers) }, { TEXT("MaxPlayers"), FText::AsNumber(Activity.LobbyMaxPlayers) } });
	}
	if (Activity.Activity != EMKHPlayerActivity::OfflineMatch && Activity.Activity != EMKHPlayerActivity::OnlineMatch)
	{
		return LOCTEXT("PresenceInMakhia", "Playing Makhia");
	}

	const bool bOnline = Activity.Activity == EMKHPlayerActivity::OnlineMatch;
	const FText Match = bOnline ? LOCTEXT("ActivityOnlineMatch", "Makhia: In online match") : LOCTEXT("ActivityOfflineMatch", "Makhia: In offline match");
	if (Activity.Round <= 0)
	{
		return Match;
	}
	return FText::Format(LOCTEXT("ActivityMatchRound", "{Match} - Round {Round} - {FriendScore} : {OpponentScore}"), FFormatNamedArguments{
		{ TEXT("Match"), Match }, { TEXT("Round"), FText::AsNumber(Activity.Round) },
		{ TEXT("FriendScore"), FText::AsNumber(Activity.OwnScore) }, { TEXT("OpponentScore"), FText::AsNumber(Activity.OpponentScore) } });
}

FLinearColor UMKHFriendEntryWidget::GetPresenceColor() const
{
	if (Info.Relation == EMKHFriendRelation::RecentPlayer)
	{
		return RecentPlayerColor;
	}
	if (Info.Relation != EMKHFriendRelation::Friend)
	{
		return RequestColor;
	}

	switch (Info.Presence)
	{
	case EMKHFriendPresence::InYourParty:
	case EMKHFriendPresence::InMakhia: return InMakhiaColor;
	case EMKHFriendPresence::Online:   return OnlineColor;
	case EMKHFriendPresence::Away:     return AwayColor;
	default:                           return OfflineColor;
	}
}

void UMKHFriendEntryWidget::HandleInviteClicked()
{
	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->InviteToLobby(Info.NetId);
	}
}

void UMKHFriendEntryWidget::HandleAcceptClicked()
{
	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->AcceptFriendRequest(Info.NetId);
	}
}

void UMKHFriendEntryWidget::HandleRejectClicked()
{
	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->RejectFriendRequest(Info.NetId);
	}
}

void UMKHFriendEntryWidget::HandlePlatformClicked()
{
	UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this);
	if (!IsValid(Friends))
	{
		return;
	}

	if (Info.Relation == EMKHFriendRelation::RecentPlayer)
	{
		Friends->AddFriendOnPlatform(Info.NetId);
		return;
	}
	Friends->OpenPlatformProfile(Info.NetId);
}

void UMKHFriendEntryWidget::RefreshAvatar()
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
	if (UTexture2D* Avatar = Friends->GetFriendAvatar(Info.NetId); IsValid(Avatar))
	{
		AvatarImage->SetBrushFromTexture(Avatar, false);
		return;
	}
	AvatarImage->SetBrush(PlaceholderAvatarBrush);
}

void UMKHFriendEntryWidget::HandleAvatarChanged(const FString& NetId)
{
	if (NetId == Info.NetId)
	{
		RefreshAvatar();
	}
}

#undef LOCTEXT_NAMESPACE
