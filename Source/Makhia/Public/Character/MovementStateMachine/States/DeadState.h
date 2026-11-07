// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "DeadState.generated.h"

/**
 * 
 */
UCLASS()
class MAKHIA_API UDeadState : public UMovementState
{
	GENERATED_BODY()
	
public:
	UDeadState();

	virtual EMovementStateValue GetStateType() const override { return EMovementStateValue::Dead; }
	virtual void EnterState(EMovementStateValue PreviousState) override;

	/**
	 * Restores the capsule collision that EnterState disabled, so a revived character
	 * (forced back out of the Dead state for a new duel round) collides normally again.
	 *
	 * @param NextState  The state being transitioned into.
	 */
	virtual void ExitState(EMovementStateValue NextState) override;

	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;

private:

	/**
	 * Blocks or restores the owning player's gameplay input by swapping the mapping context.
	 * Only has an effect on the machine where a player locally controls the character; AI-controlled
	 * characters and other machines (server for a remote pawn, simulated proxies) have no input to block.
	 *
	 * @param bEnabled  True to restore gameplay input, false to apply the restricted dead context.
	 */
	void SetOwnerGameplayInputEnabled(bool bEnabled) const;
};
