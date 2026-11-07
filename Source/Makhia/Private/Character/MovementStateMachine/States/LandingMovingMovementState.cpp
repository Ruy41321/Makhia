// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/States/LandingMovingMovementState.h"
#include "Character/Components/Jump/JumpSystemComponent.h"
#include "Character/Components/Sprint/SprintSystemComponent.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/CharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/Components/Blocking/BlockingSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

ULandingMovingMovementState::ULandingMovingMovementState()
{
}

EMovementStateValue ULandingMovingMovementState::GetDesiredTransition_Implementation() const
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

	// Check if landing state is finished
	if (!Character->JumpSystem || !Character->JumpSystem->IsLanding())
	{
		// Landing finished - transition based on current conditions
		
		// Enter block state on state ending if the player was willing to block during this state
		if (Character->BlockingSystem && Character->BlockingSystem->IsBlocking())
		{
			return EMovementStateValue::Blocking;
		}
		
		if (Character->CrouchSystem && Character->CrouchSystem->IsCrouched())
			return EMovementStateValue::CrouchingMoving; // Landing while moving goes to crouching moving
		
		// Check movement based on horizontal velocity
		float Speed = Character->GetCharacterMovement()->Velocity.Size2D();
		if (Speed > 10.0f)
		{
			if (Character->SprintSystem && Character->SprintSystem->IsSprinting())
				return EMovementStateValue::Sprinting;
			else
				return EMovementStateValue::Walking;
		}
		else
		{
			// No more movement - go to idle
			return EMovementStateValue::Idle;
		}
	}

	return EMovementStateValue::None;
}

bool ULandingMovingMovementState::CanTransitionTo_Implementation(EMovementStateValue NewState) const
{
	return Super::CanTransitionTo_Implementation(NewState);
}