// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/MKHGameState.h"
#include "UObject/Object.h"
#include "MKHItemSpawnConditions.generated.h"

class AMKHItemSpawnPoint;
class FDataValidationContext;

/**
 * A rule an item spawn point must satisfy before spawning, edited inline in the spawn point's Conditions.
 * Extend it in C++ (IsSatisfied_Implementation) or in Blueprint (override IsSatisfied); Blueprint conditions can
 * use world nodes, the world being the one of the owning spawn point. Evaluated on the server only.
 *
 * Full model: see Documentation/ItemSpawnSystemArchitecture.md.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class MAKHIA_API UMKHItemSpawnCondition : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Evaluates the condition for a spawn point, applying bNegate.
	 *
	 * @param SpawnPoint The spawn point about to spawn.
	 * @return True when the spawn point may spawn as far as this condition is concerned.
	 */
	bool Evaluate(const AMKHItemSpawnPoint& SpawnPoint) const;

	/** Returns the world of the owning spawn point, so Blueprint conditions can use world-dependent nodes. */
	virtual UWorld* GetWorld() const override;

#if WITH_EDITOR
	/**
	 * Editor validation of this condition in the context of the spawn point holding it.
	 *
	 * @param SpawnPoint The spawn point holding this condition.
	 * @param Context    Receives the errors and warnings.
	 * @return Invalid when this condition can never work on that spawn point.
	 */
	virtual EDataValidationResult ValidateForSpawnPoint(const AMKHItemSpawnPoint& SpawnPoint, FDataValidationContext& Context) const;
#endif

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Returns whether the condition currently holds for the spawn point (before bNegate is applied).
	 *
	 * @param SpawnPoint The spawn point about to spawn.
	 * @return True when satisfied.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Items|Spawn")
	bool IsSatisfied(const AMKHItemSpawnPoint* SpawnPoint) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Inverts the result of the condition (e.g. "a pawn is nearby" instead of "no pawn nearby"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	bool bNegate = false;
};

/** Holds when no player pawn stands within Radius of the spawn point, so items never appear in a player's face. */
UCLASS(meta = (DisplayName = "No Pawn Nearby"))
class MAKHIA_API UMKHItemSpawnCondition_NoPawnNearby : public UMKHItemSpawnCondition
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** True when every player pawn is farther than Radius from the spawn point. */
	virtual bool IsSatisfied_Implementation(const AMKHItemSpawnPoint* SpawnPoint) const override;

	// ============================================================
	// Properties
	// ============================================================

	/** Distance from the spawn point within which a player pawn blocks the spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0", Units = "cm"))
	float Radius = 300.f;
};

/** Score situations tested by UMKHItemSpawnCondition_Score. */
UENUM(BlueprintType)
enum class EMKHSpawnScoreRule : uint8
{
	/** Both participants have the same score. */
	Tied,
	/** One participant (either) leads by at least ScoreDifference. */
	LeadAtLeast,
	/** The spawn point's exclusive owner trails by at least ScoreDifference (catch-up items). */
	OwnerBehindAtLeast,
	/** The spawn point's exclusive owner leads by at least ScoreDifference. */
	OwnerAheadAtLeast
};

/**
 * Holds depending on the duel score. The Owner rules need a spawn point reserved to one player
 * (CollectRestriction = PlayerStartOwner); on a free spawn point they never hold.
 */
UCLASS(meta = (DisplayName = "Score"))
class MAKHIA_API UMKHItemSpawnCondition_Score : public UMKHItemSpawnCondition
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/** Reports an Owner rule used on a spawn point that is not reserved to a player. */
	virtual EDataValidationResult ValidateForSpawnPoint(const AMKHItemSpawnPoint& SpawnPoint, FDataValidationContext& Context) const override;
#endif

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Compares the two participants' scores according to Rule. */
	virtual bool IsSatisfied_Implementation(const AMKHItemSpawnPoint* SpawnPoint) const override;

	/** Whether Rule compares the exclusive owner against the opponent. */
	bool IsOwnerRule() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Score situation required to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	EMKHSpawnScoreRule Rule = EMKHSpawnScoreRule::Tied;

	/** Minimum score gap for the Lead / Behind / Ahead rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0", EditCondition = "Rule != EMKHSpawnScoreRule::Tied", EditConditionHides))
	int32 ScoreDifference = 1;
};

/** Holds while the match is in one of the listed phases (useful for manual triggers and respawns). */
UCLASS(meta = (DisplayName = "Match Phase"))
class MAKHIA_API UMKHItemSpawnCondition_MatchPhase : public UMKHItemSpawnCondition
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** True when the current match phase is one of Phases. */
	virtual bool IsSatisfied_Implementation(const AMKHItemSpawnPoint* SpawnPoint) const override;

	// ============================================================
	// Properties
	// ============================================================

	/** Phases in which the spawn point may spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TArray<EMKHMatchPhase> Phases;
};
