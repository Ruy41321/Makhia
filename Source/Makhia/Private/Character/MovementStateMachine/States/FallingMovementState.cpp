// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/States/FallingMovementState.h"
#include "Character/CharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/Components/Blocking/BlockingSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

UFallingMovementState::UFallingMovementState()
{
}

EMovementStateValue UFallingMovementState::GetDesiredTransition_Implementation() const
{
	ACharacterBase* Character = GetOwnerCharacter();
	if (!Character || !Character->GetCharacterMovement())
		return EMovementStateValue::None;

	// Check for attacking
	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(MKHGameplayTags::Ability::Attacking))
		{
			return EMovementStateValue::Attacking;
		}
	}

	// Check if we've landed
	if (!Character->GetCharacterMovement()->IsFalling())
	{
		// Enter block state on state ending if the player was willing to block during this state
		if (Character->BlockingSystem && Character->BlockingSystem->IsBlocking())
		{
			return EMovementStateValue::Blocking;
		}
		
		// Decide between LandingInPlace and LandingMoving based on horizontal velocity
		float HorizontalSpeed = Character->GetCharacterMovement()->Velocity.Size2D();
		if (HorizontalSpeed > 0.1f) // Small threshold to avoid floating-point precision issues
			return EMovementStateValue::LandingMoving;
		else
			return EMovementStateValue::LandingInPlace;
	}

	return EMovementStateValue::None;
}

bool UFallingMovementState::CanTransitionTo_Implementation(EMovementStateValue NewState) const
{
	return Super::CanTransitionTo_Implementation(NewState);
}
