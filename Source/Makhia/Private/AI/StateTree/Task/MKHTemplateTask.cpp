// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHTemplateTask.h"

#include "AIController.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHTemplateTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime = 0.f;

	const APawn* Pawn = GetControlledPawn(Data.AIController);
	if (!IsValid(Pawn))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHTemplateTask: no pawn on %s."), *GetNameSafe(Data.AIController));
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMKHTemplateTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime += DeltaTime;

	if (!IsValid(GetControlledPawn(Data.AIController)))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (Data.MaxDuration > 0.f && Data.ElapsedTime >= Data.MaxDuration)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

void FMKHTemplateTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!IsValid(GetControlledPawn(Data.AIController)))
	{
		return;
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

APawn* FMKHTemplateTask::GetControlledPawn(const AAIController* AIController)
{
	return IsValid(AIController) ? AIController->GetPawn() : nullptr;
}
