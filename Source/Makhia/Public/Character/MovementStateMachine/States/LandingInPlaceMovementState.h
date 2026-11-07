// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "LandingInPlaceMovementState.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API ULandingInPlaceMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	ULandingInPlaceMovementState();
	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::LandingInPlace; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;
};