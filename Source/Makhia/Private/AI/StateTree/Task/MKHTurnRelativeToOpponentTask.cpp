// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHTurnRelativeToOpponentTask.h"

#include "AI/Controller/MKHDuelBotController.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHTurnRelativeToOpponentTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.HeldTime = -1.f;

	APawn* Bot = nullptr;
	const AActor* Opponent = nullptr;
	if (!GetBotAndOpponent(Data.AIController, Bot, Opponent))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHTurnRelativeToOpponentTask: missing bot or opponent on %s."), *GetNameSafe(Bot));
		return EStateTreeRunStatus::Failed;
	}

	// With bOrientRotationToMovement any leftover path following would turn the body back towards the movement.
	Data.AIController->StopMovement();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMKHTurnRelativeToOpponentTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	APawn* Bot = nullptr;
	const AActor* Opponent = nullptr;
	if (!GetBotAndOpponent(Data.AIController, Bot, Opponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	// Standing still the body keeps the yaw it is given; the controller yaw (focus) only drives the aim.
	const FRotator CurrentRotation = Bot->GetActorRotation();
	const float TargetYaw = GetTargetYaw(Bot->GetActorLocation(), Opponent->GetActorLocation(), Data.AngleFromOpponent, CurrentRotation.Yaw);
	const FRotator TargetRotation(CurrentRotation.Pitch, TargetYaw, CurrentRotation.Roll);
	Bot->SetActorRotation(FMath::RInterpConstantTo(CurrentRotation, TargetRotation, DeltaTime, Data.TurnRate));

	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(Bot->GetActorRotation().Yaw, TargetYaw));
	if (Data.HeldTime < 0.f)
	{
		// Still turning: the hold starts the first time the facing is reached and keeps counting if the opponent circles.
		if (YawError > Data.AngleTolerance)
		{
			return EStateTreeRunStatus::Running;
		}
		Data.HeldTime = 0.f;
	}

	Data.HeldTime += DeltaTime;
	return Data.HoldDuration > 0.f && Data.HeldTime >= Data.HoldDuration ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool FMKHTurnRelativeToOpponentTask::GetBotAndOpponent(const AAIController* AIController, APawn*& OutBot, const AActor*& OutOpponent)
{
	const AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(AIController);
	OutBot = IsValid(BotController) ? BotController->GetPawn() : nullptr;
	OutOpponent = IsValid(BotController) ? BotController->GetOpponent() : nullptr;
	return IsValid(OutBot) && IsValid(OutOpponent);
}

float FMKHTurnRelativeToOpponentTask::GetTargetYaw(const FVector& BotLocation, const FVector& OpponentLocation, const float AngleFromOpponent,
	const float FallbackYaw)
{
	const FVector ToOpponent = (OpponentLocation - BotLocation).GetSafeNormal2D();
	if (ToOpponent.IsNearlyZero())
	{
		return FallbackYaw;
	}
	return FRotator::NormalizeAxis(ToOpponent.Rotation().Yaw + AngleFromOpponent);
}
