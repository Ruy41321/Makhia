// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "AttackingMovementState.generated.h"

/**
 * Attacking state for the movement state machine
 */
UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UAttackingMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	UAttackingMovementState();

	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::Attacking; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;
};

