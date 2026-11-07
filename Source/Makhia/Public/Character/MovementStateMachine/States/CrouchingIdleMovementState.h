// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "CrouchingIdleMovementState.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UCrouchingIdleMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	UCrouchingIdleMovementState();
	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::CrouchingIdle; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;
};