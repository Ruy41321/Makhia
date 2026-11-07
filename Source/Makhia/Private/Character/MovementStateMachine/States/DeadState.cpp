// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/MovementStateMachine/States/DeadState.h"

#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Character/CharacterBase.h"
#include "Player/PlayerController/MKHPlayerController.h"

UDeadState::UDeadState()
{
}

void UDeadState::EnterState(EMovementStateValue PreviousState)
{
	Super::EnterState(PreviousState);


	if (OwnerCharacter && OwnerCharacter->GetCapsuleComponent())
	{
		//Disable player collision
		OwnerCharacter->GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		OwnerCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
		OwnerCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Block);
	}

	// Strip the gameplay bindings so a dead character cannot move, look around or trigger actions.
	SetOwnerGameplayInputEnabled(false);
}

void UDeadState::ExitState(EMovementStateValue NextState)
{
	Super::ExitState(NextState);

	// Re-enable the capsule collision disabled by EnterState. Exiting the Dead state only happens
	// through a forced transition (e.g. a revive between duel rounds), so restoring the standard
	// Pawn collision profile here brings the character fully back to a collidable, playable state.
	if (IsValid(OwnerCharacter) && OwnerCharacter->GetCapsuleComponent())
	{
		OwnerCharacter->GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
		OwnerCharacter->IgnoreCameraCollision();
	}

	// Give the bindings back to the revived character.
	SetOwnerGameplayInputEnabled(true);
}

bool UDeadState::CanTransitionTo_Implementation(EMovementStateValue NewState) const
{
	return false;
}

void UDeadState::SetOwnerGameplayInputEnabled(const bool bEnabled) const
{
	// Only a player has gameplay input: an AI is locally controlled on the server but has no player controller.
	if (!IsValid(OwnerCharacter) || !OwnerCharacter->IsLocallyControlled() || !OwnerCharacter->IsPlayerControlled())
	{
		return;
	}

	if (AMKHPlayerController* MKHPlayerController = Cast<AMKHPlayerController>(OwnerCharacter->GetController()))
	{
		MKHPlayerController->SetGameplayInputEnabled(bEnabled);
	}
}
