// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHSetCrouchTask.h"

#include "AIController.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "InputActionValue.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHSetCrouchTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	UCrouchSystemComponent* CrouchSystem = GetPawnCrouchSystem(Data.AIController);
	if (!IsValid(CrouchSystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHSetCrouchTask: no crouch component on %s."),
			*GetNameSafe(IsValid(Data.AIController) ? Data.AIController->GetPawn() : nullptr));
		return EStateTreeRunStatus::Failed;
	}

	// The player's crouch key is a toggle: pressing it in the asked stance would leave it.
	if (CrouchSystem->IsCrouched() != Data.bCrouch)
	{
		CrouchSystem->CrouchPressed(FInputActionValue());
	}

	// Refused for lack of headroom (e.g. against a wall): retrying from the same spot would never succeed.
	if (!Data.bCrouch && CrouchSystem->IsCrouched())
	{
		CrouchSystem->TryStandUpWithNudge();
	}

	if (CrouchSystem->IsCrouched() != Data.bCrouch)
	{
		UE_LOG(LogMKHAI, Log, TEXT("MKHSetCrouchTask: %s refused on %s (crowd control, dodge, movement state or headroom)."),
			Data.bCrouch ? TEXT("crouch") : TEXT("stand up"), *GetNameSafe(CrouchSystem->GetOwner()));
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Succeeded;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

UCrouchSystemComponent* FMKHSetCrouchTask::GetPawnCrouchSystem(const AAIController* AIController)
{
	const APawn* Pawn = IsValid(AIController) ? AIController->GetPawn() : nullptr;
	return IsValid(Pawn) ? Pawn->FindComponentByClass<UCrouchSystemComponent>() : nullptr;
}
