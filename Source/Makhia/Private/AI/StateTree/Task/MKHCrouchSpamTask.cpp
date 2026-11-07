// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHCrouchSpamTask.h"

#include "AIController.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "InputActionValue.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHCrouchSpamTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime = 0.f;

	UCrouchSystemComponent* CrouchSystem = GetPawnCrouchSystem(Data.AIController);
	if (!IsValid(CrouchSystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHCrouchSpamTask: no crouch component on %s."),
			*GetNameSafe(IsValid(Data.AIController) ? Data.AIController->GetPawn() : nullptr));
		return EStateTreeRunStatus::Failed;
	}

	PressCrouch(*CrouchSystem, Data);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMKHCrouchSpamTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime += DeltaTime;

	UCrouchSystemComponent* CrouchSystem = GetPawnCrouchSystem(Data.AIController);
	if (!IsValid(CrouchSystem))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (Data.MaxDuration > 0.f && Data.ElapsedTime >= Data.MaxDuration)
	{
		CrouchSystem->ForceUncrouch();
		return EStateTreeRunStatus::Succeeded;
	}

	Data.TimeToNextPress -= DeltaTime;
	if (Data.TimeToNextPress <= 0.f)
	{
		PressCrouch(*CrouchSystem, Data);
	}
	return EStateTreeRunStatus::Running;
}

void FMKHCrouchSpamTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (UCrouchSystemComponent* CrouchSystem = GetPawnCrouchSystem(Data.AIController); IsValid(CrouchSystem))
	{
		CrouchSystem->ForceUncrouch();
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

UCrouchSystemComponent* FMKHCrouchSpamTask::GetPawnCrouchSystem(const AAIController* AIController)
{
	const APawn* Pawn = IsValid(AIController) ? AIController->GetPawn() : nullptr;
	return IsValid(Pawn) ? Pawn->FindComponentByClass<UCrouchSystemComponent>() : nullptr;
}

void FMKHCrouchSpamTask::PressCrouch(UCrouchSystemComponent& CrouchSystem, FMKHCrouchSpamTaskInstanceData& Data)
{
	// The player's crouch key: a toggle with the same gates (crowd control, dodge, movement state).
	CrouchSystem.CrouchPressed(FInputActionValue());
	Data.TimeToNextPress = FMath::FRandRange(Data.ToggleIntervalRange.Min, Data.ToggleIntervalRange.Max);
}
