// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "WalkingMovementState.generated.h"

/**
 * Movement state for when the player is walking
 */
UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UWalkingMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	UWalkingMovementState();

	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::Walking; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
};
