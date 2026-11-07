// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/MovementStateMachine.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "Character/MovementStateMachine/MovementState.h"
#include "Character/MovementStateMachine/States/IdleMovementState.h"
#include "Character/MovementStateMachine/States/WalkingMovementState.h"
#include "Character/MovementStateMachine/States/SprintingMovementState.h"
#include "Character/MovementStateMachine/States/CrouchingIdleMovementState.h"
#include "Character/MovementStateMachine/States/CrouchingMovingMovementState.h"
#include "Character/MovementStateMachine/States/JumpingMovementState.h"
#include "Character/MovementStateMachine/States/FallingMovementState.h"
#include "Character/MovementStateMachine/States/LandingInPlaceMovementState.h"
#include "Character/MovementStateMachine/States/LandingMovingMovementState.h"
#include "Character/MovementStateMachine/States/DodgingMovementState.h"
#include "Character/MovementStateMachine/States/BlockingState.h"
#include "Character/MovementStateMachine/States/AttackingMovementState.h"
#include "AbilitySystemComponent.h"
#include "Character/MovementStateMachine/States/DeadState.h"

UMovementStateMachine::UMovementStateMachine()
{
	PrimaryComponentTick.bCanEverTick = true;
	CurrentState = EMovementStateValue::None;
	PreviousState = EMovementStateValue::None;
}

void UMovementStateMachine::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	
	if (!OwnerCharacter)
	{
		UE_LOG(LogMKHMovement, Error, TEXT("MovementStateMachine: Owner is not a CharacterBase!"));
		return;
	}

	InitializeDefaultStates();
	
	// Start with Idle state
	TransitionToState(EMovementStateValue::Idle, true);
}

void UMovementStateMachine::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update current state
	if (UMovementState* CurrentStateObject = GetCurrentStateObject())
	{
		CurrentStateObject->UpdateState(DeltaTime);
	}

	// Evaluate automatic transitions
	EvaluateStateTransitions();
}

bool UMovementStateMachine::TransitionToState(EMovementStateValue NewState, bool bForceTransition)
{
	if (NewState == CurrentState)
	{
		return false; // Already in the desired state
	}

	if (!bForceTransition && !CanTransitionToState(NewState))
	{
		return false; // Transition not allowed
	}

	PerformStateTransition(NewState);
	return true;
}

bool UMovementStateMachine::CanTransitionToState(EMovementStateValue NewState) const
{
	if (UMovementState* CurrentStateObject = GetCurrentStateObject())
	{
		return CurrentStateObject->CanTransitionTo(NewState);
	}
	
	return true; // If no current state, allow transition
}

void UMovementStateMachine::RegisterState(EMovementStateValue StateType, TSubclassOf<UMovementState> StateClass)
{
	if (StateClass)
	{
		DefaultStateClasses.Add(StateType, StateClass);
		
		// If we're already initialized, create the state object immediately
		if (OwnerCharacter)
		{
			if (UMovementState* NewStateObject = CreateStateObject(StateType, StateClass))
			{
				StateObjects.Add(StateType, NewStateObject);
			}
		}
	}
}

UMovementState* UMovementStateMachine::GetCurrentStateObject() const
{
	if (StateObjects.Contains(CurrentState))
	{
		return StateObjects[CurrentState].Get();
	}
	return nullptr;
}

void UMovementStateMachine::InitializeDefaultStates()
{
	// Set default state classes if not overridden
	if (!DefaultStateClasses.Contains(EMovementStateValue::Idle))
		DefaultStateClasses.Add(EMovementStateValue::Idle, UIdleMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::Walking))
		DefaultStateClasses.Add(EMovementStateValue::Walking, UWalkingMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::Sprinting))
		DefaultStateClasses.Add(EMovementStateValue::Sprinting, USprintingMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::CrouchingIdle))
		DefaultStateClasses.Add(EMovementStateValue::CrouchingIdle, UCrouchingIdleMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::CrouchingMoving))
		DefaultStateClasses.Add(EMovementStateValue::CrouchingMoving, UCrouchingMovingMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::Jumping))
		DefaultStateClasses.Add(EMovementStateValue::Jumping, UJumpingMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::Falling))
		DefaultStateClasses.Add(EMovementStateValue::Falling, UFallingMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::LandingInPlace))
		DefaultStateClasses.Add(EMovementStateValue::LandingInPlace, ULandingInPlaceMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::LandingMoving))
		DefaultStateClasses.Add(EMovementStateValue::LandingMoving, ULandingMovingMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::Dodging))
		DefaultStateClasses.Add(EMovementStateValue::Dodging, UDodgingMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::Blocking))
		DefaultStateClasses.Add(EMovementStateValue::Blocking, UBlockingState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::Attacking))
		DefaultStateClasses.Add(EMovementStateValue::Attacking, UAttackingMovementState::StaticClass());
	if (!DefaultStateClasses.Contains(EMovementStateValue::Dead))
		DefaultStateClasses.Add(EMovementStateValue::Dead, UDeadState::StaticClass());

	// Create state objects
	for (const auto& StateClassPair : DefaultStateClasses)
	{
		if (UMovementState* NewStateObject = CreateStateObject(StateClassPair.Key, StateClassPair.Value))
		{
			StateObjects.Add(StateClassPair.Key, NewStateObject);
		}
	}
}

UMovementState* UMovementStateMachine::CreateStateObject(EMovementStateValue StateType, TSubclassOf<UMovementState> StateClass)
{
	if (!StateClass || !OwnerCharacter)
	{
		return nullptr;
	}

	UMovementState* NewStateObject = NewObject<UMovementState>(this, StateClass);
	if (NewStateObject)
	{
		NewStateObject->Initialize(this, OwnerCharacter);
	}

	return NewStateObject;
}

ACharacterBase* UMovementStateMachine::GetValidCharacter() const
{
	return OwnerCharacter.Get();
}

void UMovementStateMachine::EvaluateStateTransitions()
{
	if (UMovementState* CurrentStateObject = GetCurrentStateObject())
	{
		EMovementStateValue DesiredState = CurrentStateObject->GetDesiredTransition();
		if (DesiredState != EMovementStateValue::None && DesiredState != CurrentState)
		{
			TransitionToState(DesiredState);
		}
	}
}

void UMovementStateMachine::PerformStateTransition(EMovementStateValue NewState)
{
	EMovementStateValue OldState = CurrentState;

	// Exit current state
	if (UMovementState* CurrentStateObject = GetCurrentStateObject())
	{
		CurrentStateObject->ExitState(NewState);
	}

	// Update states
	PreviousState = CurrentState;
	CurrentState = NewState;

	// Enter new state
	if (UMovementState* NewStateObject = GetCurrentStateObject())
	{
		NewStateObject->EnterState(PreviousState);
	}

	// Broadcast state change
	OnStateChanged.Broadcast(OldState, NewState);
	// stampa server: jumpcomponent->bIsLanding o client: Character->bIsLanding
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
		return;

	// UE_LOG(LogTemp, Log, TEXT("MovementStateMachine: Transitioned from %s to %s"), 
	// 	*UMovementStateTypes::MovementStateToString(OldState),
	// 	*UMovementStateTypes::MovementStateToString(NewState));
}

void UMovementStateMachine::UnsubscribeFromStateChanges(UObject* Subscriber)
{
	if (!Subscriber)
	{
		UE_LOG(LogTemp, Warning, TEXT("MovementStateMachine: Cannot unsubscribe null object from state changes"));
		return;
	}

	// Remove all dynamic delegates for this subscriber
	OnStateChanged.RemoveAll(Subscriber);

	UE_LOG(LogTemp, Log, TEXT("MovementStateMachine: Unsubscribed '%s' from state changes"), 
		*Subscriber->GetClass()->GetName());
}

void UMovementStateMachine::SyncCurrentStateTagToASC()
{
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		const FGameplayTag StateTag = UMovementState::GetTagForState(CurrentState);
		if (StateTag.IsValid() && !ASC->HasMatchingGameplayTag(StateTag))
		{
			ASC->AddLooseGameplayTag(StateTag);
		}
	}
}
