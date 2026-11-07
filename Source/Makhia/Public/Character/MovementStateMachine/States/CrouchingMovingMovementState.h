// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "CrouchingMovingMovementState.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UCrouchingMovingMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	UCrouchingMovingMovementState();
	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::CrouchingMoving; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;
};