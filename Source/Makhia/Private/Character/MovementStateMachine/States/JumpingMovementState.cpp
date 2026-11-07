// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/States/JumpingMovementState.h"
#include "Character/Components/Jump/JumpSystemComponent.h"
#include "Character/Components/Sprint/SprintSystemComponent.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/CharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

UJumpingMovementState::UJumpingMovementState()
{
}

EMovementStateValue UJumpingMovementState::GetDesiredTransition_Implementation() const
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

	// Check if we're falling (velocity.Z <= 0)
	if (Character->GetCharacterMovement()->IsFalling())
	{
		FVector Velocity = Character->GetCharacterMovement()->Velocity;
		if (Velocity.Z <= 0.0f)
			return EMovementStateValue::Falling;
	}
	else
	{
		// We've landed, check for landing state
		if (Character->JumpSystem && Character->JumpSystem->IsLanding())
		{
			// Decide between LandingInPlace and LandingMoving based on horizontal velocity
			float HorizontalSpeed = Character->GetCharacterMovement()->Velocity.Size2D();
			if (HorizontalSpeed > 0.1f) // Small threshold to avoid floating-point precision issues
				return EMovementStateValue::LandingMoving;
			else
				return EMovementStateValue::LandingInPlace;
		}
		
		// Determine ground state based on other conditions - decide between crouching states based on velocity
		if (Character->CrouchSystem && Character->CrouchSystem->IsCrouched())
		{
			float HorizontalSpeed = Character->GetCharacterMovement()->Velocity.Size2D();
			if (HorizontalSpeed > 0.1f)
				return EMovementStateValue::CrouchingMoving;
			else
				return EMovementStateValue::CrouchingIdle;
		}
		
		float Speed = Character->GetCharacterMovement()->Velocity.Size2D();
		if (Speed > 10.0f)
		{
			if (Character->SprintSystem && Character->SprintSystem->IsSprinting())
				return EMovementStateValue::Sprinting;
			else
				return EMovementStateValue::Walking;
		}
		else
			return EMovementStateValue::Idle;
	}

	return EMovementStateValue::None;
}

bool UJumpingMovementState::CanTransitionTo_Implementation(EMovementStateValue NewState) const
{
	return Super::CanTransitionTo_Implementation(NewState);
}