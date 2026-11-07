// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "SprintingMovementState.generated.h"

/**
 * Movement state for when the player is sprinting
 */
UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API USprintingMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	USprintingMovementState();

	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::Sprinting; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
};
