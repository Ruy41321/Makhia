// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "IdleMovementState.generated.h"

/**
 * Movement state for when the player is idle (not moving)
 */
UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UIdleMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	UIdleMovementState();

	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::Idle; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
	virtual void EnterState(EMovementStateValue PreviousState) override;
};
