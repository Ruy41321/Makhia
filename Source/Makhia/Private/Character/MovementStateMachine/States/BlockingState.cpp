// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/States/BlockingState.h"
#include "Character/CharacterBase.h"
#include "Character/Components/Blocking/BlockingSystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/Components/Dodge/DodgeSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

UBlockingState::UBlockingState()
{
}

void UBlockingState::EnterState(EMovementStateValue PreviousState)
{
	Super::EnterState(PreviousState);

	if (ACharacterBase* Character = GetOwnerCharacter())
	{
		if (UBlockingSystemComponent* BlockingSystem = Character->FindComponentByClass<UBlockingSystemComponent>())
		{
			BlockingSystem->ApplyBlockingStateModifiers();
		}
	}
}

void UBlockingState::ExitState(EMovementStateValue NextState)
{
	Super::ExitState(NextState);

	if (ACharacterBase* Character = GetOwnerCharacter())
	{
		if (UBlockingSystemComponent* BlockingSystem = Character->FindComponentByClass<UBlockingSystemComponent>())
		{
			BlockingSystem->RemoveBlockingStateModifiers();
		}
	}
}

EMovementStateValue UBlockingState::GetDesiredTransition_Implementation() const
{
	ACharacterBase* Character = GetOwnerCharacter();
	if (!Character || !Character->GetCharacterMovement())
		return EMovementStateValue::None;

	// Check for attacking
	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		// if it has attacking and a Priority tag
		if ( ASC->HasMatchingGameplayTag(MKHGameplayTags::Ability::Attacking) &&
			(
				ASC->HasMatchingGameplayTag(MKHGameplayTags::Ability::Priority1) ||
				ASC->HasMatchingGameplayTag(MKHGameplayTags::Ability::Priority2)
			)
		   )
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


	UBlockingSystemComponent* BlockingSystem = Character->FindComponentByClass<UBlockingSystemComponent>();
	if (!IsValid(BlockingSystem) || !BlockingSystem->IsBlocking())
	{
		// Fallback to idle if block is released (the basic states will transition from Idle to Walk/Sprint depending on velocity)
		return EMovementStateValue::Idle;
	}
	
	// Remain in Blocking state
	return EMovementStateValue::None;
}

bool UBlockingState::CanTransitionTo_Implementation(EMovementStateValue NewState) const
{
	return Super::CanTransitionTo_Implementation(NewState);
}
