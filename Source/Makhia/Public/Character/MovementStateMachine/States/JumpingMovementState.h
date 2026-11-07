// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "JumpingMovementState.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UJumpingMovementState : public UMovementState
{
	GENERATED_BODY()

public:
	UJumpingMovementState();
	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::Jumping; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;
};
