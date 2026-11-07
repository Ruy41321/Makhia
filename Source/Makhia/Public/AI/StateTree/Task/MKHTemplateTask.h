// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHTemplateTask.generated.h"

class AAIController;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/*
 * Skeleton of a StateTree task: copy this pair (Public/.../MKHTemplateTask.h, Private/.../MKHTemplateTask.cpp), rename
 * "Template" everywhere (file names, structs, generated.h include, log prefix), drop "Hidden" from the USTRUCT meta and
 * remove the callbacks the task does not need (an instant task usually only keeps EnterState).
 */

/**
 * Instance data of FMKHTemplateTask: the bot's controller, the parameters set in the tree and the runtime state.
 */
USTRUCT()
struct FMKHTemplateTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Seconds after which the task succeeds; 0 runs until the state is left. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, Units = "s"))
	float MaxDuration = 0.f;

	/** Seconds since the state was entered. */
	float ElapsedTime = 0.f;
};

/**
 * Template StateTree task of the duel bot, hidden from the node picker: describe here what the task does, which
 * component of the bot it drives and when it is Running, Succeeded or Failed. Leaving the state early (death, higher
 * priority state) must undo whatever the task left half-done.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (Hidden, DisplayName = "MKH Template", Category = "MKH"))
struct MAKHIA_API FMKHTemplateTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHTemplateTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Resets the runtime state and starts the task.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running, or Failed when the bot has no pawn.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/**
	 * Advances the task each frame.
	 *
	 * @param Context    Execution context of the running StateTree.
	 * @param DeltaTime  Seconds since the last tick.
	 * @return Running until MaxDuration, then Succeeded; Failed when the bot lost its pawn.
	 */
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Cleans up when the state is left, whatever the reason.
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
	 * Gets the pawn driven by a controller.
	 *
	 * @param AIController  Controller of the bot.
	 * @return The controlled pawn, or null.
	 */
	static APawn* GetControlledPawn(const AAIController* AIController);
};
