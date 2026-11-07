// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHEnableBaitAwarenessTask.generated.h"

class AAIController;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHEnableBaitAwarenessTask: the bot's controller.
 */
USTRUCT()
struct FMKHBaitAwarenessInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

};

/**
 * Switches the bot's reactions to the bait values while its state is active. The awareness ends when the state is left,
 * or earlier after MaxBaitTime (Perception evaluator).
 *
 * Model: see Documentation/DuelBotAI.md, Bait.
 */
USTRUCT(meta = (DisplayName = "MKH Enable Bait Awareness", Category = "MKH"))
struct MAKHIA_API FMKHEnableBaitAwarenessTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHBaitAwarenessInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Enables the bait awareness.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running until the state is left (never Succeeded: see DuelBotAI.md, State completion of parent tasks), or
	 *         Failed when the bot has no pawn.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/**
	 * Disables the bait awareness.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that left the state (unused).
	 */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

};
