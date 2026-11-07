// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "MKHSocialProvider_Steam.h"

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "MKHLogChannels.h"
#include "MKHSteamShared.h"
#include "MKHSteamSocialCallbacks.h"
#include "Online/Auth/MKHAuthSubsystem.h"
#include "Online/Backend/MKHSessionProvider_OSS.h"
#include "OnlineSubsystem.h"
#include "TimerManager.h"

namespace MKHSocialSteam
{
	/** Rich presence key marking a player as in Makhia: AppID 480 is shared, "playing this game" is not enough. */
	static const char* InMakhiaKey = "mkh";

	/** Value of InMakhiaKey while the player is in Makhia. */
	static const char* InMakhiaValue = "1";

	/** Rich presence keys of the activity published to friends (FMKHPlayerActivity). */
	static const char* ActivityKey = "mkh_act";
	static const char* LobbyKey = "mkh_lobby";
	static const char* RoundKey = "mkh_round";
	static const char* ScoreKey = "mkh_score";
	static const char* CareerKey = "mkh_career";

	/** Values of ActivityKey, by EMKHPlayerActivity (None deletes the key). */
	static const char* ActivityValues[] = { "", "lobby", "offline", "online" };

	/** Friend flags of the friend list: friends, requests received and requests sent. */
	static constexpr int32 FriendListFlags = k_EFriendFlagImmediate | k_EFriendFlagFriendshipRequested | k_EFriendFlagRequestingFriendship;

	/** Recent players listed at most. */
	static constexpr int32 MaxRecentPlayers = 10;

	/** FindFriendSession attempts for one invite (a running lobby search makes it fail at once). */
	static constexpr int32 MaxInviteLookupAttempts = 4;

	/** Seconds between two FindFriendSession attempts. */
	static constexpr float InviteLookupRetryDelay = 1.f;

	/** Returns the AppID of the running game, 0 without Steam. */
	static AppId_t GetAppId()
	{
		return SteamUtils() ? SteamUtils()->GetAppID() : 0;
	}

	/** Returns the name the local player sees for a user: their Steam nickname when set, else their persona name. */
	static FString GetDisplayName(ISteamFriends& Friends, const CSteamID SteamId)
	{
		const char* Nickname = Friends.GetPlayerNickname(SteamId);
		return UTF8_TO_TCHAR(Nickname && *Nickname ? Nickname : Friends.GetFriendPersonaName(SteamId));
	}

	/** Returns true when the local player is a member of the given lobby (Steam only lists members of joined lobbies). */
	static bool IsLocalPlayerInLobby(const CSteamID LobbyId)
	{
		ISteamMatchmaking* Matchmaking = SteamMatchmaking();
		if (!Matchmaking || !SteamUser() || !LobbyId.IsValid())
		{
			return false;
		}

		const CSteamID LocalId = SteamUser()->GetSteamID();
		const int32 MemberCount = Matchmaking->GetNumLobbyMembers(LobbyId);
		for (int32 Index = 0; Index < MemberCount; ++Index)
		{
			if (Matchmaking->GetLobbyMemberByIndex(LobbyId, Index) == LocalId)
			{
				return true;
			}
		}
		return false;
	}

	/** Publishes an activity in the rich presence of the local player (None deletes the keys). */
	static void WriteActivity(ISteamFriends& Friends, const FMKHPlayerActivity& Activity)
	{
		const bool bInLobby = Activity.Activity == EMKHPlayerActivity::InLobby;
		const bool bInMatch = Activity.Activity == EMKHPlayerActivity::OfflineMatch || Activity.Activity == EMKHPlayerActivity::OnlineMatch;
		const FString Lobby = bInLobby ? FString::Printf(TEXT("%d/%d"), Activity.LobbyPlayers, Activity.LobbyMaxPlayers) : FString();
		const FString Round = bInMatch && Activity.Round > 0 ? LexToString(Activity.Round) : FString();
		const FString Score = bInMatch && Activity.Round > 0 ? FString::Printf(TEXT("%d-%d"), Activity.OwnScore, Activity.OpponentScore) : FString();

		Friends.SetRichPresence(ActivityKey, ActivityValues[static_cast<uint8>(Activity.Activity)]);
		Friends.SetRichPresence(LobbyKey, TCHAR_TO_UTF8(*Lobby));
		Friends.SetRichPresence(RoundKey, TCHAR_TO_UTF8(*Round));
		Friends.SetRichPresence(ScoreKey, TCHAR_TO_UTF8(*Score));

		const FString Career = Activity.Activity != EMKHPlayerActivity::None ? LexToString(Activity.CareerLevels) : FString();
		Friends.SetRichPresence(CareerKey, TCHAR_TO_UTF8(*Career));
	}

	/** Reads the activity a friend published (keys missing or malformed read as 0). */
	static FMKHPlayerActivity ReadFriendActivity(ISteamFriends& Friends, const CSteamID FriendId)
	{
		FMKHPlayerActivity Activity;
		const FString Value = UTF8_TO_TCHAR(Friends.GetFriendRichPresence(FriendId, ActivityKey));
		for (uint8 Index = 1; Index < UE_ARRAY_COUNT(ActivityValues); ++Index)
		{
			if (Value == UTF8_TO_TCHAR(ActivityValues[Index]))
			{
				Activity.Activity = static_cast<EMKHPlayerActivity>(Index);
			}
		}

		// Values written by another build may be missing or malformed: they simply read as 0.
		FString Left;
		FString Right;
		if (FString(UTF8_TO_TCHAR(Friends.GetFriendRichPresence(FriendId, LobbyKey))).Split(TEXT("/"), &Left, &Right))
		{
			Activity.LobbyPlayers = FCString::Atoi(*Left);
			Activity.LobbyMaxPlayers = FCString::Atoi(*Right);
		}
		Activity.Round = FCString::Atoi(UTF8_TO_TCHAR(Friends.GetFriendRichPresence(FriendId, RoundKey)));
		if (FString(UTF8_TO_TCHAR(Friends.GetFriendRichPresence(FriendId, ScoreKey))).Split(TEXT("-"), &Left, &Right))
		{
			Activity.OwnScore = FCString::Atoi(*Left);
			Activity.OpponentScore = FCString::Atoi(*Right);
		}
		Activity.CareerLevels = FCString::Atoi(UTF8_TO_TCHAR(Friends.GetFriendRichPresence(FriendId, CareerKey)));
		return Activity;
	}

	/** Maps the Steam presence of a friend to the UI presence states. */
	static EMKHFriendPresence GetPresence(ISteamFriends& Friends, const CSteamID SteamId)
	{
		const EPersonaState State = Friends.GetFriendPersonaState(SteamId);
		if (State == k_EPersonaStateOffline)
		{
			return EMKHFriendPresence::Offline;
		}

		FriendGameInfo_t GameInfo;
		const bool bInThisApp = Friends.GetFriendGamePlayed(SteamId, &GameInfo) && GameInfo.m_gameID.AppID() == GetAppId();
		if (bInThisApp && FCStringAnsi::Strcmp(Friends.GetFriendRichPresence(SteamId, InMakhiaKey), InMakhiaValue) == 0)
		{
			return IsLocalPlayerInLobby(GameInfo.m_steamIDLobby) ? EMKHFriendPresence::InYourParty : EMKHFriendPresence::InMakhia;
		}
		return State == k_EPersonaStateAway || State == k_EPersonaStateSnooze ? EMKHFriendPresence::Away : EMKHFriendPresence::Online;
	}
}

// ============================================================
// FMKHSteamSocialCallbacks
// ============================================================

FMKHSteamSocialCallbacks::FMKHSteamSocialCallbacks(UMKHSocialProvider_Steam* InOwner)
	: Owner(InOwner)
{
}

void FMKHSteamSocialCallbacks::RunOnGameThread(TUniqueFunction<void(UMKHSocialProvider_Steam&)>&& Handler) const
{
	AsyncTask(ENamedThreads::GameThread, [WeakOwner = Owner, Handler = MoveTemp(Handler)]()
	{
		if (UMKHSocialProvider_Steam* Provider = WeakOwner.Get())
		{
			Handler(*Provider);
		}
	});
}

void FMKHSteamSocialCallbacks::OnPersonaStateChange(PersonaStateChange_t* Data)
{
	const uint64 SteamId = Data->m_ulSteamID;
	const int32 Flags = Data->m_nChangeFlags;
	RunOnGameThread([SteamId, Flags](UMKHSocialProvider_Steam& Provider) { Provider.HandlePersonaStateChanged(SteamId, Flags); });
}

void FMKHSteamSocialCallbacks::OnFriendRichPresenceUpdate(FriendRichPresenceUpdate_t* Data)
{
	const uint64 SteamId = Data->m_steamIDFriend.ConvertToUint64();
	RunOnGameThread([SteamId](UMKHSocialProvider_Steam& Provider) { Provider.HandleRichPresenceChanged(SteamId); });
}

void FMKHSteamSocialCallbacks::OnAvatarImageLoaded(AvatarImageLoaded_t* Data)
{
	const uint64 SteamId = Data->m_steamID.ConvertToUint64();
	RunOnGameThread([SteamId](UMKHSocialProvider_Steam& Provider) { Provider.HandleAvatarLoaded(SteamId); });
}

void FMKHSteamSocialCallbacks::OnLobbyChatUpdate(LobbyChatUpdate_t* Data)
{
	// Someone entered or left a lobby the local player is in: "In your party" may change.
	RunOnGameThread([](UMKHSocialProvider_Steam& Provider) { Provider.HandleLobbyMembersChanged(); });
}

void FMKHSteamSocialCallbacks::OnLobbyInvite(LobbyInvite_t* Data)
{
	// Invites to lobbies of other AppID 480 games are not for Makhia.
	if (CGameID(Data->m_ulGameID).AppID() != MKHSocialSteam::GetAppId())
	{
		return;
	}

	const uint64 SenderId = Data->m_ulSteamIDUser;
	const uint64 LobbyId = Data->m_ulSteamIDLobby;
	RunOnGameThread([SenderId, LobbyId](UMKHSocialProvider_Steam& Provider) { Provider.HandleLobbyInviteReceived(SenderId, LobbyId); });
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHSocialProvider_Steam::InitializeProvider()
{
	if (MKHSteam::GetSubsystem() && SteamFriends())
	{
		Callbacks = MakeShared<FMKHSteamSocialCallbacks>(this);
	}

	const UGameInstance* GameInstance = GetTypedOuter<UGameInstance>();
	if (UMKHAuthSubsystem* Auth = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHAuthSubsystem>() : nullptr; IsValid(Auth))
	{
		AuthStateHandle = Auth->OnAuthStateChangedNative.AddUObject(this, &UMKHSocialProvider_Steam::HandleAuthStateChanged);
	}
}

void UMKHSocialProvider_Steam::ShutdownProvider()
{
	StopService();
	FinishInviteLookup(false, FMKHLobbySearchResult());

	const UGameInstance* GameInstance = GetTypedOuter<UGameInstance>();
	if (UMKHAuthSubsystem* Auth = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHAuthSubsystem>() : nullptr; IsValid(Auth))
	{
		Auth->OnAuthStateChangedNative.Remove(AuthStateHandle);
	}
	AuthStateHandle.Reset();

	Callbacks.Reset();
	Avatars.Empty();
	PendingAvatarRequests.Empty();
}

// ============================================================
// Public Interface
// ============================================================

EMKHSocialAvailability UMKHSocialProvider_Steam::GetAvailability() const
{
	if (!Callbacks.IsValid())
	{
		return EMKHSocialAvailability::Unavailable;
	}

	const UGameInstance* GameInstance = GetTypedOuter<UGameInstance>();
	const UMKHAuthSubsystem* Auth = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHAuthSubsystem>() : nullptr;
	return IsValid(Auth) && Auth->IsLoggedIn() ? EMKHSocialAvailability::Available : EMKHSocialAvailability::SignedOut;
}

void UMKHSocialProvider_Steam::StartService()
{
	if (bIsServiceRunning || !Callbacks.IsValid())
	{
		return;
	}

	bIsServiceRunning = true;
	SetInMakhiaPresence(true);
	UE_LOG(LogMKHSocial, Log, TEXT("Steam social: service started (%d friends and requests)."), SteamFriends()->GetFriendCount(MKHSocialSteam::FriendListFlags));
}

void UMKHSocialProvider_Steam::StopService()
{
	if (!bIsServiceRunning)
	{
		return;
	}

	bIsServiceRunning = false;
	SetInMakhiaPresence(false);
}

void UMKHSocialProvider_Steam::ReadFriends(const FMKHFriendsReadComplete& OnComplete)
{
	const bool bReady = bIsServiceRunning && SteamFriends();
	OnComplete.ExecuteIfBound(bReady, bReady ? FString() : TEXT("The Steam friends service is not running."));
}

void UMKHSocialProvider_Steam::GetCachedFriends(TArray<FMKHFriendInfo>& OutFriends) const
{
	ISteamFriends* Friends = SteamFriends();
	if (!bIsServiceRunning || !Friends)
	{
		return;
	}

	const int32 Count = Friends->GetFriendCount(MKHSocialSteam::FriendListFlags);
	OutFriends.Reserve(OutFriends.Num() + Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const CSteamID SteamId = Friends->GetFriendByIndex(Index, MKHSocialSteam::FriendListFlags);
		FMKHFriendInfo Info;
		switch (Friends->GetFriendRelationship(SteamId))
		{
		case k_EFriendRelationshipFriend:           Info.Relation = EMKHFriendRelation::Friend; break;
		case k_EFriendRelationshipRequestRecipient: Info.Relation = EMKHFriendRelation::RequestReceived; break;
		case k_EFriendRelationshipRequestInitiator: Info.Relation = EMKHFriendRelation::RequestSent; break;
		default: continue;
		}

		Info.NetId = LexToString(SteamId.ConvertToUint64());
		Info.DisplayName = MKHSocialSteam::GetDisplayName(*Friends, SteamId);
		Info.Presence = Info.Relation == EMKHFriendRelation::Friend ? MKHSocialSteam::GetPresence(*Friends, SteamId) : EMKHFriendPresence::Offline;
		// Any Steam friend can receive a Steam lobby invite; the UI still offers it only to friends in Makhia.
		Info.bHasPlayedMakhia = true;
		const bool bInGame = Info.Presence == EMKHFriendPresence::InMakhia || Info.Presence == EMKHFriendPresence::InYourParty;
		Info.Activity = bInGame ? MKHSocialSteam::ReadFriendActivity(*Friends, SteamId) : FMKHPlayerActivity();
		OutFriends.Add(MoveTemp(Info));
	}
}

void UMKHSocialProvider_Steam::ResolveLobbyInvite(const FMKHLobbyInvite& Invite, const FMKHLobbyInviteResolved& OnResolved)
{
	if (!PendingInvite.InviteId.IsEmpty() || !bIsServiceRunning)
	{
		UE_LOG(LogMKHSocial, Warning, TEXT("Steam social: an invite is already being resolved, or the service is off."));
		OnResolved.ExecuteIfBound(false, FMKHLobbySearchResult());
		return;
	}

	PendingInvite = Invite;
	PendingInviteResolved = OnResolved;
	InviteLookupAttempts = 0;
	StartInviteLookup();
}

UTexture2D* UMKHSocialProvider_Steam::GetAvatar(const FString& UserId)
{
	if (const TObjectPtr<UTexture2D>* Found = Avatars.Find(UserId))
	{
		return *Found;
	}

	ISteamFriends* Friends = SteamFriends();
	const uint64 SteamId = MKHSteam::ParseSteamId64(UserId);
	if (!Callbacks.IsValid() || !Friends || SteamId == 0)
	{
		return nullptr;
	}

	// -1: downloading (AvatarImageLoaded_t follows); 0: not known yet for a non-friend (persona data needed first).
	const int32 ImageHandle = Friends->GetMediumFriendAvatar(CSteamID(SteamId));
	if (ImageHandle <= 0)
	{
		bool bAlreadyRequested = false;
		PendingAvatarRequests.Add(SteamId, &bAlreadyRequested);
		if (ImageHandle == 0 && !bAlreadyRequested)
		{
			Friends->RequestUserInformation(CSteamID(SteamId), false);
		}
		return nullptr;
	}

	PendingAvatarRequests.Remove(SteamId);
	UTexture2D* Texture = CreateAvatarTexture(ImageHandle);
	if (IsValid(Texture))
	{
		Avatars.Add(UserId, Texture);
	}
	return Texture;
}

void UMKHSocialProvider_Steam::GetRecentPlayers(TArray<FMKHFriendInfo>& OutPlayers) const
{
	ISteamFriends* Friends = SteamFriends();
	if (!bIsServiceRunning || !Friends)
	{
		return;
	}

	const AppId_t AppId = MKHSocialSteam::GetAppId();
	const int32 Count = Friends->GetCoplayFriendCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const CSteamID SteamId = Friends->GetCoplayFriend(Index);
		// Friends and pending requests are already in the friend list.
		if (Friends->GetFriendCoplayGame(SteamId) != AppId || Friends->GetFriendRelationship(SteamId) != k_EFriendRelationshipNone)
		{
			continue;
		}

		// Name and avatar of a stranger come with their persona data (PersonaStateChange_t when it arrives).
		Friends->RequestUserInformation(SteamId, false);
		FMKHFriendInfo Info;
		Info.NetId = LexToString(SteamId.ConvertToUint64());
		Info.DisplayName = MKHSocialSteam::GetDisplayName(*Friends, SteamId);
		Info.Relation = EMKHFriendRelation::RecentPlayer;
		Info.LastPlayedTogether = FDateTime::FromUnixTimestamp(Friends->GetFriendCoplayTime(SteamId));
		OutPlayers.Add(MoveTemp(Info));
	}

	OutPlayers.Sort([](const FMKHFriendInfo& A, const FMKHFriendInfo& B) { return A.LastPlayedTogether > B.LastPlayedTogether; });
	if (OutPlayers.Num() > MKHSocialSteam::MaxRecentPlayers)
	{
		OutPlayers.SetNum(MKHSocialSteam::MaxRecentPlayers);
	}
}

void UMKHSocialProvider_Steam::ReportPlayedWith(const TArray<FString>& UserIds)
{
	ISteamFriends* Friends = SteamFriends();
	if (!Callbacks.IsValid() || !Friends)
	{
		return;
	}

	for (const FString& UserId : UserIds)
	{
		if (const uint64 SteamId = MKHSteam::ParseSteamId64(UserId); SteamId != 0)
		{
			Friends->SetPlayedWith(CSteamID(SteamId));
		}
	}
}

bool UMKHSocialProvider_Steam::OpenOverlay(EMKHSocialOverlayPage Page, const FString& UserId)
{
	ISteamFriends* Friends = SteamFriends();
	if (!Callbacks.IsValid() || !Friends)
	{
		return false;
	}
	UE_CLOG(SteamUtils() && !SteamUtils()->IsOverlayEnabled(), LogMKHSocial, Warning,
		TEXT("Steam social: the Steam overlay is disabled (Steam settings, or the game was not started with Steam running)."));

	if (Page == EMKHSocialOverlayPage::Friends)
	{
		Friends->ActivateGameOverlay("Friends");
		return true;
	}

	const uint64 SteamId = MKHSteam::ParseSteamId64(UserId);
	if (SteamId == 0)
	{
		return false;
	}
	Friends->ActivateGameOverlayToUser(Page == EMKHSocialOverlayPage::AddFriend ? "friendadd" : "steamid", CSteamID(SteamId));
	return true;
}

bool UMKHSocialProvider_Steam::IsBlocked(const FString& UserId) const
{
	ISteamFriends* Friends = SteamFriends();
	const uint64 SteamId = MKHSteam::ParseSteamId64(UserId);
	return Callbacks.IsValid() && Friends && SteamId != 0 && Friends->HasFriend(CSteamID(SteamId), k_EFriendFlagBlocked);
}

// ============================================================
// Protected / Internal Logic - Notifications
// ============================================================

void UMKHSocialProvider_Steam::HandleAuthStateChanged(EMKHAuthState NewState, EMKHAuthState OldState)
{
	OnLoginStateChanged.Broadcast();
}

void UMKHSocialProvider_Steam::HandlePersonaStateChanged(uint64 SteamId, int32 ChangeFlags)
{
	if ((ChangeFlags & k_EPersonaChangeAvatar) != 0)
	{
		InvalidateAvatar(SteamId);
	}
	if (bIsServiceRunning)
	{
		OnFriendsChanged.Broadcast();
	}
}

void UMKHSocialProvider_Steam::HandleRichPresenceChanged(uint64 SteamId)
{
	if (bIsServiceRunning)
	{
		OnFriendsChanged.Broadcast();
	}
}

void UMKHSocialProvider_Steam::HandleLobbyMembersChanged()
{
	if (bIsServiceRunning)
	{
		OnFriendsChanged.Broadcast();
	}
}

void UMKHSocialProvider_Steam::HandleAvatarLoaded(uint64 SteamId)
{
	InvalidateAvatar(SteamId);
}

void UMKHSocialProvider_Steam::HandleLobbyInviteReceived(uint64 SenderId, uint64 LobbyId)
{
	ISteamFriends* Friends = SteamFriends();
	if (!bIsServiceRunning || !Friends)
	{
		return;
	}

	FMKHLobbyInvite Invite;
	Invite.InviteId = LexToString(LobbyId);
	Invite.SenderId = LexToString(SenderId);
	Invite.SenderDisplayName = MKHSocialSteam::GetDisplayName(*Friends, CSteamID(SenderId));
	OnLobbyInviteReceived.Broadcast(Invite);
}

void UMKHSocialProvider_Steam::InvalidateAvatar(uint64 SteamId)
{
	const FString UserId = LexToString(SteamId);
	const bool bWasKnown = Avatars.Remove(UserId) > 0 || PendingAvatarRequests.Remove(SteamId) > 0;
	if (bWasKnown)
	{
		OnAvatarChanged.Broadcast(UserId);
	}
}

// ============================================================
// Protected / Internal Logic - Invites, avatars, presence
// ============================================================

void UMKHSocialProvider_Steam::StartInviteLookup()
{
	const IOnlineSubsystem* Subsystem = MKHSteam::GetSubsystem();
	const IOnlineSessionPtr Sessions = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
	const IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	const FUniqueNetIdPtr SenderId = Identity.IsValid() ? Identity->CreateUniquePlayerId(PendingInvite.SenderId) : nullptr;
	if (!Sessions.IsValid() || !SenderId.IsValid())
	{
		FinishInviteLookup(false, FMKHLobbySearchResult());
		return;
	}

	++InviteLookupAttempts;
	if (!FindFriendSessionHandle.IsValid())
	{
		FindFriendSessionHandle = Sessions->AddOnFindFriendSessionCompleteDelegate_Handle(MKHSteam::LocalUserNum,
			FOnFindFriendSessionCompleteDelegate::CreateUObject(this, &UMKHSocialProvider_Steam::HandleFindFriendSessionComplete));
	}

	// A refused lookup (e.g. a matchmaking search is running) reports its failure through the same notification.
	Sessions->FindFriendSession(MKHSteam::LocalUserNum, *SenderId);
}

void UMKHSocialProvider_Steam::HandleFindFriendSessionComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& Results)
{
	if (PendingInvite.InviteId.IsEmpty())
	{
		return;
	}

	const FOnlineSessionSearchResult* Found = Results.FindByPredicate([this](const FOnlineSessionSearchResult& Result)
	{
		return Result.IsValid() && Result.GetSessionIdStr() == PendingInvite.InviteId;
	});
	if (bWasSuccessful && Found)
	{
		FinishInviteLookup(true, UMKHSessionProvider_OSS::MakeLobbyFromSearchResult(*Found));
		return;
	}

	UE_LOG(LogMKHSocial, Log, TEXT("Steam social: lobby %s of the invite not found (attempt %d)."), *PendingInvite.InviteId, InviteLookupAttempts);
	const UGameInstance* GameInstance = GetTypedOuter<UGameInstance>();
	if (InviteLookupAttempts >= MKHSocialSteam::MaxInviteLookupAttempts || !IsValid(GameInstance))
	{
		FinishInviteLookup(false, FMKHLobbySearchResult());
		return;
	}

	FTimerHandle RetryHandle;
	GameInstance->GetTimerManager().SetTimer(RetryHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (!PendingInvite.InviteId.IsEmpty())
		{
			StartInviteLookup();
		}
	}), MKHSocialSteam::InviteLookupRetryDelay, false);
}

void UMKHSocialProvider_Steam::FinishInviteLookup(bool bSuccess, const FMKHLobbySearchResult& Lobby)
{
	const IOnlineSubsystem* Subsystem = MKHSteam::GetSubsystem();
	if (const IOnlineSessionPtr Sessions = Subsystem ? Subsystem->GetSessionInterface() : nullptr; Sessions.IsValid())
	{
		Sessions->ClearOnFindFriendSessionCompleteDelegate_Handle(MKHSteam::LocalUserNum, FindFriendSessionHandle);
	}
	FindFriendSessionHandle.Reset();

	// Cleared before running the completion, which may resolve another invite.
	const FMKHLobbyInviteResolved Completion = PendingInviteResolved;
	PendingInvite = FMKHLobbyInvite();
	PendingInviteResolved.Unbind();
	InviteLookupAttempts = 0;
	Completion.ExecuteIfBound(bSuccess, Lobby);
}

UTexture2D* UMKHSocialProvider_Steam::CreateAvatarTexture(int32 ImageHandle)
{
	ISteamUtils* Utils = SteamUtils();
	uint32 Width = 0;
	uint32 Height = 0;
	if (!Utils || !Utils->GetImageSize(ImageHandle, &Width, &Height) || Width == 0 || Height == 0)
	{
		return nullptr;
	}

	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(Width * Height * 4);
	if (!Utils->GetImageRGBA(ImageHandle, Pixels.GetData(), Pixels.Num()))
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8);
	if (!IsValid(Texture) || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
	{
		return nullptr;
	}

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Pixels.GetData(), Pixels.Num());
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

void UMKHSocialProvider_Steam::SetInMakhiaPresence(bool bInMakhia)
{
	if (ISteamFriends* Friends = SteamFriends())
	{
		// An empty value deletes the key.
		Friends->SetRichPresence(MKHSocialSteam::InMakhiaKey, bInMakhia ? MKHSocialSteam::InMakhiaValue : "");
		if (!bInMakhia)
		{
			MKHSocialSteam::WriteActivity(*Friends, FMKHPlayerActivity());
		}
	}
}

void UMKHSocialProvider_Steam::SetLocalActivity(const FMKHPlayerActivity& Activity)
{
	if (ISteamFriends* Friends = SteamFriends(); Friends && bIsServiceRunning)
	{
		MKHSocialSteam::WriteActivity(*Friends, Activity);
		UE_LOG(LogMKHSocial, Log, TEXT("Steam social: activity %s, lobby %d/%d, round %d, score %d-%d."),
			*UEnum::GetValueAsString(Activity.Activity), Activity.LobbyPlayers, Activity.LobbyMaxPlayers, Activity.Round,
			Activity.OwnScore, Activity.OpponentScore);
	}
}
