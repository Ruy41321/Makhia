// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/Components/Jump/JumpSystemComponent.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "InputActionValue.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

UJumpSystemComponent::UJumpSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UJumpSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJumpSystemComponent, bIsLanding);
}

void UJumpSystemComponent::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	
	// Validation for safety in development
	if (!OwnerCharacter)
	{
		UE_LOG(LogMKHMovement, Error, TEXT("JumpSystemComponent: Owner is not a CharacterBase! Owner class: %s"),
			GetOwner() ? *GetOwner()->GetClass()->GetName() : TEXT("NULL"));
	}
}

void UJumpSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Jump system doesn't need constant tick updates
	// All logic is event-driven through input and landing callbacks
}

void UJumpSystemComponent::JumpPressed(const FInputActionValue& Value)
{
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
		return;

	// Crowd-control states (Stunned / Staggered) must suppress any movement action.
	if (IsCrowdControlled(Character))
		return;

	if (Character->GetMovementStateMachine())
	{
		EMovementStateValue CurrentState = Character->GetMovementStateMachine()->GetCurrentState();
				
		if (bIsLanding || CurrentState == EMovementStateValue::Dodging
			|| CurrentState == EMovementStateValue::Dead || CurrentState == EMovementStateValue::Attacking)
			return;

		// If crouched, uncrouch instead of jumping
		if ((CurrentState == EMovementStateValue::CrouchingIdle || CurrentState == EMovementStateValue::CrouchingMoving) 
			&& Character->CrouchSystem)
		{
			Character->CrouchSystem->CrouchPressed(Value);
			return;
		}
	}
	// Perform the jump
	Character->GetMovementStateMachine()->TransitionToState(EMovementStateValue::Jumping);
	Character->Jump();
}

void UJumpSystemComponent::OnLanded(const FHitResult& Hit)
{
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
		return;

	bIsLanding = true;
	
	// Set a timer to reset landing state
	GetWorld()->GetTimerManager().SetTimer(LandingTimerHandle, this, &UJumpSystemComponent::ResetLanding, LandingResetTime, false);
	
	UE_LOG(LogTemp, Verbose, TEXT("JumpSystem: Character landed, setting landing state for %f seconds"), LandingResetTime);
}

void UJumpSystemComponent::ResetLanding()
{
	bIsLanding = false;
}

void UJumpSystemComponent::InterruptJump()
{
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
		return;

	// Release the sustained jump so hold-to-jump height accumulation stops.
	Character->StopJumping();

	// Cancel any remaining upward velocity so the character starts falling immediately.
	// The movement state machine will then transition Jumping -> Falling on its own.
	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	if (IsValid(MovementComponent) && MovementComponent->IsFalling() && MovementComponent->Velocity.Z > 0.0f)
	{
		MovementComponent->Velocity.Z = 0.0f;
	}
}

void UJumpSystemComponent::SetupInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpSystemComponent: EnhancedInputComponent is null"));
		return;
	}

	if (!JumpPressedAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpSystemComponent: JumpPressedAction is not set"));
		return;
	}

	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpSystemComponent: Character is not available for input binding"));
		return;
	}

	// Bind jump input
	EnhancedInputComponent->BindActionValueLambda(JumpPressedAction, ETriggerEvent::Started, 
		[this](const FInputActionValue& Value) {
			JumpPressed(Value);
		});
	EnhancedInputComponent->BindAction(JumpPressedAction, ETriggerEvent::Completed, Character, &ACharacter::StopJumping);


}

bool UJumpSystemComponent::IsCrowdControlled(const ACharacterBase* Character) const
{
	if (!IsValid(Character))
		return false;

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!IsValid(ASC))
		return false;

	// Actions are blocked while the character is under any crowd-control effect.
	static FGameplayTagContainer CrowdControlTags;
	if (CrowdControlTags.IsEmpty())
	{
		CrowdControlTags.AddTag(MKHGameplayTags::State::Stunned);
		CrowdControlTags.AddTag(MKHGameplayTags::State::Staggered);
	}

	return ASC->HasAnyMatchingGameplayTags(CrowdControlTags);
}

ACharacterBase* UJumpSystemComponent::GetValidCharacter() const
{
	if (LIKELY(OwnerCharacter != nullptr))
	{
		return OwnerCharacter.Get();
	}

	// Fallback: re-cache if necessary (should be very rare)
	return Cast<ACharacterBase>(GetOwner());
}