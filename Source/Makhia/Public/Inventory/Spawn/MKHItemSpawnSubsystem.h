// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/MKHGameState.h"
#include "Subsystems/WorldSubsystem.h"
#include "MKHItemSpawnSubsystem.generated.h"

class AMKHItemSpawnGroup;
class AMKHItemSpawnPoint;

/**
 * Server-side orchestrator of the item spawn points of a world. AMKHGameMode::SetPhase forwards every match phase
 * change here (before the MatchOver pickup cleanup); the subsystem then resets the points at match start, clears the
 * pickups that must not survive a round, stops everything at match end, and starts the attempts of the points
 * triggered by the new phase, letting a random subset of each spawn group spawn up to its free slots.
 *
 * Full model: see Documentation/ItemSpawnSystemArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHItemSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Exists only in game and PIE worlds (spawn points never run in editor worlds). */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Convenience accessor for C++ callers.
	 *
	 * @param WorldContextObject Any object living in the world.
	 * @return The spawn subsystem of that world, or nullptr.
	 */
	static UMKHItemSpawnSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Starts driving a spawn point. Called by AMKHItemSpawnPoint::BeginPlay on the server.
	 *
	 * @param SpawnPoint The spawn point to register.
	 */
	void RegisterSpawnPoint(AMKHItemSpawnPoint* SpawnPoint);

	/**
	 * Stops driving a spawn point. Called by AMKHItemSpawnPoint::EndPlay.
	 *
	 * @param SpawnPoint The spawn point to forget.
	 */
	void UnregisterSpawnPoint(AMKHItemSpawnPoint* SpawnPoint);

	/**
	 * Server: reacts to a match phase change (called by AMKHGameMode::SetPhase).
	 * InitialPreparation resets every point; IntermissionPreparation ends the round (points not keeping their
	 * pickup clear it); MatchOver and None stop every point. Then the points triggered by the phase attempt to spawn.
	 *
	 * @param NewPhase The phase the match just entered.
	 * @param Round    The current round (AMKHGameMode::GetCurrentRound).
	 */
	void HandleMatchPhaseChanged(EMKHMatchPhase NewPhase, int32 Round);

	/** Whether a match is running (from the initial preparation to sudden death): spawn attempts only happen then. */
	UFUNCTION(BlueprintPure, Category = "Items|Spawn")
	bool IsMatchRunning() const;

	/** Gets the current round as last reported by the game mode (0 before the first match). */
	UFUNCTION(BlueprintPure, Category = "Items|Spawn")
	int32 GetCurrentRound() const { return CurrentRound; }

	/**
	 * Whether a spawn point may spawn now as far as its group is concerned: always true without a group, otherwise
	 * the live pickups of the other members must leave a free slot. Pending attempts of other members do not count
	 * here (attempts run one at a time, so counting them would only let two waiting members block each other).
	 *
	 * @param SpawnPoint The spawn point about to spawn.
	 * @return True when a slot is free for it.
	 */
	bool HasGroupCapacity(const AMKHItemSpawnPoint& SpawnPoint) const;

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns the registered spawn points still alive, dropping the dead entries of the registry. */
	TArray<AMKHItemSpawnPoint*> GetLiveSpawnPoints();

	/**
	 * Starts the attempts of the points triggered by a phase: ungrouped points directly, grouped ones through
	 * StartGroupAttempts.
	 *
	 * @param Phase     The phase the match just entered.
	 * @param Points    Live spawn points.
	 */
	void TriggerPhaseSpawns(EMKHMatchPhase Phase, const TArray<AMKHItemSpawnPoint*>& Points);

	/**
	 * Lets a random subset of the triggered members of a group start an attempt, up to its free slots.
	 *
	 * @param Group     The spawn group.
	 * @param Triggered Members triggered by the phase; shuffled in place.
	 */
	void StartGroupAttempts(const AMKHItemSpawnGroup& Group, TArray<AMKHItemSpawnPoint*>& Triggered);

	/**
	 * Counts the members of a group holding a pickup (and, optionally, about to spawn one).
	 *
	 * @param Group                 The spawn group.
	 * @param Excluded              A member not to count (the one asking for a slot), or nullptr.
	 * @param bCountPendingAttempts Whether members with a scheduled attempt count as occupied (trigger-time selection).
	 * @return The number of occupied members.
	 */
	int32 CountOccupiedMembers(const AMKHItemSpawnGroup& Group, const AMKHItemSpawnPoint* Excluded, bool bCountPendingAttempts) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Spawn points registered on the server; weak so points removed with their level just expire. */
	TArray<TWeakObjectPtr<AMKHItemSpawnPoint>> SpawnPoints;

	/** Last match phase reported by the game mode. */
	EMKHMatchPhase CurrentPhase = EMKHMatchPhase::None;

	/** Last round reported by the game mode. */
	int32 CurrentRound = 0;
};
