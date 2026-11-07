// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHCommitAttackTask.h"

#include "AIController.h"
#include "AI/MKHAttackScoring.h"
#include "AI/Controller/MKHDuelBotController.h"
#include "Math/RandomStream.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

// ============================================================
// Lifecycle
// ============================================================

FMKHCommitAttackTask::FMKHCommitAttackTask()
{
	// Considered for completion on purpose (it never succeeds): see DuelBotAI.md, State completion of parent tasks.
	bShouldCallTick = false;
}

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHCommitAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.AttackCandidates.IsEmpty())
	{
		UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Commit attack: no candidate, Attack fails."));
		Data.ChosenAttack = FMKHAttackCandidate();
		return EStateTreeRunStatus::Failed;
	}

	// The probabilities already carry the expertise sharpness: the draw is the whole decision.
	TArray<float> Probabilities;
	for (const FMKHAttackCandidate& Candidate : Data.AttackCandidates)
	{
		Probabilities.Add(Candidate.Probability);
	}
	FRandomStream RandomStream(FMath::Rand());
	const int32 ChosenIndex = FMKHAttackScoring::PickIndex(Probabilities, RandomStream);
	Data.ChosenAttack = Data.AttackCandidates[ChosenIndex];
	if (AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController))
	{
		// The Bait score measures how long commits stay without an attack (chase).
		BotController->NotifyAttackCommitted();
	}

	UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Commit attack: %s (score %.2f, p %.2f%s).\n%s"),
		*Data.ChosenAttack.AbilityTag.ToString(), Data.ChosenAttack.Score, Data.ChosenAttack.Probability,
		Data.ChosenAttack.bNeedsSwap ? TEXT(", swap first") : TEXT(""),
		*FMKHDuelBotAttackOptionsEvaluator::FormatCandidateTable(Data.AttackCandidates, ChosenIndex));
	return EStateTreeRunStatus::Running;
}
