// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/MKHFriendsSubsystem.h"

#include "Career/MKHCareerSubsystem.h"
#include "Character/CharacterBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameInstance/MKHGameInstance.h"
#include "GameMode/MKHGameState.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "MKHLogChannels.h"
#include "Online/Backend/MKHOnlineBackend.h"
#include "Online/Backend/MKHOnlineServicesSubsystem.h"
#include "Online/Auth/MKHAuthSubsystem.h"
#include "Online/Backend/MKHSocialProvider.h"
#include "Online/MKHSessionSubsystem.h"
#include "TimerManager.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHFriendsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UMKHOnlineServicesSubsystem* Services = Collection.InitializeDependency<UMKHOnlineServicesSubsystem>();
	Collection.InitializeDependency<UMKHSessionSubsystem>();
	// Social providers may follow the game sign-in (e.g. Steam, where signing out only takes the game offline).
	Collection.InitializeDependency<UMKHAuthSubsystem>();
	const UMKHOnlineBackend* Backend = IsValid(Services) ? Services->GetBackend() : nullptr;
	const TSubclassOf<UMKHSocialProvider> ProviderClass = IsValid(Backend) ? Backend->GetSocialProviderClass() : nullptr;
	if (IsValid(ProviderClass) && !ProviderClass->HasAnyClassFlags(CLASS_Abstract))
	{
		Provider = NewObject<UMKHSocialProvider>(this, ProviderClass);
		Provider->OnLoginStateChanged.AddUObject(this, &UMKHFriendsSubsystem::EvaluateLoginState);
		Provider->OnFriendsChanged.AddUObject(this, &UMKHFriendsSubsystem::HandleProviderFriendsChanged);
		Provider->OnFriendsListStale.AddUObject(this, &UMKHFriendsSubsystem::RefreshFriends);
		Provider->OnLobbyInviteReceived.AddUObject(this, &UMKHFriendsSubsystem::HandleLobbyInviteReceived);
		Provider->OnAvatarChanged.AddUObject(this, &UMKHFriendsSubsystem::HandleAvatarChanged);
		Provider->InitializeProvider();
	}

	EvaluateLoginState();
}

void UMKHFriendsSubsystem::Deinitialize()
{
	DeactivateService(EMKHFriendsServiceState::Unavailable);

	if (IsValid(Provider))
	{
		Provider->OnLoginStateChanged.RemoveAll(this);
		Provider->OnFriendsChanged.RemoveAll(this);
		Provider->OnFriendsListStale.RemoveAll(this);
		Provider->OnLobbyInviteReceived.RemoveAll(this);
		Provider->OnAvatarChanged.RemoveAll(this);
		Provider->ShutdownProvider();
	}
	Provider = nullptr;

	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHFriendsSubsystem* UMKHFriendsSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHFriendsSubsystem>() : nullptr;
}

void UMKHFriendsSubsystem::RefreshFriends()
{
	// Login notifications can be missed (backend enabled after start-up, login before this subsystem existed).
	if (!bIsServiceActive)
	{
		EvaluateLoginState();
		return;
	}

	if (ServiceState != EMKHFriendsServiceState::Ready)
	{
		SetServiceState(EMKHFriendsServiceState::Loading);
	}
	Provider->ReadFriends(FMKHFriendsReadComplete::CreateUObject(this, &UMKHFriendsSubsystem::HandleReadFriendsComplete));
}

int32 UMKHFriendsSubsystem::GetOnlineFriendCount() const
{
	return Friends.FilterByPredicate([](const FMKHFriendInfo& Info)
	{
		return Info.Relation == EMKHFriendRelation::Friend && Info.Presence != EMKHFriendPresence::Offline;
	}).Num();
}

int32 UMKHFriendsSubsystem::GetFriendCount() const
{
	return Friends.FilterByPredicate([](const FMKHFriendInfo& Info)
	{
		return Info.Relation == EMKHFriendRelation::Friend;
	}).Num();
}

bool UMKHFriendsSubsystem::IsFriend(const FString& NetId) const
{
	return !NetId.IsEmpty() && Friends.ContainsByPredicate([&NetId](const FMKHFriendInfo& Info)
	{
		return Info.Relation == EMKHFriendRelation::Friend && Info.NetId == NetId;
	});
}

bool UMKHFriendsSubsystem::IsBlockedUser(const FString& NetId) const
{
	return bIsServiceActive && IsValid(Provider) && !NetId.IsEmpty() && Provider->IsBlocked(NetId);
}

int32 UMKHFriendsSubsystem::GetPendingNotificationCount() const
{
	const int32 RequestCount = Friends.FilterByPredicate([](const FMKHFriendInfo& Info)
	{
		return Info.Relation == EMKHFriendRelation::RequestReceived;
	}).Num();
	return LobbyInvites.Num() + RequestCount;
}

bool UMKHFriendsSubsystem::SupportsInGameFriendRequests() const
{
	const UMKHOnlineServicesSubsystem* Services = UMKHOnlineServicesSubsystem::Get(this);
	return IsValid(Services) && Services->GetCapabilities().bSupportsInGameFriendRequests;
}

void UMKHFriendsSubsystem::AcceptFriendRequest(const FString& NetId)
{
	if (!bIsServiceActive || !SupportsInGameFriendRequests())
	{
		UE_LOG(LogMKHSocial, Warning, TEXT("UMKHFriendsSubsystem::AcceptFriendRequest: not supported or service inactive."));
		return;
	}
	Provider->AcceptFriendRequest(NetId, FSimpleDelegate::CreateUObject(this, &UMKHFriendsSubsystem::RefreshFriends));
}

void UMKHFriendsSubsystem::RejectFriendRequest(const FString& NetId)
{
	if (!bIsServiceActive || !SupportsInGameFriendRequests() || !Provider->RejectFriendRequest(NetId))
	{
		UE_LOG(LogMKHSocial, Warning, TEXT("UMKHFriendsSubsystem::RejectFriendRequest: not supported, service inactive or refused."));
		return;
	}

	// Some backends report no completion for a rejection: drop the entry now, the next read confirms it.
	Friends.RemoveAll([&NetId](const FMKHFriendInfo& Info) { return Info.NetId == NetId; });
	OnFriendsChanged.Broadcast();
}

void UMKHFriendsSubsystem::SendFriendRequest(const FString& DisplayName)
{
	if (IsFriendRequestPending())
	{
		return;
	}

	const FString TrimmedName = DisplayName.TrimStartAndEnd();
	if (TrimmedName.IsEmpty() || !bIsServiceActive || !SupportsInGameFriendRequests())
	{
		OnFriendRequestResult.Broadcast(TrimmedName.IsEmpty() ? EMKHFriendRequestResult::InvalidName : EMKHFriendRequestResult::Failed);
		return;
	}

	const bool bAlreadyListed = Friends.ContainsByPredicate([&TrimmedName](const FMKHFriendInfo& Info)
	{
		return Info.DisplayName.Equals(TrimmedName, ESearchCase::IgnoreCase);
	});
	if (bAlreadyListed)
	{
		OnFriendRequestResult.Broadcast(EMKHFriendRequestResult::AlreadyFriends);
		return;
	}

	PendingFriendRequestName = TrimmedName;
	Provider->SendFriendRequest(TrimmedName, FMKHFriendRequestComplete::CreateUObject(this, &UMKHFriendsSubsystem::FinishFriendRequest));
}

bool UMKHFriendsSubsystem::CanInviteToLobby() const
{
	const UMKHSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UMKHSessionSubsystem>();
	return bIsServiceActive && IsInLobbyLevel() && IsValid(Sessions) && Sessions->HasFreeLobbySlot();
}

bool UMKHFriendsSubsystem::InviteToLobby(const FString& NetId)
{
	UMKHSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UMKHSessionSubsystem>();
	if (!CanInviteToLobby() || !IsValid(Sessions))
	{
		UE_LOG(LogMKHSocial, Warning, TEXT("UMKHFriendsSubsystem::InviteToLobby: no lobby to invite to."));
		return false;
	}
	if (!Sessions->InviteFriendToLobby(NetId))
	{
		UE_LOG(LogMKHSocial, Warning, TEXT("UMKHFriendsSubsystem::InviteToLobby: the invite was refused (has the friend played Makhia?)."));
		return false;
	}

	InvitedFriendNetIds.Add(NetId);
	if (FMKHFriendInfo* Info = Friends.FindByPredicate([&NetId](const FMKHFriendInfo& Entry) { return Entry.NetId == NetId; }))
	{
		Info->bInvitedToLobby = true;
	}
	OnFriendsChanged.Broadcast();
	return true;
}

bool UMKHFriendsSubsystem::CanAcceptLobbyInvites() const
{
	return bIsServiceActive && IsInLobbyLevel();
}

void UMKHFriendsSubsystem::AcceptLobbyInvite(const FString& InviteId)
{
	const FMKHLobbyInvite* Invite = LobbyInvites.FindByPredicate([&InviteId](const FMKHLobbyInvite& Entry) { return Entry.InviteId == InviteId; });
	if (!Invite || !CanAcceptLobbyInvites())
	{
		RemoveLobbyInvite(InviteId);
		OnLobbyInviteAcceptResult.Broadcast(false);
		return;
	}

	Provider->ResolveLobbyInvite(*Invite, FMKHLobbyInviteResolved::CreateUObject(this, &UMKHFriendsSubsystem::HandleInviteResolved, InviteId));
}

void UMKHFriendsSubsystem::DeclineLobbyInvite(const FString& InviteId)
{
	const FMKHLobbyInvite* Invite = LobbyInvites.FindByPredicate([&InviteId](const FMKHLobbyInvite& Entry) { return Entry.InviteId == InviteId; });
	if (Invite && IsValid(Provider))
	{
		Provider->DeclineLobbyInvite(*Invite);
	}
	RemoveLobbyInvite(InviteId);
}

bool UMKHFriendsSubsystem::SupportsRecentPlayers() const
{
	const UMKHOnlineServicesSubsystem* Services = UMKHOnlineServicesSubsystem::Get(this);
	return IsValid(Provider) && IsValid(Services) && Services->GetCapabilities().bSupportsRecentPlayers;
}

void UMKHFriendsSubsystem::ReportMatchOpponents(const AGameStateBase* GameState)
{
	if (!bIsServiceActive || !SupportsRecentPlayers() || !IsValid(GameState))
	{
		return;
	}

	// The local player is skipped by id: its controller may not have its player state yet on a client.
	const UMKHAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UMKHAuthSubsystem>();
	const FString LocalUserId = IsValid(Auth) ? Auth->GetUser().UserId : FString();
	TArray<FString> OpponentIds;
	for (const TObjectPtr<APlayerState>& PlayerState : GameState->PlayerArray)
	{
		if (!IsValid(PlayerState) || PlayerState->IsABot() || !PlayerState->GetUniqueId().IsValid())
		{
			continue;
		}
		const FString UserId = PlayerState->GetUniqueId()->ToString();
		if (UserId != LocalUserId)
		{
			OpponentIds.Add(UserId);
		}
	}

	if (!OpponentIds.IsEmpty())
	{
		UE_LOG(LogMKHSocial, Log, TEXT("UMKHFriendsSubsystem: reporting %d match opponent(s) as recent players."), OpponentIds.Num());
		Provider->ReportPlayedWith(OpponentIds);
		RequestCacheRebuild();
	}
}

bool UMKHFriendsSubsystem::SupportsAvatars() const
{
	const UMKHOnlineServicesSubsystem* Services = UMKHOnlineServicesSubsystem::Get(this);
	return IsValid(Provider) && IsValid(Services) && Services->GetCapabilities().bSupportsAvatars;
}

UTexture2D* UMKHFriendsSubsystem::GetFriendAvatar(const FString& NetId)
{
	return bIsServiceActive && SupportsAvatars() && !NetId.IsEmpty() ? Provider->GetAvatar(NetId) : nullptr;
}

bool UMKHFriendsSubsystem::HasPlatformOverlay() const
{
	const UMKHOnlineServicesSubsystem* Services = UMKHOnlineServicesSubsystem::Get(this);
	return IsValid(Provider) && IsValid(Services) && Services->GetCapabilities().bHasPlatformOverlay;
}

FText UMKHFriendsSubsystem::GetPlatformDisplayName() const
{
	const UMKHOnlineServicesSubsystem* Services = UMKHOnlineServicesSubsystem::Get(this);
	return IsValid(Services) ? Services->GetPlatformDisplayName() : FText::GetEmpty();
}

void UMKHFriendsSubsystem::OpenPlatformFriends()
{
	OpenOverlayPage(EMKHSocialOverlayPage::Friends, FString());
}

void UMKHFriendsSubsystem::OpenPlatformProfile(const FString& NetId)
{
	OpenOverlayPage(EMKHSocialOverlayPage::Profile, NetId);
}

void UMKHFriendsSubsystem::AddFriendOnPlatform(const FString& NetId)
{
	OpenOverlayPage(EMKHSocialOverlayPage::AddFriend, NetId);
}

// ============================================================
// Protected / Internal Logic - Service state
// ============================================================

bool UMKHFriendsSubsystem::IsInLobbyLevel() const
{
	const UMKHGameInstance* GameInstance = Cast<UMKHGameInstance>(GetGameInstance());
	return IsValid(GameInstance) && GameInstance->IsInLobbyLevel();
}

void UMKHFriendsSubsystem::EvaluateLoginState()
{
	const EMKHSocialAvailability Availability = IsValid(Provider) ? Provider->GetAvailability() : EMKHSocialAvailability::Unavailable;
	switch (Availability)
	{
	case EMKHSocialAvailability::Available:
		ActivateService();
		break;
	case EMKHSocialAvailability::NoSocialAccount:
		DeactivateService(EMKHFriendsServiceState::NoSocialAccount);
		break;
	case EMKHSocialAvailability::SignedOut:
		DeactivateService(EMKHFriendsServiceState::SignedOut);
		break;
	default:
		DeactivateService(EMKHFriendsServiceState::Unavailable);
		break;
	}
}

void UMKHFriendsSubsystem::ActivateService()
{
	if (bIsServiceActive)
	{
		return;
	}

	bIsServiceActive = true;
	Provider->StartService();
	RefreshFriends();

	PublishedActivity = FMKHPlayerActivity();
	ActivityTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UMKHFriendsSubsystem::HandleActivityTick), ActivityTickInterval);
	HandleActivityTick(0.f);
}

void UMKHFriendsSubsystem::DeactivateService(const EMKHFriendsServiceState NewState)
{
	FTSTicker::GetCoreTicker().RemoveTicker(ActivityTickHandle);
	ActivityTickHandle.Reset();
	if (bIsServiceActive)
	{
		Provider->StopService();
		bIsServiceActive = false;
	}

	Friends.Reset();
	RecentPlayers.Reset();
	InvitedFriendNetIds.Reset();
	PendingFriendRequestName.Reset();
	if (!LobbyInvites.IsEmpty())
	{
		LobbyInvites.Reset();
		OnLobbyInvitesChanged.Broadcast();
	}
	SetServiceState(NewState);
}

// ============================================================
// Protected / Internal Logic - Friends and presence
// ============================================================

void UMKHFriendsSubsystem::HandleReadFriendsComplete(bool bSuccess, const FString& Error)
{
	if (!bIsServiceActive)
	{
		return;
	}
	if (!bSuccess)
	{
		UE_LOG(LogMKHSocial, Warning, TEXT("UMKHFriendsSubsystem: reading the friend list failed: %s"), *Error);
		Friends.Reset();
		RecentPlayers.Reset();
		SetServiceState(EMKHFriendsServiceState::Error);
		OnFriendsChanged.Broadcast();
		return;
	}

	// A fresh read starts a new invite round: the invites already sent can be sent again.
	InvitedFriendNetIds.Reset();
	ServiceState = EMKHFriendsServiceState::Ready;
	RebuildFriendCache();
}

void UMKHFriendsSubsystem::HandleProviderFriendsChanged()
{
	RequestCacheRebuild();
}

void UMKHFriendsSubsystem::RequestCacheRebuild()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (bCacheRebuildPending || !IsValid(GameInstance))
	{
		return;
	}

	bCacheRebuildPending = true;
	GameInstance->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCacheRebuildPending = false;
		RebuildFriendCache();
	}));
}

void UMKHFriendsSubsystem::RebuildFriendCache()
{
	if (!bIsServiceActive)
	{
		return;
	}

	Friends.Reset();
	Provider->GetCachedFriends(Friends);
	for (FMKHFriendInfo& Info : Friends)
	{
		Info.bInvitedToLobby = InvitedFriendNetIds.Contains(Info.NetId);
	}
	SortFriends(Friends);

	RecentPlayers.Reset();
	if (SupportsRecentPlayers())
	{
		Provider->GetRecentPlayers(RecentPlayers);
	}
	UE_LOG(LogMKHSocial, Verbose, TEXT("UMKHFriendsSubsystem: %d friends and requests, %d recent players."), Friends.Num(), RecentPlayers.Num());
	OnFriendsChanged.Broadcast();
}

void UMKHFriendsSubsystem::SortFriends(TArray<FMKHFriendInfo>& InOutFriends)
{
	// Received requests need an answer, so they come first; sent requests only wait, so they come last. Friends in the
	// player's lobby lead the friends, then the others from the most to the least available.
	auto GetGroupRank = [](const FMKHFriendInfo& Info) -> int32
	{
		switch (Info.Relation)
		{
		case EMKHFriendRelation::RequestReceived: return 0;
		case EMKHFriendRelation::Friend:
			return Info.Presence == EMKHFriendPresence::InYourParty ? 1 : 2 + static_cast<int32>(Info.Presence);
		case EMKHFriendRelation::RequestSent:     return 10;
		default:                                  return 11;
		}
	};

	InOutFriends.Sort([&GetGroupRank](const FMKHFriendInfo& A, const FMKHFriendInfo& B)
	{
		const int32 RankA = GetGroupRank(A);
		const int32 RankB = GetGroupRank(B);
		return RankA != RankB ? RankA < RankB : A.DisplayName.Compare(B.DisplayName, ESearchCase::IgnoreCase) < 0;
	});
}

void UMKHFriendsSubsystem::SetServiceState(const EMKHFriendsServiceState NewState)
{
	if (ServiceState == NewState)
	{
		return;
	}

	ServiceState = NewState;
	OnFriendsChanged.Broadcast();
}

void UMKHFriendsSubsystem::FinishFriendRequest(const EMKHFriendRequestResult Result)
{
	PendingFriendRequestName.Reset();
	OnFriendRequestResult.Broadcast(Result);
	if (Result == EMKHFriendRequestResult::Sent)
	{
		RefreshFriends();
	}
}

// ============================================================
// Protected / Internal Logic - Lobby invites
// ============================================================

void UMKHFriendsSubsystem::HandleLobbyInviteReceived(const FMKHLobbyInvite& Invite)
{
	if (Invite.InviteId.IsEmpty() || LobbyInvites.ContainsByPredicate([&Invite](const FMKHLobbyInvite& Entry) { return Entry.InviteId == Invite.InviteId; }))
	{
		return;
	}

	LobbyInvites.Add(Invite);
	UE_LOG(LogMKHSocial, Log, TEXT("UMKHFriendsSubsystem: lobby invite received from %s."),
		Invite.SenderDisplayName.IsEmpty() ? *Invite.SenderId : *Invite.SenderDisplayName);
	OnLobbyInvitesChanged.Broadcast();
}

void UMKHFriendsSubsystem::HandleInviteResolved(bool bSuccess, const FMKHLobbySearchResult& Lobby, FString InviteId)
{
	// Some backends (EIK) fire this completion twice for one lookup: a second accept would start a second
	// leave/join and make the first one fail, so only a still-pending invite is processed.
	if (!LobbyInvites.ContainsByPredicate([&InviteId](const FMKHLobbyInvite& Invite) { return Invite.InviteId == InviteId; }))
	{
		return;
	}
	RemoveLobbyInvite(InviteId);

	UMKHSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UMKHSessionSubsystem>();
	const bool bHasFreeSlot = Lobby.CurrentPlayers < Lobby.MaxPlayers;
	if (!bSuccess || !Lobby.IsValid() || !bHasFreeSlot || !IsValid(Sessions))
	{
		UE_LOG(LogMKHSocial, Warning, TEXT("UMKHFriendsSubsystem: the invited lobby is gone or full."));
		OnLobbyInviteAcceptResult.Broadcast(false);
		return;
	}

	// Same path as an invite accepted from the platform overlay: leave the current lobby, join this one.
	OnLobbyInviteAcceptResult.Broadcast(true);
	Sessions->JoinInvitedLobby(Lobby);
}

void UMKHFriendsSubsystem::RemoveLobbyInvite(const FString& InviteId)
{
	if (LobbyInvites.RemoveAll([&InviteId](const FMKHLobbyInvite& Invite) { return Invite.InviteId == InviteId; }) > 0)
	{
		OnLobbyInvitesChanged.Broadcast();
	}
}

void UMKHFriendsSubsystem::HandleAvatarChanged(const FString& UserId)
{
	OnFriendAvatarChanged.Broadcast(UserId);
}

// ============================================================
// Protected / Internal Logic - Platform features
// ============================================================

void UMKHFriendsSubsystem::OpenOverlayPage(const EMKHSocialOverlayPage Page, const FString& NetId) const
{
	if (!bIsServiceActive || !HasPlatformOverlay() || !Provider->OpenOverlay(Page, NetId))
	{
		UE_LOG(LogMKHSocial, Warning, TEXT("UMKHFriendsSubsystem: the platform overlay could not be opened."));
	}
}

// ============================================================
// Protected / Internal Logic - Activity published to friends
// ============================================================

bool UMKHFriendsSubsystem::HandleActivityTick(float DeltaTime)
{
	if (!bIsServiceActive)
	{
		return true;
	}

	const FMKHPlayerActivity Activity = BuildLocalActivity(GetGameInstance(), PublishedActivity);
	if (!(Activity == PublishedActivity))
	{
		PublishedActivity = Activity;
		Provider->SetLocalActivity(Activity);
	}
	return true;
}

FMKHPlayerActivity UMKHFriendsSubsystem::BuildLocalActivity(const UGameInstance* InGameInstance, const FMKHPlayerActivity& Previous)
{
	FMKHPlayerActivity Activity;
	const UMKHGameInstance* GameInstance = Cast<UMKHGameInstance>(InGameInstance);
	const UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return Activity;
	}

	if (GameInstance->IsInLobbyLevel())
	{
		const UMKHLobbySubsystem* Lobby = GameInstance->GetSubsystem<UMKHLobbySubsystem>();
		Activity.Activity = EMKHPlayerActivity::InLobby;
		Activity.LobbyPlayers = IsValid(Lobby) ? FMath::Max(Lobby->GetPlayersInLobby(), 1) : 1;
		Activity.LobbyMaxPlayers = IsValid(Lobby) ? FMath::Max(Lobby->GetPlayersToStart(), Activity.LobbyPlayers) : Activity.LobbyPlayers;
		Activity.CareerLevels = ReadCareerLevels(*GameInstance);
		return Activity;
	}

	// Any other level with a match game state is a match; travels and loading screens keep the last activity.
	const AMKHGameState* GameState = World->GetGameState<AMKHGameState>();
	if (!IsValid(GameState))
	{
		return Previous;
	}

	Activity.Activity = GameInstance->IsInOfflineMatch() ? EMKHPlayerActivity::OfflineMatch : EMKHPlayerActivity::OnlineMatch;
	ReadMatchScore(*GameState, Activity);
	Activity.CareerLevels = ReadCareerLevels(*GameInstance);
	return Activity;
}

int32 UMKHFriendsSubsystem::ReadCareerLevels(const UGameInstance& InGameInstance)
{
	const UMKHCareerSubsystem* Career = InGameInstance.GetSubsystem<UMKHCareerSubsystem>();
	return IsValid(Career) ? Career->GetCompletedLevels() : 0;
}

void UMKHFriendsSubsystem::ReadMatchScore(const AMKHGameState& GameState, FMKHPlayerActivity& OutActivity)
{
	OutActivity.Round = GameState.GetRound();
	for (const FMKHDuelPlayerData& Data : GameState.GetPlayerDatas())
	{
		if (!IsValid(Data.Participant))
		{
			continue;
		}
		int32& Score = Data.Participant->IsLocallyControlled() ? OutActivity.OwnScore : OutActivity.OpponentScore;
		Score = Data.Score;
	}
}
