// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "BlockingState.generated.h"

/**
 * Blocking state for the movement state machine
 */
UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UBlockingState : public UMovementState
{
	GENERATED_BODY()

public:
	UBlockingState();

	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::Blocking; }
	virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;
	virtual void EnterState(EMovementStateValue PreviousState) override;
	virtual void ExitState(EMovementStateValue NextState) override;
};
