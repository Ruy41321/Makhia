// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/States/DodgingMovementState.h"
#include "Character/Components/Dodge/DodgeSystemComponent.h"
#include "Character/Components/Sprint/SprintSystemComponent.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/CharacterBase.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/Components/Blocking/BlockingSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

UDodgingMovementState::UDodgingMovementState()
{
}

EMovementStateValue UDodgingMovementState::GetDesiredTransition_Implementation() const
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

	// Check if no longer dodging
	if (!Character->DodgeSystem || !Character->DodgeSystem->IsDodging())
	{
		// Check for falling
		if (Character->GetCharacterMovement()->IsFalling())
		{
			FVector Velocity = Character->GetCharacterMovement()->Velocity;
			if (Velocity.Z > 0.0f)
				return EMovementStateValue::Jumping;
			else
				return EMovementStateValue::Falling;
		}
		
		// Determine ground state based on crouching and velocity
		if (Character->CrouchSystem && Character->CrouchSystem->IsCrouched() && !(!Character->SprintSystem->IsSprintInterrupted() && Character->CrouchSystem->CanUncrouchSafely()))
		{
			float HorizontalSpeed = Character->GetCharacterMovement()->Velocity.Size2D();
			if (HorizontalSpeed > 0.1f)
				return EMovementStateValue::CrouchingMoving;
			else
				return EMovementStateValue::CrouchingIdle;
		}
		
		// Enter block state on dodge finish if the player was willing to block during dodge
		if (Character->BlockingSystem && Character->BlockingSystem->IsBlocking())
		{
			return EMovementStateValue::Blocking;
		}
		
		float Speed = Character->GetCharacterMovement()->Velocity.Size2D();
		if (Speed > 10.0f)
		{
			if (Character->SprintSystem && !Character->SprintSystem->IsSprintInterrupted())
			{
				Character->SprintSystem->SetIsSprinting(true);
				if (Character->CrouchSystem && Character->CrouchSystem->IsCrouched())
					Character->CrouchSystem->CrouchPressed(FInputActionValue());
				return EMovementStateValue::Sprinting;
			}
			else
				return EMovementStateValue::Walking;
		}
		else
			return EMovementStateValue::Idle;
	}

	return EMovementStateValue::None;
}

bool UDodgingMovementState::CanTransitionTo_Implementation(EMovementStateValue NewState) const
{
	return Super::CanTransitionTo_Implementation(NewState);
}