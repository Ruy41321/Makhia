// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/MKHSessionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameInstance/MKHGameInstance.h"
#include "Lobby/MKHLobbySettings.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "MKHLogChannels.h"
#include "Online/Backend/MKHOnlineBackend.h"
#include "Online/Backend/MKHOnlineServicesSubsystem.h"
#include "Online/Backend/MKHSessionProvider.h"
#include "TimerManager.h"
#include "UI/Loading/MKHLoadingScreenSubsystem.h"
#include "UI/Loading/MKHTravelError.h"

namespace MKHSession
{
	/** Players a lobby holds, host included (duels are 1v1). */
	static constexpr int32 LobbySize = 2;

	/** Maximum lobbies returned by a matchmaking search. */
	static constexpr int32 MaxSearchResults = 10;

	/** Value of the MatchmakingTime attribute while the lobby is not searching. */
	static const TCHAR* NotSearchingTime = TEXT("No");

	/** Failed searches in a row retried silently before the matchmaking reports the error and stops. */
	static constexpr int32 MaxConsecutiveSearchFailures = 3;
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UMKHOnlineServicesSubsystem* Services = Collection.InitializeDependency<UMKHOnlineServicesSubsystem>();
	const UMKHOnlineBackend* Backend = IsValid(Services) ? Services->GetBackend() : nullptr;
	const TSubclassOf<UMKHSessionProvider> ProviderClass = IsValid(Backend) ? Backend->GetSessionProviderClass() : nullptr;
	if (!IsValid(ProviderClass) || ProviderClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogMKHSession, Warning, TEXT("No session provider for the online backend: online lobbies unavailable."));
		return;
	}

	Provider = NewObject<UMKHSessionProvider>(this, ProviderClass);
	Provider->OnLobbyInviteAccepted.AddUObject(this, &UMKHSessionSubsystem::HandleLobbyInviteAccepted);
	Provider->InitializeProvider();
	UE_LOG(LogMKHSession, Log, TEXT("Session provider %s ready (available: %d)."), *ProviderClass->GetName(),
		Provider->IsBackendAvailable() ? 1 : 0);
}

void UMKHSessionSubsystem::Deinitialize()
{
	if (const UGameInstance* GameInstance = GetGameInstance(); IsValid(GameInstance) && IsValid(GameInstance->GetWorld()))
	{
		GameInstance->GetWorld()->GetTimerManager().ClearTimer(SearchAgainHandle);
	}

	if (IsValid(Provider))
	{
		Provider->OnLobbyInviteAccepted.RemoveAll(this);
		Provider->ShutdownProvider();
	}
	Provider = nullptr;
	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHSessionSubsystem* UMKHSessionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHSessionSubsystem>() : nullptr;
}

bool UMKHSessionSubsystem::IsSessionBackendAvailable() const
{
	return IsValid(Provider) && Provider->IsBackendAvailable();
}

void UMKHSessionSubsystem::HandleLobbyLevelStarted()
{
	// The previous lobby level took its timers with it: start from a clean matchmaking state.
	bHasCancelled = false;
	bIsJoiningLobby = false;
	bIsJoiningInvite = false;
	ConsecutiveSearchFailures = 0;
	JoinTarget = FMKHLobbySearchResult();

	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	if (IsValid(GameInstance) && GameInstance->GetIsListenServer() && !GameInstance->GetIsInSession())
	{
		HostLobby(true);
		return;
	}
	ScheduleDeferredInvite();
}

void UMKHSessionSubsystem::HostLobby(bool bShouldNotify)
{
	bShouldNotifyLobby = bShouldNotify;
	if (IsInSession())
	{
		UE_LOG(LogMKHSession, Warning, TEXT("HostLobby: already in a session, nothing to create."));
		return;
	}

	BroadcastEvent(EMKHSessionEvent::CreateLobbyStart);
	if (!IsValid(Provider))
	{
		BroadcastEvent(EMKHSessionEvent::CreateLobbyFail);
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("HostLobby: creating lobby '%s'."), *GetCurrentSessionName().ToString());
	Provider->HostLobby(MakeLobbySettings(false), FMKHSessionOperationComplete::CreateUObject(this, &UMKHSessionSubsystem::HandleHostLobbyComplete));
}

void UMKHSessionSubsystem::AbandonLobby()
{
	UMKHLobbySubsystem* Lobby = GetLobby();
	const bool bAloneAsHost = IsHost() && (!IsValid(Lobby) || Lobby->GetPlayersInLobby() <= 1);
	if (!IsInSession() || bAloneAsHost)
	{
		return;
	}
	if (!IsValid(Provider))
	{
		BroadcastEvent(EMKHSessionEvent::DestroyFail);
		return;
	}

	BroadcastEvent(EMKHSessionEvent::DestroyingSession);
	// The other players learn it at once: destroying a backend lobby with members can take seconds.
	if (UMKHGameInstance* GameInstance = GetMKHGameInstance(); IsValid(GameInstance))
	{
		GameInstance->ReleaseWorldNetDriver();
	}
	Provider->DestroySession(GetCurrentSessionName(), FMKHSessionOperationComplete::CreateWeakLambda(this, [this](bool bSuccess)
	{
		if (!bSuccess)
		{
			BroadcastEvent(EMKHSessionEvent::DestroyFail);
			return;
		}

		UMKHGameInstance* GameInstance = GetMKHGameInstance();
		if (IsValid(GameInstance))
		{
			GameInstance->OnLeaveSession();
		}
		if (UMKHLobbySubsystem* LobbySubsystem = GetLobby(); IsValid(LobbySubsystem))
		{
			LobbySubsystem->SetIsMatchmaking(false);
		}
		BroadcastEvent(EMKHSessionEvent::LobbyDestroyed);
		if (IsValid(GameInstance))
		{
			GameInstance->OpenLevel(EMKHLevelType::Lobby, true);
		}
	}));
}

void UMKHSessionSubsystem::StartMatch()
{
	const UMKHLobbySubsystem* Lobby = GetLobby();
	if (!IsValid(Lobby) || !Lobby->CanStartGame())
	{
		return;
	}

	if (UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(GetGameInstance()); IsValid(Loading))
	{
		Loading->ShowMatchLoadingScreen();
	}

	SetPassiveLobby(FMKHSessionOperationComplete::CreateWeakLambda(this, [this](bool bSuccess)
	{
		if (bSuccess)
		{
			if (UMKHGameInstance* GameInstance = GetMKHGameInstance(); IsValid(GameInstance))
			{
				GameInstance->ServerTravel(EMKHLevelType::Game);
			}
			return;
		}

		if (UMKHLoadingScreenSubsystem* LoadingScreen = UMKHLoadingScreenSubsystem::Get(GetGameInstance()); IsValid(LoadingScreen))
		{
			LoadingScreen->ReportTravelError(EMKHTravelError::LobbyStartFailed);
		}
	}));
}

void UMKHSessionSubsystem::RestoreLobbyAfterError()
{
	// The lobby is reopened as host whatever the destroy answers, and also without a session.
	if (!IsInSession() || !DestroyCurrentSession(FMKHSessionOperationComplete::CreateWeakLambda(this, [this](bool)
		{
			ReopenLobbyAsHost();
		})))
	{
		ReopenLobbyAsHost();
	}
}

void UMKHSessionSubsystem::StartMatchmaking()
{
	bHasCancelled = false;
	ConsecutiveSearchFailures = 0;
	if (UMKHLobbySubsystem* Lobby = GetLobby(); IsValid(Lobby))
	{
		Lobby->SetIsMatchmaking(true);
	}

	MatchmakingStartTime = FDateTime::UtcNow().ToUnixTimestamp();
	if (!IsValid(Provider))
	{
		BroadcastEvent(EMKHSessionEvent::MatchmakingStartError);
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Matchmaking: advertising lobby '%s' (start time %lld)."), *GetCurrentSessionName().ToString(), MatchmakingStartTime);
	Provider->UpdateLobby(MakeLobbySettings(true),
		FMKHSessionOperationComplete::CreateUObject(this, &UMKHSessionSubsystem::HandleMatchmakingLobbyComplete));
}

void UMKHSessionSubsystem::StopMatchmaking()
{
	UMKHLobbySubsystem* Lobby = GetLobby();
	if (IsValid(Lobby))
	{
		Lobby->SetIsMatchmaking(false);
	}
	if (!IsInSession())
	{
		return;
	}

	const bool bAloneAsHost = IsHost() && (!IsValid(Lobby) || Lobby->GetPlayersInLobby() <= 1);
	if (!bAloneAsHost)
	{
		AbandonLobby();
		return;
	}

	SetPassiveLobby(FMKHSessionOperationComplete::CreateWeakLambda(this, [this](bool bSuccess)
	{
		if (bSuccess)
		{
			BroadcastEvent(EMKHSessionEvent::CancelSuccess);
		}
	}));
}

void UMKHSessionSubsystem::CancelMatchmaking()
{
	UMKHLobbySubsystem* Lobby = GetLobby();
	if (!IsValid(Lobby))
	{
		return;
	}

	Lobby->SetIsReady(false);
	if (!Lobby->GetIsMatchmaking())
	{
		return;
	}

	bHasCancelled = true;
	if (!IsInSession())
	{
		BroadcastEvent(EMKHSessionEvent::CancelSuccess);
		return;
	}

	// A join in flight finishes first: the player lands in the opponent's lobby and can leave from there.
	if (!bIsJoiningLobby)
	{
		StopMatchmaking();
	}
}

bool UMKHSessionSubsystem::HasFreeLobbySlot() const
{
	return IsInSession() && IsValid(Provider) && Provider->GetOpenSlots(GetCurrentSessionName()) > 0;
}

bool UMKHSessionSubsystem::InviteFriendToLobby(const FString& FriendId)
{
	if (!HasFreeLobbySlot())
	{
		return false;
	}
	return Provider->SendLobbyInvite(GetCurrentSessionName(), FriendId);
}

void UMKHSessionSubsystem::JoinInvitedLobby(const FMKHLobbySearchResult& Lobby)
{
	HandleLobbyInviteAccepted(Lobby.IsValid(), Lobby);
}

bool UMKHSessionSubsystem::DestroyCurrentSession(const FMKHSessionOperationComplete& OnComplete)
{
	const FName SessionName = GetCurrentSessionName();
	if (!IsValid(Provider) || SessionName.IsNone())
	{
		UE_LOG(LogMKHSession, Warning, TEXT("DestroyCurrentSession: no session provider or no session name."));
		return false;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Destroying session '%s'."), *SessionName.ToString());
	Provider->DestroySession(SessionName, OnComplete);
	return true;
}

void UMKHSessionSubsystem::DestroyCurrentSessionDetached()
{
	DestroyCurrentSession(FMKHSessionOperationComplete());
}

// ============================================================
// Protected / Internal Logic - Helpers
// ============================================================

UMKHGameInstance* UMKHSessionSubsystem::GetMKHGameInstance() const
{
	return Cast<UMKHGameInstance>(GetGameInstance());
}

UMKHLobbySubsystem* UMKHSessionSubsystem::GetLobby() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHLobbySubsystem>() : nullptr;
}

FName UMKHSessionSubsystem::GetCurrentSessionName() const
{
	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	return IsValid(GameInstance) ? GameInstance->GetCurrentSessionName() : NAME_None;
}

bool UMKHSessionSubsystem::IsInSession() const
{
	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	return IsValid(GameInstance) && GameInstance->GetIsInSession();
}

bool UMKHSessionSubsystem::IsHost() const
{
	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	return IsValid(GameInstance) && GameInstance->GetIsHost();
}

void UMKHSessionSubsystem::BroadcastEvent(EMKHSessionEvent Event)
{
	UE_LOG(LogMKHSession, Verbose, TEXT("Session event: %s."), *UEnum::GetValueAsString(Event));
	OnSessionEvent.Broadcast(Event);
}

FMKHLobbySessionSettings UMKHSessionSubsystem::MakeLobbySettings(bool bMatchmaking) const
{
	FMKHLobbySessionSettings Settings;
	Settings.SessionName = GetCurrentSessionName();
	Settings.MaxPlayers = MKHSession::LobbySize;
	Settings.bAdvertise = bMatchmaking;
	if (const UMKHLobbySubsystem* Lobby = GetLobby(); IsValid(Lobby))
	{
		Settings.Region = Lobby->GetSelectedRegion();
	}

	Settings.Attributes.Add(FMKHLobbyKeys::IsMatchmaking, bMatchmaking ? FMKHLobbyKeys::TrueValue : FMKHLobbyKeys::FalseValue);
	Settings.Attributes.Add(FMKHLobbyKeys::MatchmakingTime, bMatchmaking ? LexToString(MatchmakingStartTime) : FString(MKHSession::NotSearchingTime));
	return Settings;
}

void UMKHSessionSubsystem::SetPassiveLobby(const FMKHSessionOperationComplete& OnComplete)
{
	if (!IsValid(Provider))
	{
		OnComplete.ExecuteIfBound(false);
		return;
	}
	Provider->UpdateLobby(MakeLobbySettings(false), OnComplete);
}

// ============================================================
// Protected / Internal Logic - Lobby and matchmaking flow
// ============================================================

void UMKHSessionSubsystem::HandleHostLobbyComplete(bool bSuccess)
{
	UE_LOG(LogMKHSession, Log, TEXT("HostLobby: lobby '%s' %s."), *GetCurrentSessionName().ToString(), bSuccess ? TEXT("hosted") : TEXT("creation failed"));
	if (bSuccess)
	{
		if (bShouldNotifyLobby)
		{
			BroadcastEvent(EMKHSessionEvent::CreateLobbySuccess);
		}
		if (UMKHGameInstance* GameInstance = GetMKHGameInstance(); IsValid(GameInstance))
		{
			GameInstance->OnLobbyCreated();
		}
		ScheduleDeferredInvite();
	}
	else
	{
		BroadcastEvent(EMKHSessionEvent::CreateLobbyFail);
	}

	// A cancel pressed during a join that failed is completed once the own lobby is back.
	if (bHasCancelled)
	{
		CancelMatchmaking();
	}
}

void UMKHSessionSubsystem::HandleMatchmakingLobbyComplete(bool bSuccess)
{
	if (!bSuccess)
	{
		UE_LOG(LogMKHSession, Warning, TEXT("Matchmaking: the lobby could not be advertised."));
		BroadcastEvent(EMKHSessionEvent::MatchmakingStartError);
		return;
	}

	BroadcastEvent(EMKHSessionEvent::MatchmakingStart);
	SearchLobbies();
}

void UMKHSessionSubsystem::SearchLobbies()
{
	// A client of another lobby never searches.
	if (IsInSession() && !IsHost())
	{
		return;
	}
	if (!IsValid(Provider))
	{
		BroadcastEvent(EMKHSessionEvent::SearchError);
		return;
	}

	FMKHLobbySearchQuery Query;
	Query.MaxResults = MKHSession::MaxSearchResults;
	Query.RequiredAttributes.Add(FMKHLobbyKeys::IsMatchmaking, FMKHLobbyKeys::TrueValue);
	if (const UMKHLobbySubsystem* Lobby = GetLobby(); IsValid(Lobby))
	{
		Query.Region = Lobby->GetSelectedRegion();
	}

	BroadcastEvent(EMKHSessionEvent::SearchStart);
	Provider->SearchLobbies(Query, FMKHLobbySearchComplete::CreateUObject(this, &UMKHSessionSubsystem::HandleSearchComplete));
}

void UMKHSessionSubsystem::HandleSearchComplete(bool bSuccess, const TArray<FMKHLobbySearchResult>& Results)
{
	if (!bSuccess)
	{
		HandleSearchFailed();
		return;
	}
	ConsecutiveSearchFailures = 0;
	if (bHasCancelled)
	{
		CancelMatchmaking();
		return;
	}

	UMKHLobbySubsystem* LobbySubsystem = GetLobby();
	if (IsValid(LobbySubsystem))
	{
		LobbySubsystem->ResetJoinTarget();
	}

	for (const FMKHLobbySearchResult& Lobby : FilterSearchResults(Results))
	{
		const ELobbyJoinCheck Check = CanJoinLobby(Lobby);
		if (Check == ELobbyJoinCheck::Invalid)
		{
			BroadcastEvent(EMKHSessionEvent::FoundInvalidLobby);
			continue;
		}
		if (Check != ELobbyJoinCheck::Joinable)
		{
			continue;
		}

		// The destroy of the own lobby is asynchronous, the loop is not: only the first joinable lobby is joined.
		const bool bChosen = !IsValid(LobbySubsystem) || LobbySubsystem->TryChooseJoinTarget(Lobby.LobbyId);
		UE_LOG(LogMKHSession, Log, TEXT("Matchmaking: lobby %s of %s %s."), *Lobby.LobbyId, *Lobby.OwnerSessionName,
			bChosen ? TEXT("chosen to join") : TEXT("skipped, a join already started for this search"));
		if (bChosen)
		{
			BeginMatchmakingJoin(Lobby);
		}
	}

	HandleSearchFinished();
}

TArray<FMKHLobbySearchResult> UMKHSessionSubsystem::FilterSearchResults(const TArray<FMKHLobbySearchResult>& Results) const
{
	const UMKHLobbySubsystem* Lobby = GetLobby();

	// Most recent lobby of each owner (same session name before and after a return to the lobby).
	TMap<FString, int32> NewestByOwner;
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		const FMKHLobbySearchResult& Result = Results[Index];
		const bool bUnreachable = IsValid(Lobby) && Lobby->IsLobbyUnreachable(Result.LobbyId);
		UE_LOG(LogMKHSession, Log, TEXT("Matchmaking result: lobby %s, owner %s, MatchmakingTime %lld, players %d/%d%s."),
			*Result.LobbyId, *Result.OwnerSessionName, Result.GetMatchmakingTime(), Result.CurrentPlayers, Result.MaxPlayers,
			bUnreachable ? TEXT(", recently unreachable: skipped") : TEXT(""));
		if (bUnreachable)
		{
			continue;
		}

		const int32* Kept = NewestByOwner.Find(Result.OwnerSessionName);
		if (!Kept || Result.GetMatchmakingTime() > Results[*Kept].GetMatchmakingTime())
		{
			NewestByOwner.Add(Result.OwnerSessionName, Index);
		}
	}

	TArray<FMKHLobbySearchResult> Filtered;
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		if (const int32* Kept = NewestByOwner.Find(Results[Index].OwnerSessionName); Kept && *Kept == Index)
		{
			Filtered.Add(Results[Index]);
		}
	}

	if (Filtered.Num() != Results.Num())
	{
		UE_LOG(LogMKHSession, Log, TEXT("Matchmaking: %d of %d lobbies kept (older lobbies of the same owner and unreachable ones dropped)."),
			Filtered.Num(), Results.Num());
	}
	return Filtered;
}

UMKHSessionSubsystem::ELobbyJoinCheck UMKHSessionSubsystem::CanJoinLobby(const FMKHLobbySearchResult& Lobby) const
{
	if (Lobby.OwnerSessionName.Equals(GetCurrentSessionName().ToString(), ESearchCase::IgnoreCase))
	{
		return ELobbyJoinCheck::NotJoinable;
	}
	if (!Lobby.IsValid())
	{
		return ELobbyJoinCheck::Invalid;
	}

	const bool bHasFreeSlot = Lobby.CurrentPlayers > 0 && Lobby.CurrentPlayers < Lobby.MaxPlayers;
	return bHasFreeSlot && HasJoinPriority(Lobby) ? ELobbyJoinCheck::Joinable : ELobbyJoinCheck::NotJoinable;
}

bool UMKHSessionSubsystem::HasJoinPriority(const FMKHLobbySearchResult& Lobby) const
{
	if (!Lobby.HasAttribute(FMKHLobbyKeys::MatchmakingTime))
	{
		return false;
	}

	const int64 OtherStartTime = Lobby.GetMatchmakingTime();
	if (MatchmakingStartTime != OtherStartTime)
	{
		return MatchmakingStartTime > OtherStartTime;
	}

	// Same second: compared as strings so both players agree whatever the backend id format.
	return Lobby.OwnerSessionName.Compare(GetCurrentSessionName().ToString(), ESearchCase::CaseSensitive) < 0;
}

void UMKHSessionSubsystem::BeginMatchmakingJoin(const FMKHLobbySearchResult& Lobby)
{
	JoinTarget = Lobby;

	// The join goes on even when the own lobby could not be destroyed (or there was none).
	if (!IsInSession() || !DestroyCurrentSession(FMKHSessionOperationComplete::CreateWeakLambda(this, [this](bool bSuccess)
		{
			UE_CLOG(!bSuccess, LogMKHSession, Warning, TEXT("Matchmaking: the own lobby could not be destroyed before the join."));
			StartMatchmakingJoin();
		})))
	{
		StartMatchmakingJoin();
	}
}

void UMKHSessionSubsystem::StartMatchmakingJoin()
{
	if (UMKHGameInstance* GameInstance = GetMKHGameInstance(); IsValid(GameInstance))
	{
		GameInstance->OnLeaveSession();
	}

	// Matchmaking join: the flow and the loading screen follow it (an invite join does not).
	BroadcastEvent(EMKHSessionEvent::JoiningStart);
	bIsJoiningLobby = true;
	if (UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(GetGameInstance()); IsValid(Loading))
	{
		Loading->ShowLoadingScreen(EMKHLoadingReason::MatchFound, EMKHLoadingDestination::Game);
	}
	JoinLobby(JoinTarget);
}

void UMKHSessionSubsystem::JoinLobby(const FMKHLobbySearchResult& Lobby)
{
	if (!IsValid(Provider))
	{
		HandleJoinLobbyComplete(EMKHJoinLobbyResult::UnknownError, FString());
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Joining lobby %s of %s."), *Lobby.LobbyId, *Lobby.OwnerSessionName);
	Provider->JoinLobby(GetCurrentSessionName(), Lobby, FMKHLobbyJoinComplete::CreateUObject(this, &UMKHSessionSubsystem::HandleJoinLobbyComplete));
}

void UMKHSessionSubsystem::HandleJoinLobbyComplete(EMKHJoinLobbyResult Result, const FString& ConnectString)
{
	UMKHGameInstance* GameInstance = GetMKHGameInstance();
	if (Result == EMKHJoinLobbyResult::Success)
	{
		UE_LOG(LogMKHSession, Log, TEXT("Lobby joined: travelling to %s."), *ConnectString);
		if (APlayerController* PlayerController = IsValid(GameInstance) ? GameInstance->GetFirstLocalPlayerController() : nullptr;
			IsValid(PlayerController))
		{
			PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
		}

		BroadcastEvent(EMKHSessionEvent::JoiningSuccess);
		if (IsValid(GameInstance))
		{
			GameInstance->OnJoinSession();
		}
		if (UMKHLobbySubsystem* Lobby = GetLobby(); IsValid(Lobby))
		{
			// Voice chat: an invite or friend join makes the pair a party (checked again by the host).
			Lobby->SetJoinedThroughInvite(bIsJoiningInvite);
		}
		bIsJoiningLobby = false;
		bIsJoiningInvite = false;
		return;
	}

	UE_LOG(LogMKHSession, Warning, TEXT("Lobby join failed: %s."), *UEnum::GetValueAsString(Result));
	if (!bIsJoiningLobby && !bIsJoiningInvite)
	{
		return;
	}

	// The own lobby is already gone: host it again and tell the player (silent retry while matchmaking).
	if (bIsJoiningLobby)
	{
		BroadcastEvent(EMKHSessionEvent::JoiningFail);
	}
	bIsJoiningLobby = false;
	bIsJoiningInvite = false;
	HostLobby(false);
	if (UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(GetGameInstance()); IsValid(Loading))
	{
		Loading->ReportTravelError(EMKHTravelError::JoinFailed);
	}
}

void UMKHSessionSubsystem::HandleSearchFailed()
{
	++ConsecutiveSearchFailures;
	UE_LOG(LogMKHSession, Warning, TEXT("Matchmaking: the lobby search failed (%d in a row)."), ConsecutiveSearchFailures);
	if (ConsecutiveSearchFailures >= MKHSession::MaxConsecutiveSearchFailures)
	{
		BroadcastEvent(EMKHSessionEvent::SearchError);
		return;
	}

	// A transient backend error: the next search runs as if this one had found nothing.
	HandleSearchFinished();
}

void UMKHSessionSubsystem::HandleSearchFinished()
{
	if (bHasCancelled)
	{
		CancelMatchmaking();
		return;
	}
	if (!ShouldStopSearch())
	{
		ScheduleSearchAgain();
	}
}

void UMKHSessionSubsystem::HandleSearchAgain()
{
	if (bHasCancelled)
	{
		CancelMatchmaking();
		return;
	}
	if (ShouldSearchLater())
	{
		ScheduleSearchAgain();
		return;
	}
	if (!ShouldStopSearch())
	{
		SearchLobbies();
	}
}

void UMKHSessionSubsystem::ScheduleSearchAgain()
{
	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return;
	}

	const float Interval = FMath::Max(UMKHLobbySettings::Get()->MatchmakingSearchInterval, 0.5f);
	World->GetTimerManager().SetTimer(SearchAgainHandle, this, &UMKHSessionSubsystem::HandleSearchAgain, Interval, false);
}

bool UMKHSessionSubsystem::ShouldStopSearch() const
{
	const UMKHLobbySubsystem* Lobby = GetLobby();
	if (!IsValid(Lobby) || !Lobby->GetIsMatchmaking())
	{
		return true;
	}
	return IsHost() ? Lobby->GetPlayersInLobby() >= Lobby->GetPlayersToStart() : IsInSession();
}

bool UMKHSessionSubsystem::ShouldSearchLater() const
{
	return bIsJoiningLobby || !IsInSession();
}

void UMKHSessionSubsystem::HandleLobbyInviteAccepted(bool bSuccess, const FMKHLobbySearchResult& Lobby)
{
	if (!bSuccess || !IsValid(Provider))
	{
		return;
	}
	if (!CanJoinInvitedLobbyNow())
	{
		// E.g. the game was started by the invite: the own lobby is not hosted yet.
		UE_LOG(LogMKHSession, Log, TEXT("Invite accepted: lobby %s kept until the lobby is ready."), *Lobby.LobbyId);
		DeferredInvite = Lobby;
		return;
	}

	DeferredInvite = FMKHLobbySearchResult();
	UE_LOG(LogMKHSession, Log, TEXT("Invite accepted: leaving '%s' to join lobby %s."), *GetCurrentSessionName().ToString(), *Lobby.LobbyId);

	// Like the matchmaking join, the invite join goes on whatever the destroy of the current session answers.
	Provider->DestroySession(GetCurrentSessionName(), FMKHSessionOperationComplete::CreateWeakLambda(this, [this, Lobby](bool)
	{
		if (UMKHGameInstance* GameInstance = GetMKHGameInstance(); IsValid(GameInstance))
		{
			GameInstance->OnLeaveSession();
		}
		bIsJoiningInvite = true;
		JoinLobby(Lobby);
	}));
}

bool UMKHSessionSubsystem::CanJoinInvitedLobbyNow() const
{
	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	return IsValid(GameInstance) && GameInstance->IsInLobbyLevel() && IsInSession() && !bIsJoiningLobby && !bIsJoiningInvite;
}

void UMKHSessionSubsystem::ScheduleDeferredInvite()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (!DeferredInvite.IsValid() || !IsValid(GameInstance))
	{
		return;
	}

	// Next tick: the caller (lobby hosted, level started) finishes its own work first.
	GameInstance->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (DeferredInvite.IsValid() && CanJoinInvitedLobbyNow())
		{
			HandleLobbyInviteAccepted(true, DeferredInvite);
		}
	}));
}

void UMKHSessionSubsystem::ReopenLobbyAsHost()
{
	UMKHGameInstance* GameInstance = GetMKHGameInstance();
	if (IsValid(GameInstance))
	{
		GameInstance->OnLeaveSession();
	}
	if (UMKHLobbySubsystem* Lobby = GetLobby(); IsValid(Lobby))
	{
		Lobby->SetIsMatchmaking(false);
	}
	if (IsValid(GameInstance))
	{
		GameInstance->OpenLevel(EMKHLevelType::Lobby, true);
	}
}
