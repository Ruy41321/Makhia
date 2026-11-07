// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Backend/MKHSessionProvider_OSS.h"

#include "Engine/GameInstance.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "MKHLogChannels.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHSessionProvider_OSS::InitializeProvider()
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogMKHOnline, Log, TEXT("Session provider %s: online subsystem '%s' unavailable."), *GetClass()->GetName(),
			*GetOnlineSubsystemName().ToString());
		return;
	}

	BoundSessions = Sessions;
	CreateCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMKHSessionProvider_OSS::HandleCreateSessionComplete));
	UpdateCompleteHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UMKHSessionProvider_OSS::HandleUpdateSessionComplete));
	FindCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMKHSessionProvider_OSS::HandleFindSessionsComplete));
	JoinCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMKHSessionProvider_OSS::HandleJoinSessionComplete));
	DestroyCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMKHSessionProvider_OSS::HandleDestroySessionComplete));
	InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
		FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UMKHSessionProvider_OSS::HandleSessionUserInviteAccepted));
}

void UMKHSessionProvider_OSS::ShutdownProvider()
{
	if (const IOnlineSessionPtr Sessions = BoundSessions.Pin(); Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
		Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateCompleteHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
	}

	BoundSessions.Reset();
	PendingCreates.Empty();
	PendingUpdates.Empty();
	PendingDestroys.Empty();
	PendingJoins.Empty();
	PendingSearch.Reset();
	PendingSearchComplete.Unbind();
}

// ============================================================
// Public Interface
// ============================================================

bool UMKHSessionProvider_OSS::IsBackendAvailable() const
{
	return GetSessionInterface().IsValid();
}

void UMKHSessionProvider_OSS::HostLobby(const FMKHLobbySessionSettings& Settings, const FMKHSessionOperationComplete& OnComplete)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || Settings.SessionName.IsNone() || PendingCreates.Contains(Settings.SessionName))
	{
		UE_LOG(LogMKHOnline, Warning, TEXT("HostLobby('%s') refused: no session interface, no name or already pending."),
			*Settings.SessionName.ToString());
		OnComplete.ExecuteIfBound(false);
		return;
	}

	FOnlineSessionSettings SessionSettings;
	ConfigureSessionSettings(Settings, SessionSettings);
	PendingCreates.FindOrAdd(Settings.SessionName).Add(OnComplete);
	if (!Sessions->CreateSession(LocalUserNum, Settings.SessionName, SessionSettings))
	{
		// Some subsystems also notify a refused call: whichever runs first completes it.
		CompletePending(PendingCreates, Settings.SessionName, false);
	}
}

void UMKHSessionProvider_OSS::UpdateLobby(const FMKHLobbySessionSettings& Settings, const FMKHSessionOperationComplete& OnComplete)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(Settings.SessionName) : nullptr;
	if (!Session || PendingUpdates.Contains(Settings.SessionName))
	{
		UE_LOG(LogMKHOnline, Warning, TEXT("UpdateLobby('%s') refused: no such session or already pending."), *Settings.SessionName.ToString());
		OnComplete.ExecuteIfBound(false);
		return;
	}

	FOnlineSessionSettings SessionSettings = Session->SessionSettings;
	ConfigureSessionSettings(Settings, SessionSettings);
	PendingUpdates.FindOrAdd(Settings.SessionName).Add(OnComplete);
	if (!Sessions->UpdateSession(Settings.SessionName, SessionSettings, true))
	{
		CompletePending(PendingUpdates, Settings.SessionName, false);
	}
}

void UMKHSessionProvider_OSS::SearchLobbies(const FMKHLobbySearchQuery& Query, const FMKHLobbySearchComplete& OnComplete)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || PendingSearch.IsValid())
	{
		UE_LOG(LogMKHOnline, Warning, TEXT("SearchLobbies refused: no session interface or a search is already running."));
		OnComplete.ExecuteIfBound(false, TArray<FMKHLobbySearchResult>());
		return;
	}

	PendingSearch = MakeShared<FOnlineSessionSearch>();
	ConfigureSearch(Query, *PendingSearch);
	PendingSearchQuery = Query;
	PendingSearchComplete = OnComplete;
	if (!Sessions->FindSessions(LocalUserNum, PendingSearch.ToSharedRef()))
	{
		CompleteSearch(false);
	}
}

void UMKHSessionProvider_OSS::JoinLobby(FName LocalSessionName, const FMKHLobbySearchResult& Lobby, const FMKHLobbyJoinComplete& OnComplete)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !Lobby.IsValid() || PendingJoins.Contains(LocalSessionName))
	{
		UE_LOG(LogMKHOnline, Warning, TEXT("JoinLobby('%s') refused: no session interface, invalid lobby or already pending."),
			*LocalSessionName.ToString());
		OnComplete.ExecuteIfBound(EMKHJoinLobbyResult::UnknownError, FString());
		return;
	}

	// Every lobby handed to an OSS provider was produced by an OSS provider (see MakeLobbyResult).
	const FMKHLobbyHandle_OSS& Handle = static_cast<const FMKHLobbyHandle_OSS&>(*Lobby.Handle);
	PendingJoins.Add(LocalSessionName, OnComplete);
	if (!Sessions->JoinSession(LocalUserNum, LocalSessionName, Handle.Result))
	{
		CompleteJoin(LocalSessionName, EMKHJoinLobbyResult::UnknownError, FString());
	}
}

void UMKHSessionProvider_OSS::DestroySession(FName SessionName, const FMKHSessionOperationComplete& OnComplete)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || SessionName.IsNone())
	{
		OnComplete.ExecuteIfBound(false);
		return;
	}

	const bool bAlreadyPending = PendingDestroys.Contains(SessionName);
	PendingDestroys.FindOrAdd(SessionName).Add(OnComplete);
	if (bAlreadyPending)
	{
		return;
	}

	if (!Sessions->DestroySession(SessionName))
	{
		CompletePending(PendingDestroys, SessionName, false);
	}
}

bool UMKHSessionProvider_OSS::SendLobbyInvite(FName SessionName, const FString& FriendId)
{
	const IOnlineSubsystem* Subsystem = GetOnlineSubsystem();
	const IOnlineSessionPtr Sessions = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
	const IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	if (!Sessions.IsValid() || !Identity.IsValid())
	{
		return false;
	}

	const FUniqueNetIdPtr FriendNetId = Identity->CreateUniquePlayerId(FriendId);
	if (!FriendNetId.IsValid())
	{
		UE_LOG(LogMKHOnline, Warning, TEXT("SendLobbyInvite: '%s' is not a valid user id."), *FriendId);
		return false;
	}
	return Sessions->SendSessionInviteToFriend(LocalUserNum, SessionName, *FriendNetId);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSessionProvider_OSS::ConfigureSessionSettings(const FMKHLobbySessionSettings& Settings, FOnlineSessionSettings& OutSettings) const
{
	OutSettings.NumPublicConnections = Settings.MaxPlayers;
	OutSettings.NumPrivateConnections = 0;
	OutSettings.bIsDedicated = false;
	OutSettings.bIsLANMatch = false;
	OutSettings.bShouldAdvertise = Settings.bAdvertise;
	OutSettings.bAllowJoinInProgress = true;
	OutSettings.bAllowInvites = true;
	OutSettings.bUsesPresence = true;
	OutSettings.bUseLobbiesIfAvailable = true;
	OutSettings.bAllowJoinViaPresence = true;

	OutSettings.Set(FMKHLobbyKeys::SessionName, Settings.SessionName.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	for (const TPair<FName, FString>& Attribute : Settings.Attributes)
	{
		OutSettings.Set(Attribute.Key, Attribute.Value, EOnlineDataAdvertisementType::ViaOnlineService);
	}
}

void UMKHSessionProvider_OSS::ConfigureSearch(const FMKHLobbySearchQuery& Query, FOnlineSessionSearch& OutSearch) const
{
	OutSearch.MaxSearchResults = Query.MaxResults;
	OutSearch.bIsLanQuery = false;
	OutSearch.QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	for (const TPair<FName, FString>& Attribute : Query.RequiredAttributes)
	{
		OutSearch.QuerySettings.Set(Attribute.Key, Attribute.Value, EOnlineComparisonOp::Equals);
	}
}

int32 UMKHSessionProvider_OSS::GetOpenSlots(FName SessionName) const
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(SessionName) : nullptr;
	return Session ? Session->NumOpenPublicConnections + Session->NumOpenPrivateConnections : -1;
}

FMKHLobbySearchResult UMKHSessionProvider_OSS::MakeLobbyFromSearchResult(const FOnlineSessionSearchResult& Result)
{
	FMKHLobbySearchResult OutLobby;
	const FOnlineSessionSettings& Settings = Result.Session.SessionSettings;
	OutLobby.LobbyId = Result.IsValid() ? Result.GetSessionIdStr() : FString();
	for (const TPair<FName, FOnlineSessionSetting>& Setting : Settings.Settings)
	{
		OutLobby.Attributes.Add(Setting.Key, Setting.Value.Data.ToString());
	}
	OutLobby.OwnerSessionName = OutLobby.GetAttribute(FMKHLobbyKeys::SessionName);
	OutLobby.MaxPlayers = Settings.NumPublicConnections + Settings.NumPrivateConnections;
	OutLobby.CurrentPlayers = OutLobby.MaxPlayers - (Result.Session.NumOpenPublicConnections + Result.Session.NumOpenPrivateConnections);

	if (Result.IsValid())
	{
		const TSharedRef<FMKHLobbyHandle_OSS> Handle = MakeShared<FMKHLobbyHandle_OSS>();
		Handle->Result = Result;
		OutLobby.Handle = Handle;
	}
	return OutLobby;
}

void UMKHSessionProvider_OSS::ReadSearchResult(const FOnlineSessionSearchResult& Result, FMKHLobbySearchResult& OutLobby) const
{
	OutLobby = MakeLobbyFromSearchResult(Result);
}

IOnlineSubsystem* UMKHSessionProvider_OSS::GetOnlineSubsystem() const
{
	// The game instance world keeps each Play In Editor instance on its own online subsystem.
	const UGameInstance* GameInstance = GetTypedOuter<UGameInstance>();
	const UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	if (bSubsystemStartFailed)
	{
		return nullptr;
	}

	// A subsystem that failed to start (e.g. Steam closed) would retry its whole start-up on every Get and stall the
	// frame: remember the failure for the provider lifetime.
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(World, GetOnlineSubsystemName());
	bSubsystemStartFailed = Subsystem == nullptr;
	return Subsystem;
}

IOnlineSessionPtr UMKHSessionProvider_OSS::GetSessionInterface() const
{
	const IOnlineSubsystem* Subsystem = GetOnlineSubsystem();
	return Subsystem ? Subsystem->GetSessionInterface() : nullptr;
}

void UMKHSessionProvider_OSS::CompletePending(FPendingOperations& Pending, FName SessionName, bool bSuccess)
{
	TArray<FMKHSessionOperationComplete> Completions;
	if (!Pending.RemoveAndCopyValue(SessionName, Completions))
	{
		return;
	}

	for (const FMKHSessionOperationComplete& Completion : Completions)
	{
		Completion.ExecuteIfBound(bSuccess);
	}
}

void UMKHSessionProvider_OSS::CompleteJoin(FName SessionName, EMKHJoinLobbyResult Result, const FString& ConnectString)
{
	FMKHLobbyJoinComplete Completion;
	if (PendingJoins.RemoveAndCopyValue(SessionName, Completion))
	{
		Completion.ExecuteIfBound(Result, ConnectString);
	}
}

void UMKHSessionProvider_OSS::CompleteSearch(bool bSuccess)
{
	if (!PendingSearch.IsValid())
	{
		return;
	}

	TArray<FMKHLobbySearchResult> Lobbies;
	if (bSuccess)
	{
		for (const FOnlineSessionSearchResult& Result : PendingSearch->SearchResults)
		{
			FMKHLobbySearchResult Lobby = MakeLobbyResult(Result);
			if (MatchesQuery(Lobby, PendingSearchQuery))
			{
				Lobbies.Add(MoveTemp(Lobby));
			}
		}
	}

	// Cleared before running the completion, which may start the next search.
	const FMKHLobbySearchComplete Completion = PendingSearchComplete;
	PendingSearch.Reset();
	PendingSearchComplete.Unbind();
	Completion.ExecuteIfBound(bSuccess, Lobbies);
}

FMKHLobbySearchResult UMKHSessionProvider_OSS::MakeLobbyResult(const FOnlineSessionSearchResult& Result) const
{
	FMKHLobbySearchResult Lobby;
	ReadSearchResult(Result, Lobby);
	return Lobby;
}

bool UMKHSessionProvider_OSS::MatchesQuery(const FMKHLobbySearchResult& Lobby, const FMKHLobbySearchQuery& Query)
{
	for (const TPair<FName, FString>& Required : Query.RequiredAttributes)
	{
		const FString* Value = Lobby.Attributes.Find(Required.Key);
		if (!Value || *Value != Required.Value)
		{
			return false;
		}
	}
	return true;
}

EMKHJoinLobbyResult UMKHSessionProvider_OSS::ToJoinLobbyResult(EOnJoinSessionCompleteResult::Type Result)
{
	switch (Result)
	{
	case EOnJoinSessionCompleteResult::Success:
		return EMKHJoinLobbyResult::Success;
	case EOnJoinSessionCompleteResult::SessionIsFull:
		return EMKHJoinLobbyResult::SessionIsFull;
	case EOnJoinSessionCompleteResult::SessionDoesNotExist:
		return EMKHJoinLobbyResult::SessionDoesNotExist;
	case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
		return EMKHJoinLobbyResult::CouldNotRetrieveAddress;
	case EOnJoinSessionCompleteResult::AlreadyInSession:
		return EMKHJoinLobbyResult::AlreadyInSession;
	default:
		return EMKHJoinLobbyResult::UnknownError;
	}
}

void UMKHSessionProvider_OSS::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	CompletePending(PendingCreates, SessionName, bWasSuccessful);
}

void UMKHSessionProvider_OSS::HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	CompletePending(PendingUpdates, SessionName, bWasSuccessful);
}

void UMKHSessionProvider_OSS::HandleFindSessionsComplete(bool bWasSuccessful)
{
	CompleteSearch(bWasSuccessful);
}

void UMKHSessionProvider_OSS::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!PendingJoins.Contains(SessionName))
	{
		return;
	}

	EMKHJoinLobbyResult JoinResult = ToJoinLobbyResult(Result);
	FString ConnectString;
	if (JoinResult == EMKHJoinLobbyResult::Success)
	{
		const IOnlineSessionPtr Sessions = GetSessionInterface();
		if (!Sessions.IsValid() || !Sessions->GetResolvedConnectString(SessionName, ConnectString) || ConnectString.IsEmpty())
		{
			JoinResult = EMKHJoinLobbyResult::CouldNotRetrieveAddress;
		}
	}
	CompleteJoin(SessionName, JoinResult, ConnectString);
}

void UMKHSessionProvider_OSS::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	CompletePending(PendingDestroys, SessionName, bWasSuccessful);
}

void UMKHSessionProvider_OSS::HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId,
	const FOnlineSessionSearchResult& InviteResult)
{
	const FMKHLobbySearchResult Lobby = MakeLobbyResult(InviteResult);
	UE_LOG(LogMKHOnline, Log, TEXT("Lobby invite accepted outside the game (success: %d, lobby %s)."), bWasSuccessful ? 1 : 0, *Lobby.LobbyId);
	OnLobbyInviteAccepted.Broadcast(bWasSuccessful && Lobby.IsValid(), Lobby);
}
