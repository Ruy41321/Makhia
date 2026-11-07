// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHRequestDodgeDirectionTask.h"

#include "AI/Controller/MKHDuelBotController.h"
#include "Character/CharacterBase.h"
#include "Character/Components/Dodge/DodgeSystemComponent.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHRequestDodgeDirectionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController);
	const ACharacterBase* BotCharacter = IsValid(BotController) ? Cast<ACharacterBase>(BotController->GetPawn()) : nullptr;
	const ACharacterBase* Opponent = IsValid(BotController) ? BotController->GetOpponent() : nullptr;
	if (!IsValid(BotCharacter) || !IsValid(BotCharacter->DodgeSystem) || !IsValid(Opponent))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHRequestDodgeDirectionTask: missing bot, dodge system or opponent on %s."), *GetNameSafe(BotCharacter));
		return EStateTreeRunStatus::Failed;
	}

	// The path following input would add to the dodge and bend its direction.
	BotController->StopMovement();
	const FVector Direction = PickDodgeDirection(BotCharacter->GetActorLocation(), Opponent->GetActorLocation(), 
		Data.BackwardWeight, Data.ForwardWeight, Data.LeftWeight, Data.RightWeight);
	BotCharacter->DodgeSystem->SetRequestedDodgeDirection(Direction);
	return EStateTreeRunStatus::Succeeded;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

FVector FMKHRequestDodgeDirectionTask::PickDodgeDirection(const FVector& BotLocation, const FVector& OpponentLocation,
	const float BackwardWeight, const float ForwardWeight, const float LeftWeight, const float RightWeight)
{
	const FVector ToOpponent = (OpponentLocation - BotLocation).GetSafeNormal2D();
	const FVector Forward = ToOpponent;
	const FVector Backward = -ToOpponent;
	const FVector Side = FVector::CrossProduct(ToOpponent, FVector::UpVector);
	const FVector Left = Side;
	const FVector Right = -Side;

	// Randomly pick a direction based on the weights.
	const float TotalWeight = BackwardWeight + ForwardWeight + LeftWeight + RightWeight;
	if (TotalWeight <= 0.f)
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHRequestDodgeDirectionTask: all weights are zero or negative."));
		return Backward; // Default to backward if all weights are zero or negative.
	}

	const float RandomValue = FMath::FRandRange(0.f, TotalWeight);
	if (RandomValue < BackwardWeight)
	{
		return Backward;
	}
	
	if (RandomValue < BackwardWeight + ForwardWeight)
	{
		return Forward;
	}
	
	if (RandomValue < BackwardWeight + ForwardWeight + LeftWeight)
	{
		return Left;
	}
	
	return Right;
}