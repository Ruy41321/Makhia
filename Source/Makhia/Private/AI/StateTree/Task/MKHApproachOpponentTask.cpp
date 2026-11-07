// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHApproachOpponentTask.h"

#include "AI/Controller/MKHDuelBotController.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHApproachOpponentTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.bMoving = false;
	Data.TimeToRetry = 0.f;

	const AActor* Opponent = GetOpponent(Data);
	if (!IsValid(Opponent))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHApproachOpponentTask: missing controller or opponent on %s."), *GetNameSafe(Data.AIController));
		return EStateTreeRunStatus::Failed;
	}
	if (IsOpponentReached(Data, *Opponent))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	RequestMove(Context, Data, *Opponent);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMKHApproachOpponentTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	const AActor* Opponent = GetOpponent(Data);
	if (!IsValid(Opponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.TimeToRetry -= DeltaTime;
	if (Data.bMoving && Data.AIController->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		return EStateTreeRunStatus::Running;
	}

	Data.bMoving = false;
	if (IsOpponentReached(Data, *Opponent))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// Short of the opponent (outside the NavMesh, path blocked): wait here, it may come back in reach.
	if (Data.TimeToRetry <= 0.f)
	{
		RequestMove(Context, Data, *Opponent);
	}
	return EStateTreeRunStatus::Running;
}

void FMKHApproachOpponentTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.bMoving && IsValid(Data.AIController))
	{
		Data.AIController->StopMovement();
	}
	Data.bMoving = false;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

AActor* FMKHApproachOpponentTask::GetOpponent(const FMKHApproachOpponentTaskInstanceData& Data)
{
	const AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController);
	return IsValid(BotController) ? BotController->GetOpponent() : nullptr;
}

bool FMKHApproachOpponentTask::IsOpponentReached(const FMKHApproachOpponentTaskInstanceData& Data, const AActor& Opponent)
{
	const UPathFollowingComponent* PathFollowing = Data.AIController->GetPathFollowingComponent();
	return IsValid(PathFollowing) && PathFollowing->HasReached(Opponent, EPathFollowingReachMode::OverlapAgentAndGoal, Data.AcceptableRadius);
}

void FMKHApproachOpponentTask::RequestMove(const FStateTreeExecutionContext& Context, FMKHApproachOpponentTaskInstanceData& Data, const AActor& Opponent)
{
	Data.TimeToRetry = Data.RetryInterval;

	FAIMoveRequest MoveRequest(&Opponent);
	MoveRequest.SetAcceptanceRadius(Data.AcceptableRadius)
		.SetAllowPartialPath(true)
		.SetRequireNavigableEndLocation(false)
		.SetReachTestIncludesAgentRadius(true)
		.SetReachTestIncludesGoalRadius(true);
	Data.bMoving = Data.AIController->MoveTo(MoveRequest).Code == EPathFollowingRequestResult::RequestSuccessful;
	if (Data.bMoving)
	{
		return;
	}

	const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Context.GetWorld());
	FNavLocation NearestPoint;
	if (IsValid(NavSystem) && NavSystem->ProjectPointToNavigation(Opponent.GetActorLocation(), NearestPoint, Data.NearestPointExtent))
	{
		const EPathFollowingRequestResult::Type Result = Data.AIController->MoveToLocation(NearestPoint.Location, /*AcceptanceRadius*/ -1.f,
			/*bStopOnOverlap*/ true, /*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ false, /*bCanStrafe*/ false,
			/*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
		Data.bMoving = Result == EPathFollowingRequestResult::RequestSuccessful;
	}
}
