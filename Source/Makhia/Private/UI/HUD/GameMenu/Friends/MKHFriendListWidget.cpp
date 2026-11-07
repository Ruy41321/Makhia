// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/Friends/MKHFriendListWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ListView.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "MKHLogChannels.h"
#include "Online/MKHFriendsSubsystem.h"
#include "UI/HUD/GameMenu/Friends/MKHFriendEntryWidget.h"
#include "UI/HUD/GameMenu/Friends/MKHLobbyInviteEntryWidget.h"

#define LOCTEXT_NAMESPACE "MKHFriendList"

// ============================================================
// Public Interface
// ============================================================

void UMKHFriendListWidget::RefreshFriendList()
{
	if (UMKHFriendsSubsystem* Friends = FriendsSubsystem.Get(); IsValid(Friends))
	{
		Friends->RefreshFriends();
	}

	RefreshFriendsView();
	RefreshLobbyInvitesView();
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHFriendListWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	OnVisibilityChanged.AddUniqueDynamic(this, &UMKHFriendListWidget::HandleVisibilityChanged);

	if (IsValid(RefreshButton))
	{
		RefreshButton->OnClicked.AddUniqueDynamic(this, &UMKHFriendListWidget::HandleRefreshClicked);
	}

	if (IsValid(AddFriendButton))
	{
		AddFriendButton->OnClicked.AddUniqueDynamic(this, &UMKHFriendListWidget::HandleAddFriendClicked);
	}

	if (IsValid(AddFriendNameBox))
	{
		AddFriendNameBox->OnTextCommitted.AddUniqueDynamic(this, &UMKHFriendListWidget::HandleNameCommitted);
	}

	if (IsValid(PlatformFriendsButton))
	{
		PlatformFriendsButton->OnClicked.AddUniqueDynamic(this, &UMKHFriendListWidget::HandlePlatformFriendsClicked);
	}
}

void UMKHFriendListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindSubsystem();
	ShowFeedback(AddFriendResultText, FText::GetEmpty(), false);
	ShowFeedback(LobbyInviteResultText, FText::GetEmpty(), false);
	RefreshFriendList();
}

void UMKHFriendListWidget::NativeDestruct()
{
	UnbindSubsystem();

	Super::NativeDestruct();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHFriendListWidget::BindSubsystem()
{
	UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this);
	if (!IsValid(Friends))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHFriendListWidget: the friends subsystem is missing."));
		return;
	}

	FriendsSubsystem = Friends;
	Friends->OnFriendsChanged.AddUniqueDynamic(this, &UMKHFriendListWidget::HandleFriendsChanged);
	Friends->OnLobbyInvitesChanged.AddUniqueDynamic(this, &UMKHFriendListWidget::HandleLobbyInvitesChanged);
	Friends->OnFriendRequestResult.AddUniqueDynamic(this, &UMKHFriendListWidget::HandleFriendRequestResult);
	Friends->OnLobbyInviteAcceptResult.AddUniqueDynamic(this, &UMKHFriendListWidget::HandleLobbyInviteAcceptResult);
}

void UMKHFriendListWidget::UnbindSubsystem()
{
	if (UMKHFriendsSubsystem* Friends = FriendsSubsystem.Get(); IsValid(Friends))
	{
		Friends->OnFriendsChanged.RemoveAll(this);
		Friends->OnLobbyInvitesChanged.RemoveAll(this);
		Friends->OnFriendRequestResult.RemoveAll(this);
		Friends->OnLobbyInviteAcceptResult.RemoveAll(this);
	}

	FriendsSubsystem.Reset();
}

void UMKHFriendListWidget::RefreshFriendsView()
{
	const UMKHFriendsSubsystem* Friends = FriendsSubsystem.Get();
	const EMKHFriendsServiceState State = IsValid(Friends) ? Friends->GetServiceState() : EMKHFriendsServiceState::Unavailable;
	const TArray<FMKHFriendInfo> Entries = IsValid(Friends) ? Friends->GetFriends() : TArray<FMKHFriendInfo>();

	FillListView(FriendListView, Entries);

	const FText StateMessage = GetStateMessage(State, Entries.IsEmpty());
	if (IsValid(StateText))
	{
		StateText->SetText(StateMessage);
		StateText->SetVisibility(StateMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	const bool bIsReady = State == EMKHFriendsServiceState::Ready;
	if (IsValid(CountText))
	{
		FFormatNamedArguments CountArgs;
		CountArgs.Add(TEXT("Online"), FText::AsNumber(IsValid(Friends) ? Friends->GetOnlineFriendCount() : 0));
		CountArgs.Add(TEXT("Total"), FText::AsNumber(IsValid(Friends) ? Friends->GetFriendCount() : 0));
		CountText->SetText(FText::Format(LOCTEXT("OnlineCount", "{Online} / {Total} online"), CountArgs));
		CountText->SetVisibility(bIsReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IsValid(AddFriendSection))
	{
		// Shown as soon as the player is signed in with a social account, whatever the list holds, when the backend
		// lets the game send friend requests (Steam leaves them to its overlay).
		const bool bSignedIn = bIsReady || State == EMKHFriendsServiceState::Loading || State == EMKHFriendsServiceState::Error;
		const bool bCanAddFriends = bSignedIn && IsValid(Friends) && Friends->SupportsInGameFriendRequests();
		AddFriendSection->SetVisibility(bCanAddFriends ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (IsValid(AddFriendButton))
	{
		AddFriendButton->SetIsEnabled(IsValid(Friends) && !Friends->IsFriendRequestPending());
	}

	RefreshPlatformView();
}

void UMKHFriendListWidget::RefreshPlatformView()
{
	const UMKHFriendsSubsystem* Friends = FriendsSubsystem.Get();
	const bool bIsReady = IsValid(Friends) && Friends->GetServiceState() == EMKHFriendsServiceState::Ready;

	const TArray<FMKHFriendInfo> RecentPlayers = bIsReady ? Friends->GetRecentPlayers() : TArray<FMKHFriendInfo>();
	FillListView(RecentPlayersListView, RecentPlayers);
	if (IsValid(RecentPlayersSection))
	{
		RecentPlayersSection->SetVisibility(RecentPlayers.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	const bool bShowPlatformButton = bIsReady && Friends->HasPlatformOverlay();
	if (IsValid(PlatformFriendsButton))
	{
		PlatformFriendsButton->SetVisibility(bShowPlatformButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (IsValid(PlatformFriendsButtonText) && bShowPlatformButton)
	{
		PlatformFriendsButtonText->SetText(FText::Format(LOCTEXT("PlatformFriends", "{Platform} friends"),
			FFormatNamedArguments{ { TEXT("Platform"), Friends->GetPlatformDisplayName() } }));
	}
}

void UMKHFriendListWidget::FillListView(UListView* ListView, const TArray<FMKHFriendInfo>& Entries)
{
	if (!IsValid(ListView))
	{
		return;
	}

	TArray<UMKHFriendListItem*> Items;
	Items.Reserve(Entries.Num());
	for (const FMKHFriendInfo& Entry : Entries)
	{
		UMKHFriendListItem* Item = NewObject<UMKHFriendListItem>(this);
		Item->Info = Entry;
		Items.Add(Item);
	}

	ListView->SetListItems(Items);
	ListView->SetVisibility(Entries.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UMKHFriendListWidget::RefreshLobbyInvitesView()
{
	const UMKHFriendsSubsystem* Friends = FriendsSubsystem.Get();
	const TArray<FMKHLobbyInvite> Invites = IsValid(Friends) ? Friends->GetLobbyInvites() : TArray<FMKHLobbyInvite>();

	if (IsValid(LobbyInvitesSection))
	{
		LobbyInvitesSection->SetVisibility(Invites.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (!IsValid(LobbyInvitesContainer))
	{
		return;
	}

	LobbyInvitesContainer->ClearChildren();
	if (!LobbyInviteEntryClass)
	{
		if (!Invites.IsEmpty())
		{
			UE_LOG(LogMKHUI, Warning, TEXT("UMKHFriendListWidget: LobbyInviteEntryClass is not set on %s."), *GetClass()->GetName());
		}
		return;
	}

	for (int32 Index = 0; Index < Invites.Num(); ++Index)
	{
		UMKHLobbyInviteEntryWidget* Entry = CreateWidget<UMKHLobbyInviteEntryWidget>(this, LobbyInviteEntryClass);
		if (!IsValid(Entry))
		{
			continue;
		}

		Entry->SetInvite(Invites[Index]);
		if (UVerticalBoxSlot* EntrySlot = Cast<UVerticalBoxSlot>(LobbyInvitesContainer->AddChild(Entry)))
		{
			EntrySlot->SetPadding(FMargin(0.0f, Index == 0 ? 0.0f : LobbyInviteSpacing, 0.0f, 0.0f));
		}
	}
}

FText UMKHFriendListWidget::GetStateMessage(const EMKHFriendsServiceState State, const bool bListIsEmpty) const
{
	switch (State)
	{
	case EMKHFriendsServiceState::Unavailable:
		return LOCTEXT("StateUnavailable", "Online services are not available");
	case EMKHFriendsServiceState::SignedOut:
		return LOCTEXT("StateSignedOut", "Log in to see your friends");
	case EMKHFriendsServiceState::NoSocialAccount:
		return LOCTEXT("StateNoSocialAccount", "Sign in with your platform account to see your friends");
	case EMKHFriendsServiceState::Loading:
		return LOCTEXT("StateLoading", "Loading friends...");
	case EMKHFriendsServiceState::Error:
		return LOCTEXT("StateError", "Could not load your friends. Try again.");
	default:
		break;
	}

	if (!bListIsEmpty)
	{
		return FText::GetEmpty();
	}

	// Without in-game friend requests the friends are added on the platform (its overlay or client).
	const UMKHFriendsSubsystem* Friends = FriendsSubsystem.Get();
	if (IsValid(Friends) && !Friends->SupportsInGameFriendRequests())
	{
		return FText::Format(LOCTEXT("StateEmptyPlatform", "No friends yet. Add friends on {Platform}."),
			FFormatNamedArguments{ { TEXT("Platform"), Friends->GetPlatformDisplayName() } });
	}
	return LOCTEXT("StateEmpty", "No friends yet. Add one with their display name.");
}

FText UMKHFriendListWidget::GetFriendRequestMessage(const EMKHFriendRequestResult Result)
{
	switch (Result)
	{
	case EMKHFriendRequestResult::Sent:
		return LOCTEXT("RequestResultSent", "Friend request sent");
	case EMKHFriendRequestResult::InvalidName:
		return LOCTEXT("RequestResultInvalidName", "Enter the display name of another player");
	case EMKHFriendRequestResult::UserNotFound:
		return LOCTEXT("RequestResultNotFound", "No player found with that name");
	case EMKHFriendRequestResult::AlreadyFriends:
		return LOCTEXT("RequestResultAlreadyFriends", "Already in your friend list");
	default:
		return LOCTEXT("RequestResultFailed", "The request could not be sent");
	}
}

void UMKHFriendListWidget::SubmitFriendRequest()
{
	UMKHFriendsSubsystem* Friends = FriendsSubsystem.Get();
	if (!IsValid(Friends) || !IsValid(AddFriendNameBox) || Friends->IsFriendRequestPending())
	{
		return;
	}

	ShowFeedback(AddFriendResultText, LOCTEXT("RequestSending", "Sending request..."), false);
	Friends->SendFriendRequest(AddFriendNameBox->GetText().ToString());

	if (IsValid(AddFriendButton))
	{
		AddFriendButton->SetIsEnabled(!Friends->IsFriendRequestPending());
	}
}

void UMKHFriendListWidget::ShowFeedback(UTextBlock* Target, const FText& Message, const bool bIsError) const
{
	if (!IsValid(Target))
	{
		return;
	}

	Target->SetText(Message);
	Target->SetColorAndOpacity(FSlateColor(bIsError ? ErrorColor : SuccessColor));
	Target->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UMKHFriendListWidget::HandleFriendsChanged()
{
	RefreshFriendsView();
}

void UMKHFriendListWidget::HandleLobbyInvitesChanged()
{
	RefreshLobbyInvitesView();
}

void UMKHFriendListWidget::HandleFriendRequestResult(const EMKHFriendRequestResult Result)
{
	const bool bSent = Result == EMKHFriendRequestResult::Sent;
	ShowFeedback(AddFriendResultText, GetFriendRequestMessage(Result), !bSent);

	if (bSent && IsValid(AddFriendNameBox))
	{
		AddFriendNameBox->SetText(FText::GetEmpty());
	}

	if (IsValid(AddFriendButton))
	{
		AddFriendButton->SetIsEnabled(true);
	}
}

void UMKHFriendListWidget::HandleLobbyInviteAcceptResult(const bool bWasSuccessful)
{
	ShowFeedback(LobbyInviteResultText, bWasSuccessful
		? LOCTEXT("InviteJoining", "Joining the lobby...")
		: LOCTEXT("InviteExpired", "That lobby is no longer available"), !bWasSuccessful);
}

void UMKHFriendListWidget::HandleVisibilityChanged(const ESlateVisibility InVisibility)
{
	if (InVisibility != ESlateVisibility::Collapsed && InVisibility != ESlateVisibility::Hidden)
	{
		RefreshFriendList();
	}
}

void UMKHFriendListWidget::HandleRefreshClicked()
{
	RefreshFriendList();
}

void UMKHFriendListWidget::HandleAddFriendClicked()
{
	SubmitFriendRequest();
}

void UMKHFriendListWidget::HandlePlatformFriendsClicked()
{
	if (UMKHFriendsSubsystem* Friends = FriendsSubsystem.Get(); IsValid(Friends))
	{
		Friends->OpenPlatformFriends();
	}
}

void UMKHFriendListWidget::HandleNameCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SubmitFriendRequest();
	}
}

#undef LOCTEXT_NAMESPACE
