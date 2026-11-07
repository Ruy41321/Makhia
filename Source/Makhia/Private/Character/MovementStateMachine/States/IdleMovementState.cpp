// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/States/IdleMovementState.h"
#include "Character/Components/Dodge/DodgeSystemComponent.h"
#include "Character/Components/Sprint/SprintSystemComponent.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/CharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

UIdleMovementState::UIdleMovementState()
{
}

EMovementStateValue UIdleMovementState::GetDesiredTransition_Implementation() const
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

	// Check for falling
	if (Character->GetCharacterMovement()->IsFalling())
	{
		FVector Velocity = Character->GetCharacterMovement()->Velocity;
		if (Velocity.Z > 0.0f)
			return EMovementStateValue::Jumping;
		else
			return EMovementStateValue::Falling;
	}

	// Check for dodging
	if (Character->DodgeSystem && Character->DodgeSystem->IsDodging())
		return EMovementStateValue::Dodging;

	// Check for crouching - decide between idle and moving based on velocity
	if (Character->CrouchSystem && Character->CrouchSystem->IsCrouched())
	{
		float HorizontalSpeed = Character->GetCharacterMovement()->Velocity.Size2D();
		if (HorizontalSpeed > 0.1f)
			return EMovementStateValue::CrouchingMoving;
		else
			return EMovementStateValue::CrouchingIdle;
	}

	// Check for movement
	float Speed = Character->GetCharacterMovement()->Velocity.Size2D();
	if (Speed > 10.0f) // Small threshold to avoid jitter
	{
		// Check if sprinting
		if (Character->SprintSystem && Character->SprintSystem->IsSprinting())
			return EMovementStateValue::Sprinting;
		else
			return EMovementStateValue::Walking;
	}

	return EMovementStateValue::None;
}

void UIdleMovementState::EnterState(EMovementStateValue PreviousState)
{
	Super::EnterState(PreviousState);
	// Additional logic for entering idle state can be added here
}
