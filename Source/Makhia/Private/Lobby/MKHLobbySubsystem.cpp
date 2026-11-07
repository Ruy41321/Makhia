// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Lobby/MKHLobbySubsystem.h"
#include "Career/MKHCareerSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameInstance/MKHGameInstance.h"
#include "Lobby/MKHLobbyGameState.h"
#include "Lobby/MKHLobbySettings.h"
#include "Lobby/MKHLobbyPlayerState.h"
#include "MKHLogChannels.h"

#define LOCTEXT_NAMESPACE "MKHLobbyNotice"

// ============================================================
// Public Interface
// ============================================================

UMKHLobbySubsystem* UMKHLobbySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHLobbySubsystem>() : nullptr;
}

int32 UMKHLobbySubsystem::DecrementPlayersInLobby()
{
	CancelMatchCountdown();
	return --PlayersInLobby;
}

bool UMKHLobbySubsystem::HasOtherPlayersInLobby() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	return IsValid(GameState) && GameState->PlayerArray.Num() > 1;
}

void UMKHLobbySubsystem::HandleLobbyHosted()
{
	PlayersInLobby = 1;
	ResetVoiceParty();
}

void UMKHLobbySubsystem::HandleSessionLeft()
{
	PlayersInLobby = 0;
	PlayersReady = 0;
	ResetVoiceParty();
}

void UMKHLobbySubsystem::ResetLobbyState()
{
	CancelMatchCountdown();
	PlayersReady = 0;
	PlayersInLobby = 0;
	bIsReady = false;
	bIsMatchmaking = false;
	ResetVoiceParty();
}

void UMKHLobbySubsystem::ResetVoiceParty()
{
	VoicePartyMemberIds.Reset();
	bJoinedThroughInvite = false;
}

void UMKHLobbySubsystem::SetIsReady(bool bNewReady)
{
	if (bIsReady != bNewReady)
	{
		bIsReady = bNewReady;
		OnReadyStateChanged.Broadcast(bIsReady);
	}
}

void UMKHLobbySubsystem::HandlePlayerReady(bool bNewReady)
{
	if (!bNewReady)
	{
		PlayersReady--;
		CancelMatchCountdown();
		return;
	}

	PlayersReady++;
	if (PlayersReady != PlayersInLobby)
	{
		return;
	}

	if (PlayersReady >= PlayersToStart)
	{
		BeginMatchCountdown();
	}
	else
	{
		OnStartMatchmakingDelegate.Broadcast();
	}
}

bool UMKHLobbySubsystem::CanStartGame() const
{
	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	if (!IsValid(GameInstance) || !GameInstance->GetIsHost() || PlayersInLobby < PlayersToStart)
	{
		return false;
	}

	return bIsMatchmaking || PlayersReady >= PlayersInLobby;
}

bool UMKHLobbySubsystem::IsMatchCountdownRunning() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) && World->GetTimerManager().IsTimerActive(MatchCountdownHandle);
}

void UMKHLobbySubsystem::RequestOfflineMatch(const EMKHOfflineMode Mode, const int32 CareerLevelIndex)
{
	UWorld* World = GetWorld();
	if (IsOfflineMatchPending() || IsOfflineMatchRefused() || !IsValid(World))
	{
		return;
	}

	PendingOfflineRequest.Mode = Mode;
	PendingOfflineRequest.CareerLevelIndex = CareerLevelIndex;
	if (Mode == EMKHOfflineMode::Career)
	{
		const UMKHCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UMKHCareerSubsystem>();
		if (!IsValid(Career) || Career->GetLevelCount() == 0)
		{
			UE_LOG(LogMKHSession, Warning, TEXT("RequestOfflineMatch: the career has no levels."));
			return;
		}
		if (CareerLevelIndex == INDEX_NONE)
		{
			PendingOfflineRequest.CareerLevelIndex = Career->GetNextLevelIndex();
		}
		else if (!Career->IsLevelUnlocked(CareerLevelIndex))
		{
			UE_LOG(LogMKHSession, Warning, TEXT("RequestOfflineMatch: career level %d is locked."), CareerLevelIndex);
			return;
		}
	}

	const UMKHLobbySettings* Settings = UMKHLobbySettings::Get();
	const float DelayMin = Settings->OfflineMatchDelayMin;
	const float Delay = FMath::FRandRange(DelayMin, FMath::Max(DelayMin, Settings->OfflineMatchDelayMax));
	if (Delay <= 0.f)
	{
		HandleOfflineMatchDelayFinished();
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("RequestOfflineMatch: the match against the bot starts in %.1f s unless cancelled."), Delay);
	World->GetTimerManager().SetTimer(OfflineMatchDelayHandle, this, &UMKHLobbySubsystem::HandleOfflineMatchDelayFinished, Delay, false);
	OnOfflineMatchPendingChanged.Broadcast(true);
}

void UMKHLobbySubsystem::CancelOfflineMatchRequest()
{
	if (!IsOfflineMatchPending())
	{
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("RequestOfflineMatch: cancelled."));
	GetWorld()->GetTimerManager().ClearTimer(OfflineMatchDelayHandle);
	OnOfflineMatchPendingChanged.Broadcast(false);
}

bool UMKHLobbySubsystem::IsOfflineMatchPending() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) && World->GetTimerManager().IsTimerActive(OfflineMatchDelayHandle);
}

bool UMKHLobbySubsystem::IsOfflineMatchRefused() const
{
	// The bot is a solo mode: same rule as the lobby card.
	if (bIsMatchmaking || PlayersInLobby > 1 || (IsInLobbyLevel() && HasOtherPlayersInLobby()))
	{
		UE_LOG(LogMKHSession, Warning, TEXT("Match against the bot refused while matchmaking or with other players in the lobby (%d)."), PlayersInLobby);
		return true;
	}

	return false;
}

bool UMKHLobbySubsystem::TryChooseJoinTarget(const FString& LobbyId)
{
	if (bJoinTargetChosen)
	{
		return false;
	}

	bJoinTargetChosen = true;
	JoinTargetLobbyId = LobbyId;
	return true;
}

void UMKHLobbySubsystem::MarkJoinTargetUnreachable()
{
	if (JoinTargetLobbyId.IsEmpty())
	{
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Matchmaking: lobby %s could not be joined, skipped for %.0f s."),
		*JoinTargetLobbyId, UMKHLobbySettings::Get()->UnreachableLobbyMemory);
	UnreachableLobbies.Add(JoinTargetLobbyId, FPlatformTime::Seconds());
	JoinTargetLobbyId.Reset();
}

bool UMKHLobbySubsystem::IsLobbyUnreachable(const FString& LobbyId) const
{
	const double* FailedAt = UnreachableLobbies.Find(LobbyId);
	return FailedAt && FPlatformTime::Seconds() - *FailedAt < UMKHLobbySettings::Get()->UnreachableLobbyMemory;
}

void UMKHLobbySubsystem::PostLobbyNotice(const FText& Message, const bool bShowAfterTravel)
{
	if (Message.IsEmpty())
	{
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Lobby notice%s: %s"), bShowAfterTravel ? TEXT(" (next lobby)") : TEXT(""), *Message.ToString());
	PendingLobbyNotice = Message;

	// Without the broadcast the next lobby HUD picks it up when it starts.
	if (!bShowAfterTravel)
	{
		OnLobbyNoticePosted.Broadcast();
	}
}

void UMKHLobbySubsystem::PostPlayerLeftNotice(const APlayerState* PlayerState, const bool bShowAfterTravel)
{
	FString PlayerName;
	if (const AMKHLobbyPlayerState* LobbyPlayerState = Cast<AMKHLobbyPlayerState>(PlayerState); IsValid(LobbyPlayerState))
	{
		PlayerName = LobbyPlayerState->GetLobbyDisplayName();
	}
	if (PlayerName.IsEmpty() && IsValid(PlayerState))
	{
		PlayerName = PlayerState->GetPlayerName();
	}

	if (PlayerName.IsEmpty())
	{
		PostLobbyNotice(LOCTEXT("SomeoneLeftLobby", "A player left the lobby"), bShowAfterTravel);
		return;
	}

	PostLobbyNotice(FText::Format(LOCTEXT("PlayerLeftLobby", "{Name} left the lobby"),
		FFormatNamedArguments{ { TEXT("Name"), FText::AsCultureInvariant(PlayerName) } }), bShowAfterTravel);
}

FText UMKHLobbySubsystem::ConsumeLobbyNotice()
{
	FText Notice = PendingLobbyNotice;
	PendingLobbyNotice = FText::GetEmpty();
	return Notice;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHLobbySubsystem::BeginMatchCountdown()
{
	UWorld* World = GetWorld();
	const float Countdown = UMKHLobbySettings::Get()->MatchStartCountdown;
	if (Countdown <= 0.f || !IsValid(World))
	{
		OnStartMatchDelegate.Broadcast();
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Every player is ready: the match starts in %.1f s unless someone cancels."), Countdown);
	World->GetTimerManager().SetTimer(MatchCountdownHandle, this, &UMKHLobbySubsystem::HandleMatchCountdownFinished, Countdown, false);
	PublishMatchCountdown(Countdown);
}

void UMKHLobbySubsystem::CancelMatchCountdown()
{
	if (!IsMatchCountdownRunning())
	{
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Match countdown cancelled."));
	GetWorld()->GetTimerManager().ClearTimer(MatchCountdownHandle);
	PublishMatchCountdown(-1.f);
}

void UMKHLobbySubsystem::HandleMatchCountdownFinished()
{
	// A player may have left without un-readying first.
	if (PlayersInLobby < PlayersToStart || PlayersReady < PlayersInLobby)
	{
		UE_LOG(LogMKHSession, Warning, TEXT("Match countdown over, but not every player is ready any more (%d/%d)."), PlayersReady, PlayersInLobby);
		PublishMatchCountdown(-1.f);
		return;
	}

	// The countdown stays at zero: the clients cover the lobby with the loading screen until the travel.
	OnStartMatchDelegate.Broadcast();
}

void UMKHLobbySubsystem::PublishMatchCountdown(float Duration) const
{
	const UWorld* World = GetWorld();
	if (AMKHLobbyGameState* LobbyGameState = IsValid(World) ? World->GetGameState<AMKHLobbyGameState>() : nullptr; IsValid(LobbyGameState))
	{
		LobbyGameState->SetMatchCountdown(Duration);
	}
}

void UMKHLobbySubsystem::HandleOfflineMatchDelayFinished()
{
	// The timer still reads as active inside its own callback.
	OfflineMatchDelayHandle.Invalidate();
	OnOfflineMatchPendingChanged.Broadcast(false);

	if (UMKHGameInstance* GameInstance = GetMKHGameInstance(); IsValid(GameInstance))
	{
		GameInstance->StartOfflineMatch(PendingOfflineRequest);
	}
}

UMKHGameInstance* UMKHLobbySubsystem::GetMKHGameInstance() const
{
	return Cast<UMKHGameInstance>(GetGameInstance());
}

bool UMKHLobbySubsystem::IsInLobbyLevel() const
{
	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	return IsValid(GameInstance) && GameInstance->IsInLobbyLevel();
}

#undef LOCTEXT_NAMESPACE
