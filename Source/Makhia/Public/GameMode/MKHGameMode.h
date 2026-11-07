// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "AI/MKHDifficultyAdjustment.h"
#include "GameplayEffectTypes.h"
#include "GameMode/MKHGameState.h"
#include "GameMode/MKHMatchMode.h"
#include "Inventory/MKHStartingItem.h"
#include "MKHGameMode.generated.h"

class UCharacterClassInfo;
class UDataTable;
class UProjectileInfo;
class ACharacterBase;
class AMKHDuelBot;
class AMKHDuelBotController;
class UMKHDifficultyDirector;
class UMKHDuelBotProfile;
class AMKHGameState;
class APlayerState;
class UGameplayEffect;
class UInventoryComponent;
struct FGameplayEventData;

/**
 * Per-participant runtime data tracked by the game mode for the duration of the match.
 * Holds the participant reference (a player, or the bot of an offline match), its score and the spawn
 * transform it respawns at between rounds. Extend this struct with future per-participant match data
 * (streaks, stats, ...).
 *
 * This is the server-authoritative record; the replicated subset used by UI lives in the
 * game state (see FMKHDuelPlayerData).
 */
USTRUCT(BlueprintType)
struct FMKHPlayerData
{
	GENERATED_BODY()

	/** The character participating in the duel; implements IMKHDuelParticipant. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	TObjectPtr<ACharacterBase> Participant = nullptr;

	/** Number of rounds this participant has won so far. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 Score = 0;

	/** World transform this participant spawned at; used to reset position between rounds. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	FTransform SpawnTransform = FTransform::Identity;

	/** Start spot (usually a PlayerStart) this participant spawned at: identifies its side of the arena. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	TObjectPtr<AActor> StartSpot = nullptr;

	/** Handle of the sudden-death drain effect currently active on this participant; invalid when no drain is applied. */
	FActiveGameplayEffectHandle SuddenDeathDrainHandle;

	/** True once this participant's death has been counted in the current round; cleared on round reset. */
	bool bDiedThisRound = false;

	/** True once this slot has been assigned a participant. */
	bool HasParticipant() const { return Participant != nullptr; }
};

/**
 * Game mode for the 1v1 arena duel (server-authoritative orchestrator).
 *
 * The match mode comes from the travel URL (MKHMatchMode::OptionKey): an OnlineDuel waits for two players, an
 * OfflineBot match fills the second slot with a bot (BotClass) as soon as the player has spawned. Participants are
 * ACharacterBase implementing IMKHDuelParticipant; the rules are the same in both modes.
 *
 * Match flow:
 *   InitialPreparation (equip) -> Duel -> [player dies OR duel timer -> SuddenDeath drain -> player dies]
 *   -> award point -> (death-resolve delay) -> reset positions -> IntermissionPreparation -> Duel -> ...
 *   until a player reaches ScoreToWin.
 *   Each preparation ends with a final countdown; once both players are ready, the initial preparation
 *   jumps straight to it.
 *
 * The game mode owns the logic and timers; all replicated state and the Blueprint-bindable events
 * that UI on every peer reacts to live on AMKHGameState, which this class drives.
 */
UCLASS()
class MAKHIA_API AMKHGameMode : public AGameMode
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Sets up the two participant slots and the duel game state class. */
	AMKHGameMode();

	/**
	 * Reads the match mode from the travel URL options; without the option the match is an OnlineDuel.
	 *
	 * @param MapName       Name of the map being loaded.
	 * @param Options       Travel URL options, e.g. "?Mode=Bot".
	 * @param ErrorMessage  Set by the engine when the game cannot be initialized.
	 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

protected:
	/**
	 * Registers a participant once their pawn has been spawned and possessed,
	 * the first point where the controller's pawn is guaranteed valid on the server.
	 * In an offline match it then spawns the bot opponent.
	 */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/**
	 * Gives each player a PlayerStart no other controller or participant holds, so the two duelists start in
	 * opposite rooms. The engine picks the start at login/seamless travel, before any pawn exists, so its
	 * "unoccupied" test cannot tell the starts apart (see MatchFlowArchitecture.md, "Start spots").
	 *
	 * @param Player  Controller looking for a start spot.
	 * @return A free PlayerStart, or the engine's choice when none is free (or a Play-From-Here start in PIE).
	 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/**
	 * Gives a player the StartingLoadoutTable items through its inventory component. Runs from RestartPlayer and, after
	 * ResetPlayerLoadouts, from every rematch (the pawn is revived between rounds, never restarted), when the pawn
	 * exists for the equipment visuals.
	 *
	 * @param NewPlayer  Controller that just got its pawn; non-player controllers are ignored (the bot gets
	 *                   its profile's loadout from SpawnBotOpponent).
	 */
	void GrantStartingLoadout(AController* NewPlayer) const;

	/**
	 * Drops the leaving player's rematch vote so a stale ballot cannot restart the match, reports a remote
	 * player leaving (see ReportRemotePlayerLeft) and sends this machine back to the lobby (see ReturnToLobbyAfterLogout).
	 */
	virtual void Logout(AController* Exiting) override;

	/**
	 * Any logout ends the 1v1: frees the lobby seat and returns this machine to its lobby as host.
	 * Skipped while the world is tearing down (PIE stop, closing the game): the game instance subsystems are
	 * already gone and there is no lobby to return to.
	 */
	void ReturnToLobbyAfterLogout() const;

	/**
	 * Reports a remote player leaving to the loading screen subsystem: MATCH-OPPONENT-LEFT while the match is still
	 * loading, NET-CONNECTION-LOST during the match. Nothing when this machine is the one leaving or the match is over.
	 *
	 * @param ExitingPlayer  Player leaving the match.
	 */
	void ReportRemotePlayerLeft(const APlayerController* ExitingPlayer) const;

	// ============================================================
	// Public Interface (Class default data accessors)
	// ============================================================

public:
	/** Returns the character class default info data asset configured on this game mode. */
	UCharacterClassInfo* GetCharacterClassDefaultInfo() const;

	/** Returns the projectile info data asset configured on this game mode. */
	UProjectileInfo* GetProjectileInfo() const;

	// ============================================================
	// Public Interface (Match state queries)
	// ============================================================

	/** Returns the current round: 1 from the initial preparation, +1 at every intermission; 0 before the first match starts. */
	int32 GetCurrentRound() const { return CurrentRound; }

	/** Returns who the duel is played against, read from the travel URL in InitGame. */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	EMKHMatchMode GetMatchMode() const { return MatchMode; }

	/** Returns the offline mode and career level read from the travel URL in InitGame; only meaningful in an OfflineBot match. */
	UFUNCTION(BlueprintPure, Category = "Duel|State")
	const FMKHOfflineMatchRequest& GetOfflineRequest() const { return OfflineRequest; }

	/**
	 * Finds the duel participant who spawned at the given start spot (e.g. the PlayerStart of a preparation
	 * room). Used by item spawn points reserved to one side of the arena.
	 *
	 * @param InStartSpot The start spot, usually an APlayerStart placed in the level.
	 * @return The participant's player state, or nullptr when no registered participant started there
	 *         or when the participant is a bot (bots have no player state).
	 */
	APlayerState* FindParticipantAtStartSpot(const AActor* InStartSpot) const;

	/**
	 * Tells whether the side of the arena starting at the given start spot belongs to a bot. Item spawn points
	 * reserved to that side never spawn: nobody could collect their pickups.
	 *
	 * @param InStartSpot The start spot, usually an APlayerStart placed in the level.
	 * @return True when the participant registered at that start spot is not human.
	 */
	bool IsStartSpotHeldByBot(const AActor* InStartSpot) const;

	// ============================================================
	// Public Interface (Rematch ballot — called by player controllers)
	// ============================================================

	/**
	 * Records a rematch vote for the given player and restarts the match once every participant
	 * has voted. Votes are per-player, so pressing the button twice cannot restart a duel on its own.
	 * Ignored outside the MatchOver phase.
	 *
	 * @param Voter  The controller asking for a rematch.
	 */
	void RequestRematch(APlayerController* Voter);

	// ============================================================
	// Public Interface (Preparation skip ballot — called by player controllers)
	// ============================================================

	/**
	 * Toggles the given player's ready-to-skip vote on the initial preparation: a second press withdraws it.
	 * Once every participant is ready, the preparation jumps straight to its final countdown.
	 * Ignored while the ballot is closed (outside the initial preparation, or during its final countdown)
	 * and for players who are not duel participants.
	 *
	 * @param Voter  The controller toggling its vote.
	 */
	void ToggleSkipPreparationVote(APlayerController* Voter);

	// ============================================================
	// Protected / Internal Logic (Match flow)
	// ============================================================

protected:
	/**
	 * Registers a participant into the first free slot, caches its spawn transform and start spot and binds its
	 * death event. Ignores characters already registered and characters that are not IMKHDuelParticipant.
	 *
	 * @param Participant  The spawned character: a player's pawn or the bot.
	 * @param InStartSpot  The start spot it spawned at (identifies its side of the arena), may be null.
	 */
	void RegisterParticipant(ACharacterBase* Participant, AActor* InStartSpot);

	/**
	 * Offline match only: spawns BotClass on a start spot no participant took and registers it.
	 * Does nothing once a bot is registered.
	 */
	void SpawnBotOpponent();

	/**
	 * Spawns BotClass; a Training bot gets a plain AI controller instead of its own (no StateTree) and the training
	 * dummy component.
	 *
	 * @param SpawnTransform  Where the bot spawns.
	 * @return The bot, or nullptr when the spawn failed.
	 */
	AMKHDuelBot* SpawnBotActor(const FTransform& SpawnTransform) const;

	/**
	 * Gives the bot of the current offline mode its skin, difficulty profile and loadout.
	 *
	 * @param Bot  The bot just spawned.
	 */
	void ConfigureBot(AMKHDuelBot& Bot);

	/** Starts the match once both slots are filled (two players, or the player and the bot): enters the initial preparation phase. */
	void TryStartMatch();

	/** Enters the initial equip phase, opens the preparation-skip ballot and schedules the first duel. */
	void StartInitialPreparationPhase();

	/** Training: enters the duel phase with no preparation, no duel timer and no end. */
	void StartTrainingPhase();

	/** Enters the duel phase and schedules the sudden-death transition when the duel timer expires. */
	void StartDuelPhase();

	/** Enters the short intermission phase between duels and schedules the next duel. */
	void StartIntermissionPhase();

	/**
	 * Schedules the end of the preparation phase that just started: its final countdown begins
	 * FinalCountdownDuration before the duel (right away for a shorter phase).
	 *
	 * @param PhaseDurationSeconds  Full duration of the preparation phase.
	 */
	void SchedulePreparationEnd(float PhaseDurationSeconds);

	/**
	 * Enters the final countdown of the current preparation phase: closes the skip ballot, publishes the
	 * countdown and schedules the duel when it runs out. Never extends a phase already closer to its end.
	 * Reached on schedule, or early once every participant is ready to skip the initial preparation.
	 */
	void StartFinalCountdown();

	/** Applies the sudden-death drain effect to both players and moves the match into the sudden-death phase. */
	void HandleDuelTimeOver();

	/**
	 * Awards a point to the opposite participant on death (both deaths count on a double KO); the first
	 * death of the round stops the timers, removes the drain and schedules ResolveRound.
	 *
	 * @param Payload       The gameplay event payload (unused, required by the delegate signature).
	 * @param DeadCharacter The participant that triggered the death event.
	 */
	void HandlePlayerDeath(const FGameplayEventData* Payload, ACharacterBase* DeadCharacter);

	/**
	 * Sends Event.Combat.EnemyKilled to the participant that scored the death (kill confirmation sound effect).
	 *
	 * @param Killer         Participant awarded the point.
	 * @param DeadCharacter  Participant that died.
	 */
	void SendEnemyKilledEvent(ACharacterBase* Killer, ACharacterBase* DeadCharacter) const;

	/**
	 * Runs after the death-resolve delay. Ends the match once ScoreToWin is reached (equal scores
	 * mean a draw), otherwise resets both participants and starts the intermission.
	 */
	void ResolveRound();

	/**
	 * Starts a fresh match without leaving the level: wipes the scoreboard, cancels anything the
	 * previous match left running, revives both participants at their spawns and re-enters the
	 * initial preparation phase.
	 *
	 * Preferred over AGameMode::RestartGame for a rematch: no server travel means no reconnection,
	 * no reloaded level and no rebuilt HUD, so the transition is a plain phase change every peer
	 * already knows how to react to.
	 */
	void RestartMatchInPlace();

	// ============================================================
	// Protected / Internal Logic (Helpers)
	// ============================================================

protected:
	/**
	 * Sets the local authoritative phase and mirrors it (with its countdown) onto the replicated game state.
	 * Then notifies the item spawn points (UMKHItemSpawnSubsystem::HandleMatchPhaseChanged) and, entering
	 * MatchOver, destroys every world pickup (UMKHItemSubsystem::DestroyAllPickups), in this order.
	 */
	void SetPhase(EMKHMatchPhase NewPhase, float PhaseDurationSeconds);

	/** Returns the typed duel game state, or nullptr if it is not available yet. */
	AMKHGameState* GetDuelGameState() const;

	/**
	 * Gets a player's inventory component through IInventoryInterface.
	 *
	 * @param Controller Controller to query; non-player controllers return nullptr.
	 * @return The inventory, or nullptr (logged when the controller should have one).
	 */
	UInventoryComponent* FindPlayerInventory(AController* Controller) const;

	/**
	 * Rematch: empties every player's inventory, equipment and quick slots (UInventoryComponent::ResetInventory),
	 * then grants the StartingLoadoutTable items again, so no loot of the previous match carries over. The offline bot gets its
	 * profile's loadout again the same way (consumables drunk in the previous match come back).
	 */
	void ResetPlayerLoadouts() const;

	/** Binds the game mode's death handler to the given participant's ability system death event. */
	void BindDeathEvent(ACharacterBase* Character);

	/** Clears any pending phase timer so a new phase can be scheduled cleanly. */
	void ClearPhaseTimer();

	/**
	 * Restores both participants for a fresh duel: round-reset effect, then IMKHDuelParticipant::ResetForNewRound
	 * (back to the spawn transform, revived).
	 */
	void ResetParticipantsForNewRound();

	/** Applies the configured round-reset gameplay effect to a single participant, if one is set. */
	void ApplyRoundResetEffect(ACharacterBase* Character) const;

	/**
	 * Applies the sudden-death drain effect to a participant and stores the active-effect handle
	 * in the player data for later removal.
	 */
	void ApplySuddenDeathDrain(FMKHPlayerData& PlayerData) const;

	/** Removes the active sudden-death drain from every participant and invalidates the stored handles. */
	void RemoveSuddenDeathDrainFromParticipants();

	/** Returns the index (0/1) of the slot holding the given character, or INDEX_NONE if not a participant. */
	int32 FindParticipantIndex(const ACharacterBase* Character) const;

	/**
	 * Finds the slot of the participant registered at the given start spot.
	 *
	 * @param InStartSpot The start spot to look for.
	 * @return The participant's data, or nullptr when no registered participant started there.
	 */
	const FMKHPlayerData* FindParticipantDataAtStartSpot(const AActor* InStartSpot) const;

	/**
	 * Tells whether a start spot is already taken: another controller holds it as its StartSpot, or a registered
	 * participant (the bot included) started there.
	 *
	 * @param InStartSpot  The start spot to check.
	 * @param Asking       The controller asking, ignored among the holders.
	 * @return True when someone else holds the start.
	 */
	bool IsStartSpotTaken(const AActor* InStartSpot, const AController* Asking) const;

	/**
	 * Picks where the bot spawns: a PlayerStart no participant took, or any PlayerStart when all are taken.
	 *
	 * @return The start spot, or nullptr when the level has no PlayerStart.
	 */
	AActor* FindFreeStartSpotForBot() const;

	/**
	 * Finds the bot profile whose ProfileId matches, among BotProfiles.
	 *
	 * @param ProfileId  Id read from the BotProfile URL option; None when the option is missing.
	 * @return The matching profile, or nullptr (the bot controller then uses its DefaultProfile).
	 */
	UMKHDuelBotProfile* FindBotProfile(FName ProfileId) const;

	/**
	 * Reads the offline mode and career level from the travel URL; a career level still locked becomes the next one to play.
	 *
	 * @param Options  Travel URL options.
	 */
	void ReadOfflineRequest(const FString& Options);

	/**
	 * Chooses the weapons the bot quick slots in the current offline mode.
	 *
	 * @param Loadout  The bot's loadout, to pick random weapons from.
	 * @return Weapon tags in quick slot order: the dummy's weapon, the career level's pair, or two random weapons.
	 */
	TArray<FGameplayTag> ResolveBotWeapons(const TArray<FMKHStartingItem>& Loadout) const;

	/** @return True in an offline match of the Training mode. */
	bool IsTrainingMatch() const;

	/** @return The profile the bot starts with in the current offline mode; None means the controller's default. */
	FName ResolveBotProfileId() const;

	/** @return The career levels of the bot's title in the current offline mode; INDEX_NONE for the training dummy. */
	int32 ResolveBotTitleCareerLevels() const;

	/** @return The skin the bot wears in the current offline mode. */
	FName ResolveBotSkinId() const;

	/** @return A random skin different from the one this machine's player wears. */
	FName PickRandomBotSkin() const;

	/** @return The spot of the training dummy (an actor tagged TrainingDummySpotTag), or a free start when the level has none (logged). */
	AActor* FindTrainingDummySpot() const;

	/** @return The controller of the registered bot, or nullptr. */
	AMKHDuelBotController* FindBotController() const;

	/** Random Duel without a forced profile: creates the difficulty director for the two registered participants. */
	void StartDifficultyTracking();

	/** Random Duel: closes the round on the difficulty director and hands the bot the profile of the next round. */
	void ApplyDifficultyForNextRound();

	/** Random Duel: gives the bot a new random pair among the weapons it owns, for the next round. */
	void ApplyWeaponsForNextRound() const;

	/** Rematch of a Random Duel: back to the starting difficulty, before the bot gets its loadout again. */
	void ResetDifficultyForRematch();

	/**
	 * Career: records the level when the human participant won the match.
	 *
	 * @param Winner  Winner of the match, null on a draw.
	 */
	void RecordCareerResult(const ACharacterBase* Winner) const;

	/**
	 * Reads the bot's loadout (active difficulty profile, or the dummy's table in Training).
	 *
	 * @param Bot  Bot whose controller holds the profile (set by SpawnBotOpponent).
	 * @return The items to grant; empty without a controller or a profile (logged).
	 */
	TArray<FMKHStartingItem> BuildBotLoadout(const AMKHDuelBot& Bot) const;

	/**
	 * Chooses the weapons the bot starts with: those of the current offline mode (ResolveBotWeapons).
	 *
	 * @param Loadout  The bot's loadout.
	 * @return Weapon tags in quick slot order; empty leaves the choice to the default rule (online, forced ?BotProfile=).
	 */
	TArray<FGameplayTag> ChooseBotStartingWeapons(const TArray<FMKHStartingItem>& Loadout) const;

	/**
	 * Number of votes the rematch ballot needs: one per connected player.
	 * Recomputed on every vote so a disconnect cannot leave the ballot unreachable.
	 */
	int32 CountRematchVotersNeeded() const;

	/** Pushes the current ballot onto the game state so every peer's panel can render it. */
	void PublishRematchVotes() const;

	/** Number of registered human participants (2 online, 1 against the bot): the preparation-skip ballot target. */
	int32 CountHumanParticipants() const;

	/**
	 * Opens or closes the preparation-skip ballot with no vote cast, and publishes it.
	 *
	 * @param bOpen  True to open the ballot, false to close it.
	 */
	void ResetSkipPreparationBallot(bool bOpen);

	/** Pushes the preparation-skip ballot onto the game state so every peer's widget can render it. */
	void PublishSkipPreparationVotes() const;

	// ============================================================
	// Properties (Designer-tunable match rules)
	// ============================================================

protected:
	/**
	 * Items every player receives when the match starts, one per row in loadout order (FMKHStartingLoadoutRow).
	 * See Documentation/MatchFlowArchitecture.md.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Loadout", meta = (
		RequiredAssetDataTags = "RowStructure=/Script/Makhia.MKHStartingLoadoutRow"))
	TObjectPtr<UDataTable> StartingLoadoutTable;

	/** Duration in seconds of the initial equip preparation phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Duel|Rules", meta = (ClampMin = "0.0"))
	float InitialPreparationDuration = 60.f;

	/**
	 * Last seconds of every preparation phase: the ready-up ballot closes and the final countdown message
	 * shows. Once both players are ready, the initial preparation is cut down to this.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Duel|Rules", meta = (ClampMin = "0.0"))
	float FinalCountdownDuration = 3.f;

	/** Maximum duration in seconds of a duel before sudden death triggers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Duel|Rules", meta = (ClampMin = "0.0"))
	float DuelDuration = 180.f;

	/** Duration in seconds of the short intermission preparation phase between duels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Duel|Rules", meta = (ClampMin = "0.0"))
	float IntermissionPreparationDuration = 10.f;

	/** Delay in seconds between a death and the round reset, giving the death animation time to play. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Duel|Rules", meta = (ClampMin = "0.0"))
	float DeathResolveDelay = 3.f;

	/** Score a participant must reach to win the match. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Duel|Rules", meta = (ClampMin = "1"))
	int32 ScoreToWin = 3;

	/**
	 * Health-drain gameplay effect applied to both players when the duel timer expires.
	 * Should reduce health over time until one participant inevitably dies.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Effects")
	TSubclassOf<UGameplayEffect> SuddenDeathDrainEffect;

	/**
	 * Gameplay effect applied to each participant when reset for a new round.
	 * Typically restores health/stamina/shield to full so the next duel starts fresh.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Effects")
	TSubclassOf<UGameplayEffect> RoundResetEffect;

	/** Opponent spawned in an offline match (EMKHMatchMode::OfflineBot). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Bot")
	TSubclassOf<AMKHDuelBot> BotClass;

	/** Difficulty profiles the bot can use, picked by ProfileId through the URL option MKHMatchMode::BotProfileOptionKey. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Bot")
	TArray<TObjectPtr<UMKHDuelBotProfile>> BotProfiles;

	/** Rules of the Random Duel's difficulty adjustment (see Documentation/OfflineModes.md). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Bot")
	FMKHDifficultyAdjustmentSettings DifficultySettings;

	/** Career levels of the title of the Random Duel's bot, whatever its difficulty: 2 is Gladiatore. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Bot", meta = (ClampMin = "0"))
	int32 RandomDuelTitleCareerLevels = 2;

	/** Actor tag of the spot of the training dummy, placed in the centre of the arena. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Training")
	FName TrainingDummySpotTag = TEXT("TrainingDummySpot");

	/** Items of the training dummy (armor equipped, weapons in the bag): DT_MKHStartingLoadout_AllCommon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Training", meta = (
		RequiredAssetDataTags = "RowStructure=/Script/Makhia.MKHStartingLoadoutRow"))
	TObjectPtr<UDataTable> TrainingDummyLoadoutTable;

	/** Weapon the training dummy holds, matched against TrainingDummyLoadoutTable: Item.Equipment.Weapon.Sword. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Training", meta = (Categories = "Item.Equipment.Weapon"))
	FGameplayTag TrainingDummyWeapon;

	/** Items the player receives in Training, instead of StartingLoadoutTable (which is used when this is not set). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Training", meta = (
		RequiredAssetDataTags = "RowStructure=/Script/Makhia.MKHStartingLoadoutRow"))
	TObjectPtr<UDataTable> TrainingLoadoutTable;

	// ============================================================
	// Properties (Runtime state)
	// ============================================================

protected:
	/** The two duel participants and their per-match data (index 0 and 1). */
	UPROPERTY(BlueprintReadOnly, Category = "Duel|State")
	TArray<FMKHPlayerData> Players;

	/** Who the duel is played against; set once by InitGame from the travel URL. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Duel|State")
	EMKHMatchMode MatchMode = EMKHMatchMode::OnlineDuel;

	/** Offline mode and career level of an OfflineBot match; set once by InitGame from the travel URL. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Duel|State")
	FMKHOfflineMatchRequest OfflineRequest;

	/** Measures the rounds of a Random Duel and picks the next difficulty; null in every other match. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHDifficultyDirector> DifficultyDirector;

	/** Bot profile id requested by the travel URL (?BotProfile=), None when absent; set once by InitGame. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Duel|State")
	FName RequestedBotProfileId;

	/** Authoritative current match phase (mirrored to the game state for replication). */
	UPROPERTY(BlueprintReadOnly, Category = "Duel|State")
	EMKHMatchPhase MatchPhase = EMKHMatchPhase::None;

	/** Current round: set to 1 by the initial preparation, incremented by every intermission; 0 before the first match. */
	UPROPERTY(BlueprintReadOnly, Category = "Duel|State")
	int32 CurrentRound = 0;

	/**
	 * Controllers that have asked for a rematch since the match ended.
	 * A set rather than a counter: one player spamming the button must not restart the duel alone.
	 */
	UPROPERTY()
	TSet<TObjectPtr<APlayerController>> RematchVoters;

	/** Participants currently ready to skip the rest of the initial preparation (a vote can be withdrawn). */
	UPROPERTY()
	TSet<TObjectPtr<APlayerState>> SkipPreparationVoters;

	/** True while the preparation-skip ballot is open: from the start of the initial preparation to its final countdown. */
	bool bSkipPreparationBallotOpen = false;

	/** True while a death is being resolved (between the death and the round reset); blocks re-entrant death handling. */
	bool bRoundResolving = false;

	/** Timer handle driving the current phase's duration (preparations: up to and through their final countdown) and the death-resolve delay. */
	FTimerHandle PhaseTimerHandle;

	// ============================================================
	// Properties (Class defaults data assets)
	// ============================================================

private:
	/** Starting abilities, passives and default attributes per character class tag (players and bots). */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Class Defaults")
	TObjectPtr<UCharacterClassInfo> ClassDefaults;

	/** Projectile definitions used by projectile abilities. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Projectiles")
	TObjectPtr<UProjectileInfo> ProjectileInfo;
};
