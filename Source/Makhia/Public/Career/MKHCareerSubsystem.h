// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHCareerSubsystem.generated.h"

class UMKHCareerLadder;
struct FMKHCareerLevel;

DECLARE_MULTICAST_DELEGATE_OneParam(FMKHCareerLevelCompletedSignature, int32 /*LevelIndex*/);

/**
 * Career of this machine's player: the levels (UMKHDataSubsystem::GetCareerLadder) and how many were beaten, saved in
 * the "Career" slot. A level is playable once the previous one is beaten; beaten levels stay playable.
 * The progress lives only here, so moving it to Steam changes this class alone. Full model: see
 * Documentation/OfflineModes.md.
 */
UCLASS()
class MAKHIA_API UMKHCareerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the career subsystem of the game instance of a context object.
	 *
	 * @param WorldContextObject  Any object living in a world owned by a game instance.
	 * @return The subsystem, or null without a game instance.
	 */
	static UMKHCareerSubsystem* Get(const UObject* WorldContextObject);

	/** Returns the career levels, or null when none are configured. */
	const UMKHCareerLadder* GetLadder() const;

	/**
	 * Finds a level.
	 *
	 * @param LevelIndex  Index of the level, in play order.
	 * @return The level, or null when out of range. Do not store it.
	 */
	const FMKHCareerLevel* FindLevel(int32 LevelIndex) const;

	/** Returns the number of levels. */
	UFUNCTION(BlueprintPure, Category = "Career")
	int32 GetLevelCount() const;

	/** Returns how many levels were beaten, counted from the first one. */
	UFUNCTION(BlueprintPure, Category = "Career")
	int32 GetCompletedLevels() const { return CompletedLevels; }

	/**
	 * Returns the title of a player who beat a number of levels.
	 *
	 * @param InCompletedLevels  Levels beaten, counted from the first one (clamped to the existing ones).
	 * @return The title, empty without levels. Lobby nameplates use it for every player.
	 */
	UFUNCTION(BlueprintPure, Category = "Career")
	FText GetTitleForCompletedLevels(int32 InCompletedLevels) const;

	/**
	 * Tells whether a level can be played.
	 *
	 * @param LevelIndex  Index of the level.
	 * @return True when it exists and every level before it was beaten.
	 */
	UFUNCTION(BlueprintPure, Category = "Career")
	bool IsLevelUnlocked(int32 LevelIndex) const;

	/**
	 * Returns the level to play when none is chosen: the first one not beaten, the last one when all were.
	 *
	 * @return The index of the level, INDEX_NONE without levels.
	 */
	UFUNCTION(BlueprintPure, Category = "Career")
	int32 GetNextLevelIndex() const;

	/**
	 * Records a won level: saves the progress and broadcasts OnLevelCompleted the first time it is beaten.
	 *
	 * @param LevelIndex  Index of the won level.
	 */
	void RecordLevelWon(int32 LevelIndex);

	/** Broadcast when a level is beaten for the first time. Skins and titles will listen to this. */
	FMKHCareerLevelCompletedSignature OnLevelCompleted;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Reads the "Career" save slot. */
	void LoadProgress();

	/** Writes the progress to the "Career" save slot. */
	void SaveProgress() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Levels beaten so far, counted from the first one. */
	int32 CompletedLevels = 0;
};
