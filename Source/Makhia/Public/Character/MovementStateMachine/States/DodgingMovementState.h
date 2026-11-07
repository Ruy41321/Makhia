// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "DodgingMovementState.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UDodgingMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	UDodgingMovementState();
	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::Dodging; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;
};
