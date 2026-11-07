// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "AI/Data/MKHDuelBotScoring.h"
#include "Interfaces/EquipmentInterface.h"
#include "Interfaces/InventoryInterface.h"
#include "Interfaces/QuickSlotInterface.h"
#include "MKHDuelBotController.generated.h"

class UDataTable;
class ACharacterBase;
class UEquipmentManagerComponent;
class UInventoryComponent;
class UQuickSlotManagerComponent;
class UMKHDuelBotStateTreeComponent;

/**
 * Brain of the offline duel bot (AMKHDuelBot), server only. On possess it picks the human opponent and starts its
 * StateTree (ST_DuelBot, set on BP_DuelBotController), which approaches, attacks and repositions; the tree reads the
 * opponent through the duel opponent evaluator.
 *
 * Learning plan and decisions: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
UCLASS()
class MAKHIA_API AMKHDuelBotController : public AAIController, public IEquipmentInterface, public IInventoryInterface,
	public IQuickSlotInterface
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Creates the StateTree component; its logic is started on possess, not on BeginPlay. */
	AMKHDuelBotController();

protected:
	/**
	 * Finds the opponent and starts the StateTree.
	 *
	 * @param InPawn  The bot pawn being possessed.
	 */
	virtual void OnPossess(APawn* InPawn) override;

	/** Stops the StateTree. */
	virtual void OnUnPossess() override;

	/**
	 * Refreshes the effective tuning and scoring (profile, scoring row and session overrides) and prints the
	 * StateTree's active states on screen when they change, if bPrintStateTreeChanges is set.
	 *
	 * @param DeltaSeconds  Seconds since the last tick.
	 */
	virtual void Tick(float DeltaSeconds) override;

	// ============================================================
	// Public Interface
	// ============================================================

public:
	/** @return The character the bot fights, or null before one is found. */
	UFUNCTION(BlueprintPure, Category = "Duel|AI")
	ACharacterBase* GetOpponent() const;

	/**
	 * Tells whether a character is dead, for players and bot alike.
	 *
	 * @param Character  Character to check.
	 * @return True when its ability system carries State.Movement.Dead.
	 */
	static bool IsCharacterDead(const ACharacterBase* Character);

	/**
	 * IEquipmentInterface: the project reaches a character's equipment through its controller (damage calculation,
	 * weapon abilities); the bot keeps it on the possessed AMKHDuelBot.
	 *
	 * @return The bot's equipment component, or null without a possessed bot.
	 */
	virtual UEquipmentManagerComponent* GetEquipmentManagerComponent_Implementation() const override;

	/**
	 * IInventoryInterface: GA_QuickSlot reaches the inventory through the avatar's controller, like the player's; the
	 * bot keeps it on the possessed AMKHDuelBot.
	 *
	 * @return The bot's inventory, or null without a possessed bot.
	 */
	virtual UInventoryComponent* GetInventoryComponent_Implementation() const override;

	/**
	 * IQuickSlotInterface: see GetInventoryComponent_Implementation.
	 *
	 * @return The bot's quick slots, or null without a possessed bot.
	 */
	virtual UQuickSlotManagerComponent* GetQuickSlotManagerComponent_Implementation() const override;

	/**
	 * Behaviour parameters of the active difficulty profile with the session overrides, read every tick by the
	 * Context evaluator.
	 *
	 * @return The effective tuning (default tuning when no profile is set).
	 */
	const FMKHDuelBotTuning& GetTuning() const { return EffectiveTuning; }

	/**
	 * Generic scoring parameters: the profile's DT_DuelBotScoring row, re-read every tick (DataTable edits in PIE apply
	 * at once), with the session overrides. Read by the Perception, Attack Options and Decisions evaluators.
	 *
	 * @return The effective scoring (defaults when the profile has no table).
	 */
	const FMKHDuelBotScoringRow& GetScoring() const { return EffectiveScoring; }

	/**
	 * Debug (MKH.Bot.Tuning.Set): overrides one field of the tuning or of the scoring row for this session, by
	 * reflection (e.g. "Aggression", "States.Bait.ChaseMemory", "AttackScoring.RepeatScore"). Tuning fields are
	 * tried first.
	 *
	 * @param Path      Field path, dot separated, case insensitive.
	 * @param Value     Value in the property's text format (e.g. 0.5, (Min=0.2,Max=0.3)).
	 * @param OutError  Why the override was refused.
	 * @return True when the field was found and the value imported.
	 */
	bool SetParameterOverride(const FString& Path, const FString& Value, FString& OutError);

	/** Debug: drops every session override (MKH.Bot.Tuning.Reset). */
	void ClearParameterOverrides();

	/** Debug: logs the effective tuning and scoring and the overrides (MKH.Bot.Tuning.Dump). */
	void DumpParameters() const;

	/**
	 * Tells whether a Combat or React state may be selected (MKH Bot State Enabled condition).
	 *
	 * @param StateTag  AI.State.* tag of the state.
	 * @return False when disabled with MKH.Bot.State.Disable / Only.
	 */
	bool IsStateEnabled(const FGameplayTag& StateTag) const { return !DisabledStates.HasTagExact(StateTag); }

	/**
	 * Debug: enables or disables states for the session.
	 *
	 * @param StateTags  AI.State.* tags.
	 * @param bEnabled   True to enable them.
	 */
	void SetStatesEnabled(const FGameplayTagContainer& StateTags, bool bEnabled);

	/**
	 * Debug: disables every selectable state but StateTags (MKH.Bot.State.Only). While any state is disabled the
	 * Combat > Idle fallback (AI.State.Idle) is enabled, so Combat always has a state to select.
	 */
	void SetOnlyStatesEnabled(const FGameplayTagContainer& StateTags);

	/** @return The states disabled for the session. */
	const FGameplayTagContainer& GetDisabledStates() const { return DisabledStates; }

	/**
	 * Debug (MKH.Bot.Pause / Resume): pauses the StateTree, stops the movement and releases every held input; on
	 * resume the tree continues where it was.
	 *
	 * @param bPause  True to pause.
	 */
	void SetBotPaused(bool bPause);

	/** @return True while paused with SetBotPaused. */
	bool IsBotPaused() const { return bBotPaused; }

	/** Called by the Commit Attack task when the bot commits to an attack (chase time of the Bait score). */
	void NotifyAttackCommitted();

	/** @return World time of the last attack commit; far in the past before any. */
	float GetLastAttackCommitTime() const { return LastAttackCommitTime; }

	/**
	 * Perception's reading of the incoming attack, for the Decisions evaluator of the same tick.
	 *
	 * @param InStrike  Next strike of the recognised attack.
	 */
	void SetIncomingStrike(const FMKHIncomingStrike& InStrike) { IncomingStrike = InStrike; }

	/** @return Next strike of the opponent's recognised attack. */
	const FMKHIncomingStrike& GetIncomingStrike() const { return IncomingStrike; }

	/**
	 * Stores the bait roll of the opponent's current attack.
	 *
	 * @param bWantsCounter  True when the bot should answer this attack with an attack.
	 */
	void SetBaitWantsCounter(const bool bWantsCounter) { bBaitWantsCounter = bWantsCounter; }

	/** @return True when the bait roll of the opponent's current attack asks for an attack. */
	bool DoesBaitWantCounter() const { return bBaitWantsCounter; }

	/**
	 * Sets the difficulty profile the bot plays with (called by AMKHGameMode right after the spawn).
	 *
	 * @param NewProfile  Profile to use; null falls back to DefaultProfile.
	 */
	void SetProfile(UMKHDuelBotProfile* NewProfile);

	/** @return The profile the bot plays with (DefaultProfile included), or null before SetProfile. */
	UMKHDuelBotProfile* GetActiveProfile() const { return ActiveProfile; }

	/** Debug: asks the Attack Options evaluator to print its candidate table on its next tick (MKH.Bot.DumpAttackOptions). */
	void RequestAttackOptionsDump() { bAttackOptionsDumpRequested = true; }

	/**
	 * Debug: read by the Attack Options evaluator every tick.
	 *
	 * @return True once after RequestAttackOptionsDump; the request is cleared.
	 */
	bool ConsumeAttackOptionsDumpRequest() { return std::exchange(bAttackOptionsDumpRequested, false); }

	/** Debug: asks the Decisions evaluator to print the state scores on its next tick (MKH.Bot.DumpAttackOptions). */
	void RequestStateScoresDump() { bStateScoresDumpRequested = true; }

	/**
	 * Debug: read by the Decisions evaluator every tick.
	 *
	 * @return True once after RequestStateScoresDump; the request is cleared.
	 */
	bool ConsumeStateScoresDumpRequest() { return std::exchange(bStateScoresDumpRequested, false); }
	
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Logs how the last move request ended (reached, blocked, aborted...).
	 *
	 * @param RequestID  Id of the finished move request.
	 * @param Result     Outcome of the move.
	 */
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

private:
	/**
	 * Picks the first human duel participant other than the possessed pawn as Opponent and focuses it, so the control
	 * rotation (used by the abilities to aim) always points at the opponent.
	 */
	void AcquireOpponent();

	/** Rebuilds EffectiveTuning and EffectiveScoring from the active profile, its scoring row and the overrides. */
	void RefreshEffectiveParameters();

	/**
	 * Logs a DataTable edit of the active scoring table (the next tick reads the new values).
	 */
	void HandleScoringTableChanged();

	/** Safety net: restarts the StateTree (at most once per TreeRestartInterval) if it stopped while a pawn is possessed. */
	void RestartStoppedTree();

	/** Releases every input the bot holds on its ability system (SetBotPaused). */
	void ReleaseHeldInputs() const;

	/**
	 * Debug: compares the StateTree's active states with the last ones seen and, on a change, prints the new path
	 * (e.g. "Root > Combat > React > Dodge") on screen and in LogMKHAI. Development builds only.
	 */
	void PrintStateTreeChanges();

	// ============================================================
	// Properties
	// ============================================================

	/** Runs the bot's StateTree (AI component schema, evaluators-only tick while paused); the asset is set on BP_DuelBotController. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Duel|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMKHDuelBotStateTreeComponent> StateTreeComponent;

	/** Character the bot fights; weak because the controller does not own it. */
	UPROPERTY()
	TWeakObjectPtr<ACharacterBase> Opponent;

	/** Difficulty profile the bot plays with, set by SetProfile; null until the game mode hands one. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel|AI|Profile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMKHDuelBotProfile> ActiveProfile;

	/** Profile used when the travel URL asks for none or for an unknown one (set on BP_DuelBotController). */
	UPROPERTY(EditDefaultsOnly, Category = "Duel|AI|Profile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMKHDuelBotProfile> DefaultProfile;

public:
	/** Debug: prints on screen every state the StateTree enters (development builds only, not in Shipping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel|AI|Debug")
	bool bPrintStateTreeChanges = false;

	/** Debug: seconds each printed state change stays on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel|AI|Debug", meta = (ClampMin = "0.1", Units = "s", EditCondition = "bPrintStateTreeChanges", EditConditionHides))
	float StateChangePrintDuration = 3.f;

private:
	/** Profile tuning with the session overrides; read by the tree every tick. */
	FMKHDuelBotTuning EffectiveTuning;

	/** Scoring row with the session overrides; holds curves (external curve assets), so it is a property. */
	UPROPERTY(Transient)
	FMKHDuelBotScoringRow EffectiveScoring;

	/** Session overrides of the tuning: field path and value text, applied in order. */
	TArray<TPair<FString, FString>> TuningOverrides;

	/** Session overrides of the scoring row. */
	TArray<TPair<FString, FString>> ScoringOverrides;

	/** Scoring table whose OnDataTableChanged is bound. */
	TWeakObjectPtr<UDataTable> BoundScoringTable;

	/** States disabled for the session (AI.State.*). */
	FGameplayTagContainer DisabledStates;

	/** True while paused by SetBotPaused. */
	bool bBotPaused = false;

	/** World time of the last attack commit. */
	float LastAttackCommitTime = -1000.f;

	/** Next strike of the opponent's recognised attack. */
	FMKHIncomingStrike IncomingStrike;

	/** Bait roll of the opponent's current attack. */
	bool bBaitWantsCounter = false;

	/** World time of the last RestartStoppedTree restart. */
	float LastTreeRestartTime = -1000.f;

	/** Seconds between two restarts of a stopped StateTree. */
	static constexpr float TreeRestartInterval = 1.f;

	/** Active state path printed last, to print only the changes. */
	FString LastPrintedStatePath;

	/** Debug: set by RequestAttackOptionsDump until the Attack Options evaluator prints its table. */
	bool bAttackOptionsDumpRequested = false;

	/** Debug: set by RequestStateScoresDump until the Decisions evaluator prints the state scores. */
	bool bStateScoresDumpRequested = false;
};
