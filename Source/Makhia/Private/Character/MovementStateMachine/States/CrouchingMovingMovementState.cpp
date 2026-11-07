// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/States/CrouchingMovingMovementState.h"
#include "Character/Components/Dodge/DodgeSystemComponent.h"
#include "Character/Components/Sprint/SprintSystemComponent.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/CharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/Components/Blocking/BlockingSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

UCrouchingMovingMovementState::UCrouchingMovingMovementState()
{
}

EMovementStateValue UCrouchingMovingMovementState::GetDesiredTransition_Implementation() const
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

	// Check for dodging - high priority
	if (Character->DodgeSystem && Character->DodgeSystem->IsDodging())
		return EMovementStateValue::Dodging;

	// Check for falling
	if (Character->GetCharacterMovement()->IsFalling())
	{
		FVector Velocity = Character->GetCharacterMovement()->Velocity;
		if (Velocity.Z > 0.0f)
			return EMovementStateValue::Jumping;
		else
			return EMovementStateValue::Falling;
	}

	// Check if willing to block but didnt because it was not safe to uncrouch
	if (Character->BlockingSystem && Character->BlockingSystem->IsBlocking() && 
		Character->CrouchSystem && Character->CrouchSystem->CanUncrouchSafely())
	{
		return EMovementStateValue::Blocking;
	}
	
	// Check if no longer crouching
	if (!Character->CrouchSystem || !Character->CrouchSystem->IsCrouched())
	{
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

	// Check if stopped moving while crouched
	float HorizontalSpeed = Character->GetCharacterMovement()->Velocity.Size2D();
	if (HorizontalSpeed <= 0.1f) // Small threshold to avoid floating-point precision issues
	{
		return EMovementStateValue::CrouchingIdle;
	}

	return EMovementStateValue::None;
}

bool UCrouchingMovingMovementState::CanTransitionTo_Implementation(EMovementStateValue NewState) const
{
	// Crouching Moving can transition to various states
	switch (NewState)
	{
	case EMovementStateValue::CrouchingIdle:
		// Can stop while crouched
		return true;
	case EMovementStateValue::Dodging:
		// Can dodge while crouched moving
		return true;
	case EMovementStateValue::Jumping:
	case EMovementStateValue::Falling:
		// Air movement always allowed
		return true;
	case EMovementStateValue::Walking:
	case EMovementStateValue::Sprinting:
		// Stand up and continue moving
		return true;
	case EMovementStateValue::Idle:
		// Stand up and stop
		return true;
	case EMovementStateValue::LandingInPlace:
	case EMovementStateValue::LandingMoving:
		// Landing states can be reached from air
		return true;
	default:
		return Super::CanTransitionTo_Implementation(NewState);
	}
}