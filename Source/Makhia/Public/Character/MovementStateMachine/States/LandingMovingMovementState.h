// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "LandingMovingMovementState.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API ULandingMovingMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	ULandingMovingMovementState();
	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::LandingMoving; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;
};