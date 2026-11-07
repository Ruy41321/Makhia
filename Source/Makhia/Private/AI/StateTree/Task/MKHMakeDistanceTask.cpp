// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHMakeDistanceTask.h"

#include "AI/Controller/MKHDuelBotController.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHMakeDistanceTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime = 0.f;
	Data.TimeToRepath = 0.f;
	Data.bRetreating = false;

	const APawn* Bot = nullptr;
	const AActor* Opponent = nullptr;
	if (!GetBotAndOpponent(Data.AIController, Bot, Opponent))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHMakeDistanceTask: missing bot or opponent on %s."), *GetNameSafe(Bot));
		return EStateTreeRunStatus::Failed;
	}

	Data.CurrentDistance = FVector::Dist2D(Bot->GetActorLocation(), Opponent->GetActorLocation());
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMKHMakeDistanceTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime += DeltaTime;

	const APawn* Bot = nullptr;
	const AActor* Opponent = nullptr;
	if (!GetBotAndOpponent(Data.AIController, Bot, Opponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (Data.MaxDuration > 0.f && Data.ElapsedTime >= Data.MaxDuration)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const FVector BotLocation = Bot->GetActorLocation();
	const FVector OpponentLocation = Opponent->GetActorLocation();
	Data.CurrentDistance = FVector::Dist2D(BotLocation, OpponentLocation);

	// Far enough: stop and wait; the retreat starts again only below the tolerance band, not at every step closer.
	if (Data.CurrentDistance >= Data.DesiredDistance)
	{
		if (Data.bRetreating)
		{
			Data.AIController->StopMovement();
			Data.bRetreating = false;
		}
		Data.TimeToRepath = 0.f;
		return EStateTreeRunStatus::Running;
	}
	if (!Data.bRetreating && Data.CurrentDistance >= Data.DesiredDistance - Data.DistanceTolerance)
	{
		return EStateTreeRunStatus::Running;
	}

	// A move that ended on its own (point reached, blocked) is refreshed at once, otherwise every RepathInterval (also
	// when cornered, so the NavMesh is not queried every frame).
	Data.TimeToRepath -= DeltaTime;
	const bool bMoveEnded = Data.bRetreating && Data.AIController->GetMoveStatus() == EPathFollowingStatus::Idle;
	if (!bMoveEnded && Data.TimeToRepath > 0.f)
	{
		return EStateTreeRunStatus::Running;
	}

	const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Context.GetWorld());
	if (!IsValid(NavSystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHMakeDistanceTask: no navigation system in this world."));
		return EStateTreeRunStatus::Failed;
	}

	Data.TimeToRepath = Data.RepathInterval;
	if (!RequestRetreat(*NavSystem, BotLocation, OpponentLocation, Data))
	{
		// Cornered with no reachable point: keep trying, the opponent or the bot may move.
		UE_LOG(LogMKHAI, Verbose, TEXT("MKHMakeDistanceTask: no retreat point for %s after %d attempts."), *Bot->GetName(),
			Data.MaxAttempts);
	}
	return EStateTreeRunStatus::Running;
}

void FMKHMakeDistanceTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.bRetreating && IsValid(Data.AIController))
	{
		Data.AIController->StopMovement();
	}
	Data.bRetreating = false;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool FMKHMakeDistanceTask::GetBotAndOpponent(const AAIController* AIController, const APawn*& OutBot, const AActor*& OutOpponent)
{
	const AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(AIController);
	OutBot = IsValid(BotController) ? BotController->GetPawn() : nullptr;
	OutOpponent = IsValid(BotController) ? BotController->GetOpponent() : nullptr;
	return IsValid(OutBot) && IsValid(OutOpponent);
}

bool FMKHMakeDistanceTask::RequestRetreat(const UNavigationSystemV1& NavSystem, const FVector& BotLocation, const FVector& OpponentLocation,
	FMKHMakeDistanceTaskInstanceData& Data)
{
	FVector AwayFromOpponent = (BotLocation - OpponentLocation).GetSafeNormal2D();
	if (AwayFromOpponent.IsNearlyZero())
	{
		AwayFromOpponent = FVector::ForwardVector;
	}

	for (int32 Attempt = 0; Attempt < Data.MaxAttempts; ++Attempt)
	{
		// Points around the opponent, not the bot: a wide angle slides along a wall instead of running towards it.
		const FVector Direction = AwayFromOpponent.RotateAngleAxis(GetAttemptAngle(Attempt, Data.MaxAttempts, Data.MaxDeviationAngle), FVector::UpVector);
		const FVector Candidate = OpponentLocation + Direction * Data.DesiredDistance;

		FNavLocation NavLocation;
		if (!NavSystem.ProjectPointToNavigation(Candidate, NavLocation)
			|| FVector::Dist2D(NavLocation.Location, OpponentLocation) < Data.DesiredDistance - Data.DistanceTolerance)
		{
			continue;
		}

		// Strafing keeps the focus on the opponent, so the abilities still aim at it while the body faces the path.
		const EPathFollowingRequestResult::Type Result = Data.AIController->MoveToLocation(NavLocation.Location,
			/*AcceptanceRadius*/ -1.f, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ false,
			/*bCanStrafe*/ true, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ false);
		if (Result != EPathFollowingRequestResult::Failed)
		{
			Data.bRetreating = true;
			return true;
		}
	}
	return false;
}

float FMKHMakeDistanceTask::GetAttemptAngle(const int32 Attempt, const int32 MaxAttempts, const float MaxDeviationAngle)
{
	if (Attempt == 0 || MaxAttempts <= 1)
	{
		return 0.f;
	}

	// Attempts 1,2 -> step 1; 3,4 -> step 2... the last pair reaches MaxDeviationAngle.
	const int32 PairCount = MaxAttempts / 2;
	const int32 Step = (Attempt + 1) / 2;
	const float Sign = Attempt % 2 == 1 ? 1.f : -1.f;
	return Sign * MaxDeviationAngle * Step / FMath::Max(PairCount, 1);
}
