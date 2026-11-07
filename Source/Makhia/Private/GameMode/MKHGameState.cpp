// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/MKHGameState.h"

#include "Character/CharacterBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "MKHLogChannels.h"
#include "Online/MKHFriendsSubsystem.h"

// ============================================================
// Lifecycle
// ============================================================

AMKHGameState::AMKHGameState()
{
	// Two fixed scoreboard slots for the 1v1 participants.
	PlayerDatas.SetNum(2);
	PreviousPlayerDatas = PlayerDatas;
}

void AMKHGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMKHGameState, DuelState);
	DOREPLIFETIME(AMKHGameState, PlayerDatas);
	DOREPLIFETIME(AMKHGameState, RematchVotes);
	DOREPLIFETIME(AMKHGameState, PreparationSkipVotes);
}

// ============================================================
// Public Interface (Server authority)
// ============================================================

void AMKHGameState::SetMatchPhase(EMKHMatchPhase NewPhase, float PhaseDurationSeconds, int32 Round)
{
	if (!HasAuthority())
	{
		return;
	}

	DuelState.Phase = NewPhase;
	DuelState.Round = Round;
	DuelState.PhaseDuration = FMath::Max(0.f, PhaseDurationSeconds);
	DuelState.PhaseEndServerTime = (DuelState.PhaseDuration > 0.f)
		? GetServerWorldTimeSeconds() + DuelState.PhaseDuration
		: 0.f;
	DuelState.bFinalCountdown = false;
	++DuelState.PhaseSerial;

	// Push the change to clients immediately instead of waiting for the next net cadence.
	ForceNetUpdate();

	// OnRep does not run on the authority, so broadcast the phase events locally here.
	HandlePhaseChanged();
}

void AMKHGameState::RegisterParticipant(int32 SlotIndex, ACharacterBase* Character)
{
	if (!HasAuthority() || !PlayerDatas.IsValidIndex(SlotIndex))
	{
		return;
	}

	PlayerDatas[SlotIndex].Participant = Character;
	PlayerDatas[SlotIndex].Score = 0;

	ForceNetUpdate();
	OnParticipantRegistered.Broadcast(SlotIndex, Character);
}

void AMKHGameState::BroadcastExistingPlayerDatas()
{
	for (int32 SlotIndex = 0; SlotIndex < PlayerDatas.Num(); ++SlotIndex)
	{
		const FMKHDuelPlayerData& Data = PlayerDatas[SlotIndex];
		if (!IsValid(Data.Participant))
		{
			continue;
		}

		OnParticipantRegistered.Broadcast(SlotIndex, Data.Participant.Get());
		OnScoreChanged.Broadcast(SlotIndex, Data.Score);
	}
}

void AMKHGameState::BroadcastCurrentPhase()
{
	// Only the generic notification is replayed: the granular per-phase events (OnInitialPreparationStarted,
	// etc.) carry one-shot semantics and must not be re-fired to already-bound listeners.
	OnMatchPhaseChanged.Broadcast(DuelState.Phase);

	// The final countdown is state rather than a moment, so replaying it only re-applies what listeners show.
	if (DuelState.bFinalCountdown)
	{
		OnFinalCountdownStarted.Broadcast();
	}
}

void AMKHGameState::SetScore(int32 SlotIndex, int32 NewScore)
{
	if (!HasAuthority() || !PlayerDatas.IsValidIndex(SlotIndex))
	{
		return;
	}

	PlayerDatas[SlotIndex].Score = NewScore;

	ForceNetUpdate();
	OnScoreChanged.Broadcast(SlotIndex, NewScore);
}

void AMKHGameState::Multicast_PlayerDied_Implementation(ACharacterBase* DeadPlayer, ACharacterBase* ScoringPlayer, int32 ScoringScore)
{
	OnPlayerDied.Broadcast(DeadPlayer, ScoringPlayer, ScoringScore);
}

void AMKHGameState::Multicast_MatchOver_Implementation(ACharacterBase* WinningPlayer, bool bIsDraw)
{
	OnMatchOver.Broadcast(WinningPlayer, bIsDraw);
}

void AMKHGameState::SetRematchVotes(int32 CurrentVotes, int32 RequiredVotes)
{
	if (!HasAuthority())
	{
		return;
	}

	RematchVotes.CurrentVotes = CurrentVotes;
	RematchVotes.RequiredVotes = RequiredVotes;

	// Push the change immediately: the panel is waiting on it to update the button label.
	ForceNetUpdate();

	// OnRep does not run on the authority, so broadcast locally here.
	OnRematchVotesChanged.Broadcast(CurrentVotes, RequiredVotes);
}

void AMKHGameState::StartFinalCountdown(float CountdownSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	// PhaseDuration is left untouched: the phase keeps its identity, only its deadline moves.
	DuelState.bFinalCountdown = true;
	DuelState.PhaseEndServerTime = GetServerWorldTimeSeconds() + FMath::Max(0.f, CountdownSeconds);

	ForceNetUpdate();

	// OnRep does not run on the authority, so broadcast locally here.
	OnFinalCountdownStarted.Broadcast();
}

void AMKHGameState::SetPreparationSkipVotes(const TArray<TObjectPtr<APlayerState>>& ReadyPlayers, int32 RequiredVotes)
{
	if (!HasAuthority())
	{
		return;
	}

	PreparationSkipVotes.ReadyPlayers = ReadyPlayers;
	PreparationSkipVotes.RequiredVotes = RequiredVotes;

	// Push the change immediately: both ready-up widgets are waiting on it.
	ForceNetUpdate();

	// OnRep does not run on the authority, so broadcast locally here.
	OnPreparationSkipVotesChanged.Broadcast(ReadyPlayers.Num(), RequiredVotes);
}

// ============================================================
// Public Interface (Read-only queries)
// ============================================================

bool AMKHGameState::IsPlayerReadyToSkip(const APlayerState* PlayerState) const
{
	return IsValid(PlayerState) && PreparationSkipVotes.ReadyPlayers.Contains(PlayerState);
}

bool AMKHGameState::IsPhaseTimerActive() const
{
	return DuelState.PhaseEndServerTime > 0.f;
}

float AMKHGameState::GetPhaseTimeRemaining() const
{
	if (!IsPhaseTimerActive())
	{
		return 0.f;
	}

	return FMath::Max(0.f, DuelState.PhaseEndServerTime - GetServerWorldTimeSeconds());
}

float AMKHGameState::GetPhaseElapsedFraction() const
{
	if (!IsPhaseTimerActive() || DuelState.PhaseDuration <= 0.f)
	{
		return 0.f;
	}

	const float Elapsed = DuelState.PhaseDuration - GetPhaseTimeRemaining();
	return FMath::Clamp(Elapsed / DuelState.PhaseDuration, 0.f, 1.f);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AMKHGameState::HandlePhaseChanged()
{
	OnMatchPhaseChanged.Broadcast(DuelState.Phase);

	// Fan the generic change out to the granular per-phase events UI can bind to directly.
	switch (DuelState.Phase)
	{
	case EMKHMatchPhase::InitialPreparation:
		OnInitialPreparationStarted.Broadcast();
		ReportMatchOpponents();
		break;
	case EMKHMatchPhase::Duel:
		OnDuelStarted.Broadcast();
		// Again at each duel: on a client the opponent's player state may replicate after the preparation phase.
		ReportMatchOpponents();
		break;
	case EMKHMatchPhase::SuddenDeath:
		OnDuelTimeOver.Broadcast();
		break;
	case EMKHMatchPhase::IntermissionPreparation:
		OnIntermissionPreparationStarted.Broadcast();
		break;
	default:
		break;
	}
}

void AMKHGameState::ReportMatchOpponents() const
{
	// The platform remembers the opponent for its recent players list.
	if (UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this); IsValid(Friends))
	{
		Friends->ReportMatchOpponents(this);
	}
}

void AMKHGameState::OnRep_DuelState(const FMKHMatchStateData& OldDuelState)
{
	// Same serial: the server only moved the countdown of the current phase, which is not a phase change.
	const bool bPhaseChanged = DuelState.PhaseSerial != OldDuelState.PhaseSerial;
	if (bPhaseChanged)
	{
		HandlePhaseChanged();
	}

	// A phase shorter than the final countdown enters it in the same update as the phase itself.
	if (DuelState.bFinalCountdown && (bPhaseChanged || !OldDuelState.bFinalCountdown))
	{
		OnFinalCountdownStarted.Broadcast();
	}
}

void AMKHGameState::OnRep_PlayerDatas()
{
	// A single OnRep covers any change to the array, so diff against the previous snapshot
	// to fire the event matching what actually changed in each slot.
	for (int32 SlotIndex = 0; SlotIndex < PlayerDatas.Num(); ++SlotIndex)
	{
		const FMKHDuelPlayerData& Current = PlayerDatas[SlotIndex];
		const FMKHDuelPlayerData Previous = PreviousPlayerDatas.IsValidIndex(SlotIndex)
			? PreviousPlayerDatas[SlotIndex]
			: FMKHDuelPlayerData();

		if (Current.Participant != Previous.Participant && IsValid(Current.Participant))
		{
			OnParticipantRegistered.Broadcast(SlotIndex, Current.Participant.Get());
		}

		if (Current.Score != Previous.Score)
		{
			OnScoreChanged.Broadcast(SlotIndex, Current.Score);
		}
	}

	PreviousPlayerDatas = PlayerDatas;
}

void AMKHGameState::OnRep_RematchVotes()
{
	OnRematchVotesChanged.Broadcast(RematchVotes.CurrentVotes, RematchVotes.RequiredVotes);
}

void AMKHGameState::OnRep_PreparationSkipVotes()
{
	OnPreparationSkipVotesChanged.Broadcast(PreparationSkipVotes.ReadyPlayers.Num(), PreparationSkipVotes.RequiredVotes);
}
