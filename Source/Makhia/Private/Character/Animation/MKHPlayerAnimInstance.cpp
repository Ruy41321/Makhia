// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/Animation/MKHPlayerAnimInstance.h"

#include "KismetAnimationLibrary.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/CharacterBase.h"

void UMKHPlayerAnimInstance::NativeUninitializeAnimation()
{
	// Unsubscribe from movement state changes
	UnsubscribeFromMovementStateChanges();

	Super::NativeUninitializeAnimation();
}

void UMKHPlayerAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();
	OwnerCharacter = Cast<ACharacterBase>(TryGetPawnOwner());
	CurrentMovementState = EMovementStateValue::None;
	PreviousMovementState = EMovementStateValue::None;
	CrouchingTransitionTarget = 0.0f;
	
	SubscribeToMovementStateChanges();
}

void UMKHPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (!IsValid(OwnerCharacter))
		return;
	
	CurrentSpeed = OwnerCharacter->GetVelocity().Size();
	Direction = UKismetAnimationLibrary::CalculateDirection(OwnerCharacter->GetVelocity(), OwnerCharacter->GetActorRotation());
	
	bIsHoldingWeapon = OwnerCharacter->bIsHoldingWeapon;
}

void UMKHPlayerAnimInstance::OnMovementStateChanged(EMovementStateValue OldState, EMovementStateValue NewState)
{
	// Update movement state properties when notified of changes
	PreviousMovementState = OldState;
	CurrentMovementState = NewState;
	
	// Update crouching transition target based on current state
	if (OwnerCharacter && OwnerCharacter->CrouchSystem)
	{
		CrouchingTransitionTarget = OwnerCharacter->CrouchSystem->IsCrouched() ? 100.0f : 0.0f;
		CrouchingTransitionTime = CrouchingTransitionTarget; // Snap immediately to target for instant transitions
	}
}

void UMKHPlayerAnimInstance::SubscribeToMovementStateChanges()
{
	if (!OwnerCharacter || !OwnerCharacter->GetMovementStateMachine() || bIsSubscribedToStateChanges)
		return;
	
	OwnerCharacter->GetMovementStateMachine()->OnStateChanged.AddDynamic(this, &UMKHPlayerAnimInstance::OnMovementStateChanged);
	bIsSubscribedToStateChanges = true;
	
	CurrentMovementState = OwnerCharacter->GetMovementStateMachine()->GetCurrentState();
	PreviousMovementState = OwnerCharacter->GetMovementStateMachine()->GetPreviousState();
}

void UMKHPlayerAnimInstance::UnsubscribeFromMovementStateChanges()
{
	if (!bIsSubscribedToStateChanges || !OwnerCharacter || !OwnerCharacter->GetMovementStateMachine())
		return;
	
	OwnerCharacter->GetMovementStateMachine()->OnStateChanged.RemoveDynamic(this, &UMKHPlayerAnimInstance::OnMovementStateChanged);
	bIsSubscribedToStateChanges = false;
}

bool UMKHPlayerAnimInstance::IsMovementStateDataValid() const
{
	return CurrentMovementState != EMovementStateValue::None && 
		   OwnerCharacter != nullptr && 
		   OwnerCharacter->GetMovementStateMachine() != nullptr &&
		   bIsSubscribedToStateChanges;
}

FString UMKHPlayerAnimInstance::GetCurrentMovementStateString() const
{
	return UMovementStateTypes::MovementStateToString(CurrentMovementState);
}

FString UMKHPlayerAnimInstance::GetPreviousMovementStateString() const
{
	return UMovementStateTypes::MovementStateToString(PreviousMovementState);
}