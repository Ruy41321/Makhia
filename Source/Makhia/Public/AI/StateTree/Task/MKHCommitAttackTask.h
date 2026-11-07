// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "AI/StateTree/Evaluator/MKHDuelBotAttackOptionsEvaluator.h"
#include "MKHCommitAttackTask.generated.h"

class AAIController;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/** Instance data of FMKHCommitAttackTask: the candidates to pick from and the attack the bot commits to. */
USTRUCT()
struct FMKHCommitAttackTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Scored attacks with their choice probabilities; bind to the Attack Options evaluator's AttackCandidates. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<FMKHAttackCandidate> AttackCandidates;

	/** The attack picked on entering the state, frozen until the state is left: the child states bind to its fields. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHAttackCandidate ChosenAttack;
};

/**
 * StateTree task of the duel bot, on the Attack parent state: on entering it draws one candidate with the choice
 * probabilities p_i (so the bot's expertise decides how often it takes the best one), logs the decision with the
 * whole candidate table to the Visual Logger and freezes it in ChosenAttack for the children (SwapWeapon,
 * CombatApproach, ActivateAbility). It does not tick and never completes the state (the children do); it fails
 * without candidates.
 *
 * Full model: see Documentation/DuelBotAI.md (Attack choice).
 */
USTRUCT(meta = (DisplayName = "MKH Commit Attack", Category = "MKH"))
struct MAKHIA_API FMKHCommitAttackTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHCommitAttackTaskInstanceData;

	// ============================================================
	// Lifecycle
	// ============================================================

	/** No tick, not considered for completion: the task only holds the choice while the children run. */
	FMKHCommitAttackTask();

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Draws the attack and freezes it in ChosenAttack.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running with a chosen attack, Failed without candidates.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
