// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHMakeDistanceTask.generated.h"

class AAIController;
class UNavigationSystemV1;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHMakeDistanceTask: the bot's controller, the distance to keep from the opponent, how often the
 * retreat point is refreshed and the runtime state of the retreat.
 */
USTRUCT()
struct FMKHMakeDistanceTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Distance from the opponent the bot tries to keep, on the ground plane. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, Units = "cm"))
	float DesiredDistance = 900.f;

	/** How much closer than DesiredDistance the opponent may come before the bot retreats again (avoids jitter). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, Units = "cm"))
	float DistanceTolerance = 150.f;

	/** Seconds between two retreat points while retreating, so the bot follows an opponent that chases it. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.05f, Units = "s"))
	float RepathInterval = 0.4f;

	/** Directions tried around "straight away from the opponent" when that one is blocked (wall, arena edge). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 1))
	int32 MaxAttempts = 9;

	/** Widest angle off "straight away" a retreat direction may take, reached by the last attempt. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, ClampMax = 180.f, Units = "deg"))
	float MaxDeviationAngle = 135.f;

	/** Seconds after which the task succeeds; 0 keeps the distance until the state is left. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, Units = "s"))
	float MaxDuration = 0.f;

	/** Distance from the opponent this tick; bind it to show or test the spacing. */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (Units = "cm"))
	float CurrentDistance = 0.f;

	/** Seconds since the state was entered. */
	float ElapsedTime = 0.f;

	/** Seconds left before the retreat point is refreshed. */
	float TimeToRepath = 0.f;

	/** True while a move requested by this task is running. */
	bool bRetreating = false;
};

/**
 * StateTree task of the duel bot: low health or healing. Moves away from the opponent and keeps it at about
 * DesiredDistance: when the opponent comes closer than DesiredDistance - DistanceTolerance the bot walks to a NavMesh
 * point DesiredDistance away from it, refreshed every RepathInterval while it chases, trying directions turned more and
 * more off "straight away" when cornered; once far enough it stops and waits. The focus on the opponent (aim) is kept.
 * Running while keeping the distance, Succeeded after MaxDuration, Failed without bot, opponent or navigation.
 * Leaving the state stops a retreat still running.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Make Distance", Category = "MKH"))
struct MAKHIA_API FMKHMakeDistanceTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHMakeDistanceTaskInstanceData;

	// ============================================================
	// Lifecycle
	// ============================================================

	/**
	 * Keeps the task running when its state is reselected (e.g. a child returns to Combat and the utility picks this
	 * state again): no ExitState/EnterState, so the retreat is not stopped and ElapsedTime keeps counting.
	 */
	FMKHMakeDistanceTask() { bShouldStateChangeOnReselect = false; }

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Resets the retreat state; the first retreat starts on the first tick.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running, or Failed without bot or opponent.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/**
	 * Starts, refreshes or stops the retreat from the current distance to the opponent.
	 *
	 * @param Context    Execution context of the running StateTree, gives access to the world.
	 * @param DeltaTime  Seconds since the last tick.
	 * @return Running while keeping the distance, Succeeded after MaxDuration, Failed without bot, opponent or navigation.
	 */
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Stops a retreat still running, so the bot does not keep walking away in the next state.
	 *
	 * @param Context     Execution context of the running StateTree.
	 * @param Transition  Transition leaving the state (unused).
	 */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Gets the bot's pawn and its opponent.
	 *
	 * @param AIController  Controller of the bot, an AMKHDuelBotController.
	 * @param OutBot        The controlled pawn, or null.
	 * @param OutOpponent   The opponent, or null.
	 * @return True when both are valid.
	 */
	static bool GetBotAndOpponent(const AAIController* AIController, const APawn*& OutBot, const AActor*& OutOpponent);

	/**
	 * Requests a move to a NavMesh point DesiredDistance away from the opponent, straight away from it first, then
	 * alternating left and right with a growing angle up to MaxDeviationAngle.
	 *
	 * @param NavSystem         Navigation system of the bot's world.
	 * @param BotLocation       Bot's location.
	 * @param OpponentLocation  Opponent's location.
	 * @param Data              Instance data with the controller and the parameters; bRetreating is set on success.
	 * @return True when a move was requested.
	 */
	static bool RequestRetreat(const UNavigationSystemV1& NavSystem, const FVector& BotLocation, const FVector& OpponentLocation,
		FMKHMakeDistanceTaskInstanceData& Data);

	/**
	 * Angle off "straight away" of a retreat attempt: 0, +a, -a, +2a, -2a... with a = MaxDeviationAngle / (pairs).
	 *
	 * @param Attempt            Index of the attempt, from 0.
	 * @param MaxAttempts        Number of attempts.
	 * @param MaxDeviationAngle  Angle reached by the last pair of attempts.
	 * @return Yaw offset in degrees.
	 */
	static float GetAttemptAngle(int32 Attempt, int32 MaxAttempts, float MaxDeviationAngle);
};
