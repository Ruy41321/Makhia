// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/Controller/MKHDuelBotStateTreeComponent.h"

#include "StateTreeExecutionContext.h"

namespace MKHDuelBotStateTreeComponentPrivate
{
	/** Execution context exposing the engine's evaluator tick (protected), the same steps as a partial tick. */
	struct FEvaluatorsOnlyContext : public FStateTreeExecutionContext
	{
		using FStateTreeExecutionContext::FStateTreeExecutionContext;

		/** Prelude, evaluators without the global tasks, postlude. */
		void TickEvaluators(const float DeltaTime)
		{
			if (TickPrelude() == EStateTreeRunStatus::Running)
			{
				TickEvaluatorsAndGlobalTasks(FMath::Max(DeltaTime, 0.f), false);
				TickPostlude();
			}
		}
	};
}

// ============================================================
// Public Interface
// ============================================================

void UMKHDuelBotStateTreeComponent::TickEvaluatorsOnly(const float DeltaTime)
{
	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (!bIsRunning || StateTree == nullptr || !IsValid(GetOwner()))
	{
		return;
	}

	MKHDuelBotStateTreeComponentPrivate::FEvaluatorsOnlyContext Context(*GetOwner(), *StateTree, InstanceData);
	if (SetContextRequirements(Context))
	{
		Context.TickEvaluators(DeltaTime);
	}
}
