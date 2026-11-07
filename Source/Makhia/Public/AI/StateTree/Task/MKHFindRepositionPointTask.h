// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHFindRepositionPointTask.generated.h"

class AActor;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHFindRepositionPointTask: the actor to reposition around, the ring the point is picked in and the
 * point found, exposed as an output (bind it to the Destination of a Move To task).
 */
USTRUCT()
struct FMKHFindRepositionPointTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Actor the point is picked around (usually the opponent, bound from the duel opponent evaluator). */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> Target = nullptr;

	/** Closest distance from Target the point may be, in cm. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0"))
	float MinDistance = 400.f;

	/** Farthest distance from Target the point may be, in cm. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0"))
	float MaxDistance = 600.f;

	/** Random points tried before giving up when none lands on the NavMesh. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1"))
	int32 MaxAttempts = 8;

	/** Point found on the NavMesh; valid only when the task succeeded. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FVector Destination = FVector::ZeroVector;
};

/**
 * StateTree task of the duel bot: picks a random point on the NavMesh in a ring around Target and finishes at once,
 * Succeeded with Destination set or Failed when no point is reachable. It does not move the bot: a Move To task in the
 * same state does, so turn off "considered for completion" on this task or the state ends before the bot moves.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Find Reposition Point", Category = "MKH"))
struct MAKHIA_API FMKHFindRepositionPointTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHFindRepositionPointTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Picks the reposition point.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data and the world.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Succeeded with Destination set, or Failed when Target is missing or no point is on the NavMesh.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Builds a random point in the ring around a centre, on the centre's height.
	 *
	 * @param Center       Centre of the ring.
	 * @param MinDistance  Inner radius in cm.
	 * @param MaxDistance  Outer radius in cm.
	 * @return The candidate point, not yet projected on the NavMesh.
	 */
	static FVector PickPointInRing(const FVector& Center, float MinDistance, float MaxDistance);
};
