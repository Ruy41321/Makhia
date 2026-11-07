// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHApproachOpponentTask.generated.h"

class AAIController;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHApproachOpponentTask: the bot's controller, how close to get, and the runtime state of the move.
 */
USTRUCT()
struct FMKHApproachOpponentTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Distance from the opponent at which it counts as reached, on top of the two capsules' radii. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, Units = "cm"))
	float AcceptableRadius = 0.f;

	/** Seconds between two attempts while the target cannot be reached. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.05f, Units = "s"))
	float RetryInterval = 0.25f;

	/** How far from the opponent the NavMesh is searched when no path leads to it. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector NearestPointExtent = FVector(1500.f, 1500.f, 500.f);

	/** Seconds left before the next attempt. */
	float TimeToRetry = 0.f;

	/** True while a move requested by this task is running. */
	bool bMoving = false;
};

/**
 * StateTree task of the duel bot: walks to the opponent. Replaces the engine's Move To on the approach states, which
 * fails at once when the target is outside the NavMesh, so the tree re-selected the state several times per frame. This
 * task never fails for an unreachable opponent: it walks as close as the NavMesh allows and retries every RetryInterval
 * until the opponent can be reached or the state is left. Succeeded when reached, Failed only without an opponent.
 * Leaving the state stops the move.
 *
 * Full model: see Documentation/DuelBotAI.md (The tree).
 */
USTRUCT(meta = (DisplayName = "MKH Approach Opponent", Category = "MKH"))
struct MAKHIA_API FMKHApproachOpponentTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHApproachOpponentTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Starts the move.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running, Succeeded when already at the opponent, Failed without an opponent.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/**
	 * Follows the move and retries when it ended short of the opponent.
	 *
	 * @param Context    Execution context of the running StateTree, gives access to the world.
	 * @param DeltaTime  Seconds since the last tick.
	 * @return Running, Succeeded when the opponent is reached, Failed without an opponent.
	 */
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Stops a move still running, so the bot does not keep walking in the next state.
	 *
	 * @param Context     Execution context of the running StateTree.
	 * @param Transition  Transition leaving the state (unused).
	 */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/** @return The opponent of the bot's controller, or null. */
	static AActor* GetOpponent(const FMKHApproachOpponentTaskInstanceData& Data);

	/**
	 * @param Data      Instance data with the controller and the radius.
	 * @param Opponent  The opponent.
	 * @return True when the bot is within AcceptableRadius of the opponent (capsules' radii included).
	 */
	static bool IsOpponentReached(const FMKHApproachOpponentTaskInstanceData& Data, const AActor& Opponent);

	/**
	 * Requests a move to the opponent; when no path leads to it, to the nearest NavMesh point. bMoving is set when a move
	 * started, and the next attempt is scheduled.
	 *
	 * @param Context   Execution context of the running StateTree, gives access to the world.
	 * @param Data      Instance data with the controller and the parameters.
	 * @param Opponent  The opponent.
	 */
	static void RequestMove(const FStateTreeExecutionContext& Context, FMKHApproachOpponentTaskInstanceData& Data, const AActor& Opponent);
};
