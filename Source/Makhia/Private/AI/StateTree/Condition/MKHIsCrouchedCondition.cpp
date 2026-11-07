// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Condition/MKHIsCrouchedCondition.h"

#include "AIController.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

bool FMKHIsCrouchedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const APawn* Pawn = IsValid(Data.AIController) ? Data.AIController->GetPawn() : nullptr;
	const UCrouchSystemComponent* CrouchSystem = IsValid(Pawn) ? Pawn->FindComponentByClass<UCrouchSystemComponent>() : nullptr;
	if (!IsValid(CrouchSystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHIsCrouchedCondition: no crouch component on %s."), *GetNameSafe(Pawn));
		return false;
	}

	return CrouchSystem->IsCrouched() == Data.bCrouched;
}
