// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "MKHGameState.generated.h"

class ACharacterBase;
class APlayerState;

/**
 * High-level phase the 1v1 duel match is currently in.
 * Drives which timer is running and which gameplay rules are active.
 */
UENUM(BlueprintType)
enum class EMKHMatchPhase : uint8
{
	/** Match has not started yet (players still joining). */
	None,
	/** Initial equip phase: players prepare their loadout before the first duel. */
	InitialPreparation,
	/** Active duel: both players fight until one dies or the duel timer expires. */
	Duel,
	/** Sudden-death window after the duel timer expired: a drain effect ticks down health. */
	SuddenDeath,
	/** Short preparation phase played between two duels. */
	IntermissionPreparation,
	/** The match ended with a winner or a draw. */
	MatchOver
};

/**
 * Atomic snapshot of the replicated match/phase state.
 * Grouping phase + timing into a single replicated struct guarantees the fields arrive together
 * with one OnRep, so clients never observe a new phase paired with a stale countdown end time.
 */
USTRUCT(BlueprintType)
struct FMKHMatchStateData
{
	GENERATED_BODY()

	/** The current match phase. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	EMKHMatchPhase Phase = EMKHMatchPhase::None;

	/** Server world time (seconds) at which the current phase's countdown ends; 0 when the phase has no countdown. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	float PhaseEndServerTime = 0.f;

	/** Total duration (seconds) of the current phase; 0 when the phase has no countdown. Used for progress bars. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	float PhaseDuration = 0.f;

	/**
	 * True once a preparation phase entered its final countdown (the last seconds before the duel, see
	 * AMKHGameMode::FinalCountdownDuration). Reset by every phase change.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	bool bFinalCountdown = false;

	/** Round being prepared or fought (1 for the first duel); 0 before the match starts. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 Round = 0;

	/**
	 * Incremented by every phase change. Lets clients tell a new phase from a countdown update of the
	 * same phase, which must not replay the phase events.
	 */
	UPROPERTY()
	int32 PhaseSerial = 0;
};

/**
 * Replicated data for a single duel participant (character reference + score): a player, or the bot of an
 * offline match. Held in a replicated array so every peer can render the live scoreboard.
 */
USTRUCT(BlueprintType)
struct FMKHDuelPlayerData
{
	GENERATED_BODY()

	/** The participant this entry refers to (implements IMKHDuelParticipant). */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	TObjectPtr<ACharacterBase> Participant = nullptr;

	/** Rounds won by this participant. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 Score = 0;
};

/**
 * Atomic snapshot of the post-match rematch ballot.
 * Both counters travel together so a client never renders "2/0" while the second field is stale.
 */
USTRUCT(BlueprintType)
struct FMKHRematchVoteData
{
	GENERATED_BODY()

	/** How many participants have asked for a rematch so far. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 CurrentVotes = 0;

	/** How many votes are needed before the match restarts; 0 while no ballot is open. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 RequiredVotes = 0;
};

/**
 * Atomic snapshot of the ready-up ballot that skips the rest of the initial preparation.
 * Holds the ready players rather than a counter, so each peer can tell whether its own player is ready.
 */
USTRUCT(BlueprintType)
struct FMKHPreparationSkipVoteData
{
	GENERATED_BODY()

	/** Participants currently ready to skip the preparation. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	TArray<TObjectPtr<APlayerState>> ReadyPlayers;

	/** Ready players needed to skip; 0 while the ballot is closed. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 RequiredVotes = 0;
};

// ============================================================
// Delegates (multicast, Blueprint-bindable — fire on every peer)
// ============================================================

/** Generic phase-change notification carrying the new phase. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMKHOnMatchPhaseChanged, EMKHMatchPhase, NewPhase);

/** Parameter-less phase notification reused by the granular per-phase events. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMKHDuelPhaseSignature);

/**
 * Broadcast when a participant is assigned to a scoreboard slot.
 * @param SlotIndex  The slot the participant occupies (0/1).
 * @param Character  The registered participant: a player character or a bot (see UMKHDuelParticipantLibrary).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMKHOnParticipantRegistered, int32, SlotIndex, ACharacterBase*, Character);

/**
 * Broadcast when a participant's score changes.
 * @param SlotIndex  The slot whose score changed (0/1).
 * @param NewScore   The updated score.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMKHOnScoreChanged, int32, SlotIndex, int32, NewScore);

/**
 * Broadcast on every peer when a participant dies and a point is awarded.
 * @param DeadPlayer      The participant that died (player or bot).
 * @param ScoringPlayer   The surviving participant that received the point (player or bot).
 * @param ScoringScore    The updated score of the scoring participant.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMKHOnPlayerDied, ACharacterBase*, DeadPlayer, ACharacterBase*, ScoringPlayer, int32, ScoringScore);

/**
 * Broadcast on every peer when the match ends.
 * @param WinningPlayer  The winning participant (player or bot), or nullptr on a draw.
 * @param bIsDraw        True when both participants reached the winning score together.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMKHOnMatchOver, ACharacterBase*, WinningPlayer, bool, bIsDraw);

/**
 * Broadcast on every peer whenever the rematch ballot changes.
 * @param CurrentVotes   Participants that have asked for a rematch.
 * @param RequiredVotes  Votes needed before the match restarts.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMKHOnRematchVotesChanged, int32, CurrentVotes, int32, RequiredVotes);

/**
 * Broadcast on every peer whenever the preparation-skip ballot changes (a vote cast or withdrawn, the ballot
 * opened or closed).
 * @param CurrentVotes   Participants currently ready.
 * @param RequiredVotes  Ready participants needed to skip; 0 once the ballot is closed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMKHOnPreparationSkipVotesChanged, int32, CurrentVotes, int32, RequiredVotes);

/**
 * Replicated state hub for the 1v1 arena duel.
 *
 * The (server-only) game mode owns the match logic and pushes state here; this game state
 * replicates that state to every client and exposes Blueprint-bindable events so UI on all peers
 * reacts to phase changes and score updates. The live countdown is driven from a replicated
 * server-time deadline, letting each client compute the remaining time locally every frame
 * (smooth, no per-frame replication).
 */
UCLASS()
class MAKHIA_API AMKHGameState : public AGameState
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	AMKHGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================================
	// Public Interface (Server authority — called by the game mode)
	// ============================================================

	/**
	 * Sets the current phase and (re)starts its countdown. Authority only.
	 * @param NewPhase             The phase to enter.
	 * @param PhaseDurationSeconds Countdown length; pass <= 0 for a phase without a countdown.
	 * @param Round                Round the phase belongs to (AMKHGameMode::GetCurrentRound).
	 */
	void SetMatchPhase(EMKHMatchPhase NewPhase, float PhaseDurationSeconds, int32 Round);

	/** Registers a participant (player or bot) in the given scoreboard slot (0/1) with a zeroed score. Authority only. */
	void RegisterParticipant(int32 SlotIndex, ACharacterBase* Character);

	/** Sets the score of the given scoreboard slot (0/1). Authority only. */
	void SetScore(int32 SlotIndex, int32 NewScore);

	/** Fires the "participant died" event on every peer. Authority only. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayerDied(ACharacterBase* DeadPlayer, ACharacterBase* ScoringPlayer, int32 ScoringScore);

	/** Fires the match-over event on every peer; WinningPlayer is null on a draw. Authority only. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MatchOver(ACharacterBase* WinningPlayer, bool bIsDraw);

	/**
	 * Publishes the current rematch ballot to every peer. Authority only.
	 *
	 * @param CurrentVotes   Participants that have asked for a rematch.
	 * @param RequiredVotes  Votes needed before the match restarts.
	 */
	void SetRematchVotes(int32 CurrentVotes, int32 RequiredVotes);

	/**
	 * Enters the final countdown of the current preparation phase: its countdown now ends in CountdownSeconds
	 * (the phase and its PhaseDuration are unchanged). Authority only.
	 *
	 * @param CountdownSeconds  Seconds left before the duel.
	 */
	void StartFinalCountdown(float CountdownSeconds);

	/**
	 * Publishes the preparation-skip ballot to every peer. Authority only.
	 *
	 * @param ReadyPlayers   Participants currently ready to skip.
	 * @param RequiredVotes  Ready participants needed to skip; 0 closes the ballot.
	 */
	void SetPreparationSkipVotes(const TArray<TObjectPtr<APlayerState>>& ReadyPlayers, int32 RequiredVotes);

	// ============================================================
	// Public Interface (Read-only queries — usable on any peer)
	// ============================================================

	/** Returns the current match phase. */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	EMKHMatchPhase GetMatchPhase() const { return DuelState.Phase; }

	/** Returns the whole duel state snapshot (phase + timing). */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	const FMKHMatchStateData& GetDuelState() const { return DuelState; }

	/** Returns the round being prepared or fought (1 for the first duel), 0 before the match starts. */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	int32 GetRound() const { return DuelState.Round; }

	/** Returns the live scoreboard. */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	const TArray<FMKHDuelPlayerData>& GetPlayerDatas() const { return PlayerDatas; }

	/** Returns the current rematch ballot (votes cast / votes needed). */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	const FMKHRematchVoteData& GetRematchVotes() const { return RematchVotes; }

	/** Returns the current preparation-skip ballot (ready players / votes needed). */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	const FMKHPreparationSkipVoteData& GetPreparationSkipVotes() const { return PreparationSkipVotes; }

	/** True while players can ready up to skip the initial preparation (closed during its final countdown). */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	bool IsPreparationSkipBallotOpen() const { return PreparationSkipVotes.RequiredVotes > 0; }

	/**
	 * True when the given player is currently ready to skip the initial preparation.
	 *
	 * @param PlayerState  The player to check, usually the local one.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	bool IsPlayerReadyToSkip(const APlayerState* PlayerState) const;

	/** True while a preparation phase is in its final countdown (the last seconds before the duel). */
	UFUNCTION(BlueprintPure, Category = "Duel|Timer")
	bool IsFinalCountdown() const { return DuelState.bFinalCountdown; }
	
	/** True while a duel is being fought (Duel or SuddenDeath phase). */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	bool IsDuelRunning() const { return DuelState.Phase == EMKHMatchPhase::Duel || DuelState.Phase == EMKHMatchPhase::SuddenDeath; }
	
	/**
	 * Re-fires OnParticipantRegistered and OnScoreChanged for every filled slot. Call it right after
	 * binding so a listener created after registration still receives the current state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Duel|Events")
	void BroadcastExistingPlayerDatas();

	/**
	 * Re-fires OnMatchPhaseChanged with the current phase (then OnFinalCountdownStarted if the final
	 * countdown is running) so a listener created after the phase was already set still reflects it.
	 * On the listen-server host the initial phase can be broadcast in the same tick the host is
	 * (re)started, before its HUD binds, so it would otherwise miss the transition; call this right
	 * after such a listener is put on screen. Safe on any peer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Duel|Events")
	void BroadcastCurrentPhase();

	/** True while the current phase has an active countdown. */
	UFUNCTION(BlueprintPure, Category = "Duel|Timer")
	bool IsPhaseTimerActive() const;

	/**
	 * Seconds remaining in the current phase, computed locally from the replicated server-time
	 * deadline. Call every frame from UI for a smooth real-time countdown. Returns 0 when no timer is active.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|Timer")
	float GetPhaseTimeRemaining() const;

	/** Normalized [0,1] elapsed fraction of the current phase, for progress bars. Returns 0 when no timer is active. */
	UFUNCTION(BlueprintPure, Category = "Duel|Timer")
	float GetPhaseElapsedFraction() const;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/** Broadcasts the generic and granular phase events for the current phase (runs on every peer). */
	void HandlePhaseChanged();

	/** Tells the friends subsystem who the human opponents of this machine are (platform recent players). */
	void ReportMatchOpponents() const;

	/**
	 * Replication callback: mirrors a server phase change onto clients. A countdown update of the same
	 * phase (same PhaseSerial) does not replay the phase events; entering the final countdown fires
	 * OnFinalCountdownStarted.
	 *
	 * @param OldDuelState  The snapshot this client held before the update.
	 */
	UFUNCTION()
	void OnRep_DuelState(const FMKHMatchStateData& OldDuelState);

	/** Replication callback: mirrors a server scoreboard change onto clients. */
	UFUNCTION()
	void OnRep_PlayerDatas();

	/** Replication callback: mirrors a server rematch-ballot change onto clients. */
	UFUNCTION()
	void OnRep_RematchVotes();

	/** Replication callback: mirrors a server preparation-skip ballot change onto clients. */
	UFUNCTION()
	void OnRep_PreparationSkipVotes();

	// ============================================================
	// Blueprint Events (bindable on every peer)
	// ============================================================

public:
	/** Fired on any phase change, carrying the new phase. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHOnMatchPhaseChanged OnMatchPhaseChanged;

	/** Fired when the initial equip preparation phase begins. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHDuelPhaseSignature OnInitialPreparationStarted;

	/** Fired when a duel phase begins (the first duel and every subsequent one). */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHDuelPhaseSignature OnDuelStarted;

	/** Fired when the duel timer expires and the sudden-death drain begins. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHDuelPhaseSignature OnDuelTimeOver;

	/** Fired when the intermission preparation phase between two duels begins. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHDuelPhaseSignature OnIntermissionPreparationStarted;

	/** Fired when a participant is registered into a scoreboard slot. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHOnParticipantRegistered OnParticipantRegistered;

	/** Fired when a participant's score changes. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHOnScoreChanged OnScoreChanged;

	/** Fired when a participant dies and a point is awarded to the survivor. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHOnPlayerDied OnPlayerDied;

	/** Fired when the match ends, with the winner or a draw flag. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHOnMatchOver OnMatchOver;

	/** Fired whenever the post-match rematch ballot changes. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHOnRematchVotesChanged OnRematchVotesChanged;

	/**
	 * Fired when a preparation phase enters its final countdown: its last seconds, or right away once
	 * both players are ready to skip the initial preparation.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHDuelPhaseSignature OnFinalCountdownStarted;

	/** Fired whenever the preparation-skip ballot changes. */
	UPROPERTY(BlueprintAssignable, Category = "Duel|Events")
	FMKHOnPreparationSkipVotesChanged OnPreparationSkipVotesChanged;

	// ============================================================
	// Properties (Replicated state)
	// ============================================================

protected:
	/** Replicated phase + countdown snapshot. */
	UPROPERTY(ReplicatedUsing = OnRep_DuelState, BlueprintReadOnly, Category = "Duel|State")
	FMKHMatchStateData DuelState;

	/** Replicated scoreboard (one entry per participant). */
	UPROPERTY(ReplicatedUsing = OnRep_PlayerDatas, BlueprintReadOnly, Category = "Duel|State")
	TArray<FMKHDuelPlayerData> PlayerDatas;

	/** Replicated rematch ballot, driven by the game mode once the match is over. */
	UPROPERTY(ReplicatedUsing = OnRep_RematchVotes, BlueprintReadOnly, Category = "Duel|State")
	FMKHRematchVoteData RematchVotes;

	/** Replicated ready-up ballot, open during the initial preparation until its final countdown. */
	UPROPERTY(ReplicatedUsing = OnRep_PreparationSkipVotes, BlueprintReadOnly, Category = "Duel|State")
	FMKHPreparationSkipVoteData PreparationSkipVotes;

	/** Last replicated snapshot, diffed in OnRep_PlayerDatas to tell registrations from score updates. */
	UPROPERTY()
	TArray<FMKHDuelPlayerData> PreviousPlayerDatas;
};
