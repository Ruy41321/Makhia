// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/States/LandingInPlaceMovementState.h"
#include "Character/Components/Jump/JumpSystemComponent.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/CharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/Components/Blocking/BlockingSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

ULandingInPlaceMovementState::ULandingInPlaceMovementState()
{
}

EMovementStateValue ULandingInPlaceMovementState::GetDesiredTransition_Implementation() const
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
			return EMovementStateValue::CrouchingIdle; // Landing in place goes to crouching idle
		
		// Since this is landing in place, we expect to go to Idle
		return EMovementStateValue::Idle;
	}

	return EMovementStateValue::None;
}

bool ULandingInPlaceMovementState::CanTransitionTo_Implementation(EMovementStateValue NewState) const
{
	return Super::CanTransitionTo_Implementation(NewState);
}