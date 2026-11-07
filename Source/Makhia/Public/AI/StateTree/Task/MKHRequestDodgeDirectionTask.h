// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHRequestDodgeDirectionTask.generated.h"

class AAIController;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHRequestDodgeDirectionTask: the bot's controller and how often it dodges backwards.
 */
USTRUCT()
struct FMKHRequestDodgeDirectionTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Weight to dodge away from the opponent. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f))
	float BackwardWeight = 1.0f;
	
	/** Weight to dodge to against the opponent. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f))
	float ForwardWeight = 1.0f;
	
	/** Weight to dodge to self left. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f))
	float LeftWeight = 1.0f;
	
	/** Weight to dodge to self right. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f))
	float RightWeight = 1.0f;
};

/**
 * StateTree task of the duel bot: stops the path following (its input would add to the dodge) and asks the bot's
 * UDodgeSystemComponent for a dodge to a random side of the opponent, or away from it, then finishes at once. It does
 * not dodge: an MKH Activate Ability By Input task (Input.Ability.Attacks.Dodge) in the same state does, so turn off
 * "considered for completion" on this task or the state ends before the dodge starts.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Request Dodge Direction", Category = "MKH"))
struct MAKHIA_API FMKHRequestDodgeDirectionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHRequestDodgeDirectionTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Stops the bot and requests the dodge direction.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Succeeded when a direction was requested, Failed without bot, dodge system or opponent.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Picks a dodge direction on the ground plane relative to the opponent.
	 *
	 * @param BotLocation       Bot's location.
	 * @param OpponentLocation  Opponent's location.
	 * @param BackwardWeight    Weight to go straight away from the opponent.
	 * @param ForwardWeight     Weight to go straight towards the opponent.
	 * @param LeftWeight        Weight to go to the bot's left.
	 * @param RightWeight       Weight to go to the bot's right.
	 * @return Normalized world direction.
	 */
	static FVector PickDodgeDirection( const FVector& BotLocation, const FVector& OpponentLocation, float BackwardWeight,
		float ForwardWeight, float LeftWeight, float RightWeight);
};
