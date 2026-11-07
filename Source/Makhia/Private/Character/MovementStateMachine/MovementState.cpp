// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/MovementState.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"
#include "Character/CharacterBase.h"
// Include component headers for speed getters
#include "Character/Components/Dodge/DodgeSystemComponent.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/Components/Sprint/SprintSystemComponent.h"
#include "Character/Components/BasicMovement/BasicMovementComponent.h"
#include "Character/Components/Blocking/BlockingSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

UMovementState::UMovementState()
{
	StateMachine = nullptr;
	OwnerCharacter = nullptr;
}

void UMovementState::Initialize(UMovementStateMachine* InStateMachine, ACharacterBase* InCharacter)
{
	StateMachine = InStateMachine;
	OwnerCharacter = InCharacter;
}

bool UMovementState::CanTransitionTo_Implementation(EMovementStateValue NewState) const
{
	// Default implementation allows all transitions
	return true;
}

EMovementStateValue UMovementState::GetDesiredTransition_Implementation() const
{
	// Default implementation: no automatic transitions
	return EMovementStateValue::None;
}

void UMovementState::EnterState(EMovementStateValue PreviousState)
{
	// Set the appropriate speed for this state
	SetStateSpeed();

	// Add the movement state gameplay tag to the ASC
	if (ACharacterBase* Character = GetOwnerCharacter())
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			const FGameplayTag StateTag = GetTagForState(GetStateType());
			if (StateTag.IsValid())
			{
				ASC->AddLooseGameplayTag(StateTag);
			}
		}
	}
	
	// Call Blueprint event
	OnEnterState(PreviousState);
}

void UMovementState::UpdateState(float DeltaTime)
{
	// Call Blueprint event
	OnUpdateState(DeltaTime);
}

void UMovementState::ExitState(EMovementStateValue NextState)
{
	// Remove the movement state gameplay tag from the ASC
	if (ACharacterBase* Character = GetOwnerCharacter())
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			const FGameplayTag StateTag = GetTagForState(GetStateType());
			if (StateTag.IsValid() && ASC->HasMatchingGameplayTag(StateTag))
			{
				ASC->RemoveLooseGameplayTag(StateTag);
			}
		}
	}

	// Call Blueprint event
	OnExitState(NextState);
}

void UMovementState::SetStateSpeed()
{
	ACharacterBase* Character = GetOwnerCharacter();
	if (!Character)
		return;

	EMovementStateValue StateType = GetStateType();
	float Speed = GetSpeedForState(StateType);
	
	Character->SetMaxWalkSpeed(Speed);
}

float UMovementState::GetSpeedForState(EMovementStateValue StateType) const
{
	ACharacterBase* Character = GetOwnerCharacter();
	if (!Character)
		return 300.0f; // Default fallback speed

	switch (StateType)
	{
	case EMovementStateValue::Dodging:
		// Get dodge speed from DodgeSystem
		if (Character->DodgeSystem)
			return Character->DodgeSystem->GetDodgeSpeed();
		return 600.0f; // Fallback dodge speed
		
	case EMovementStateValue::CrouchingIdle:
	case EMovementStateValue::CrouchingMoving:
		// Get crouch speed from CrouchSystem
		if (Character->CrouchSystem)
			return Character->CrouchSystem->GetCrouchSpeed();
		return 150.0f; // Fallback crouch speed
		
	case EMovementStateValue::Sprinting:
		// Get run speed from SprintSystem
		if (Character->SprintSystem)
			return Character->SprintSystem->GetRunSpeed();
		return 600.0f; // Fallback sprint speed
		
	case EMovementStateValue::Blocking:
		// Get block speed from BlockingSystem
		if (Character->BlockingSystem)
			return Character->BlockingSystem->GetBlockingSpeed();
		return 100.0f; // Fallback block speed
		
	case EMovementStateValue::Attacking:
	case EMovementStateValue::Dead:
		return 0.0f;
		
	default:
		// Get walk speed from BasicMovementSystem
		if (Character->BasicMovementSystem)
			return Character->BasicMovementSystem->GetWalkSpeed();
		return 300.0f; // Fallback walk speed
	}
}

FGameplayTag UMovementState::GetTagForState(EMovementStateValue StateType)
{
	using namespace MKHGameplayTags::State::Movement;

	switch (StateType)
	{
	case EMovementStateValue::Idle:            return Idle;
	case EMovementStateValue::Walking:         return Walking;
	case EMovementStateValue::Sprinting:       return Sprinting;
	case EMovementStateValue::CrouchingIdle:   return CrouchingIdle;
	case EMovementStateValue::CrouchingMoving: return CrouchingMoving;
	case EMovementStateValue::Jumping:         return Jumping;
	case EMovementStateValue::Falling:         return Falling;
	case EMovementStateValue::LandingInPlace:  return LandingInPlace;
	case EMovementStateValue::LandingMoving:   return LandingMoving;
	case EMovementStateValue::Dodging:         return Dodging;
	case EMovementStateValue::Blocking:        return Blocking;
	case EMovementStateValue::Attacking:       return Attacking;
	case EMovementStateValue::Dead:            return Dead;
	default:                                   return FGameplayTag();
	}
}

