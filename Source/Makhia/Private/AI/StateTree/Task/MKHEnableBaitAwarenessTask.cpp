// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHEnableBaitAwarenessTask.h"

#include "MKHLogChannels.h"
#include "VisualLogger/VisualLogger.h"
#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "AI/MKHDuelBot.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHEnableBaitAwarenessTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AMKHDuelBot* BotCharacter = Cast<AMKHDuelBot>(Data.AIController->GetPawn());
	if (!IsValid(BotCharacter))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHEnableBaitAwarenessTask: no pawn on %s."), *GetNameSafe(Data.AIController));
		return EStateTreeRunStatus::Failed;
	}
	
	BotCharacter->SetIsBaiting(true);
	
	UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("MKHEnableBaitAwarenessTask: enabled bait awareness."));
	return EStateTreeRunStatus::Running;
}

void FMKHEnableBaitAwarenessTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	AMKHDuelBot* BotCharacter = IsValid(Data.AIController) ? Cast<AMKHDuelBot>(Data.AIController->GetPawn()) : nullptr;
	if (!IsValid(BotCharacter) || !BotCharacter->IsBaiting())
	{
		return;
	}

	BotCharacter->SetIsBaiting(false);

	UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("MKHEnableBaitAwarenessTask: disabled bait awareness on state exit."));
}
