// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHFindRepositionPointTask.h"

#include "GameFramework/Actor.h"
#include "MKHLogChannels.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHFindRepositionPointTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!IsValid(Data.Target))
	{
		return EStateTreeRunStatus::Failed;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Context.GetWorld());
	if (!IsValid(NavSystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHFindRepositionPointTask: no navigation system in this world."));
		return EStateTreeRunStatus::Failed;
	}

	const float MinDistance = FMath::Min(Data.MinDistance, Data.MaxDistance);
	const float MaxDistance = FMath::Max(Data.MinDistance, Data.MaxDistance);
	for (int32 Attempt = 0; Attempt < Data.MaxAttempts; ++Attempt)
	{
		const FVector Candidate = PickPointInRing(Data.Target->GetActorLocation(), MinDistance, MaxDistance);
		FNavLocation NavLocation;
		if (NavSystem->ProjectPointToNavigation(Candidate, NavLocation))
		{
			Data.Destination = NavLocation.Location;
			return EStateTreeRunStatus::Succeeded;
		}
	}

	UE_LOG(LogMKHAI, Log, TEXT("MKHFindRepositionPointTask: no NavMesh point around %s after %d attempts."),
		*Data.Target->GetName(), Data.MaxAttempts);
	return EStateTreeRunStatus::Failed;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

FVector FMKHFindRepositionPointTask::PickPointInRing(const FVector& Center, const float MinDistance, const float MaxDistance)
{
	const float Yaw = FMath::FRandRange(0.f, 360.f);
	const float Distance = FMath::FRandRange(MinDistance, MaxDistance);
	return Center + FRotator(0.f, Yaw, 0.f).Vector() * Distance;
}
