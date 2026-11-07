// Copyright (c) 2025 Luigi Pennisi. All rights reserved.
#include "Character/Components/Dodge/DodgeSystemComponent.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/Components/BasicMovement/BasicMovementComponent.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/MovementStateMachine/MovementStateTypes.h"

// ============================================================
// Lifecycle
// ============================================================

UDodgeSystemComponent::UDodgeSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UDodgeSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDodgeSystemComponent, bIsDodging);
}

void UDodgeSystemComponent::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	
	if (!IsValid(OwnerCharacter))
	{
		UE_LOG(LogMKHMovement, Error, TEXT("DodgeSystemComponent: Owner is not a CharacterBase! Owner class: %s"),
			GetOwner() ? *GetOwner()->GetClass()->GetName() : TEXT("NULL"));
	}
}

void UDodgeSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character) || !IsValid(Character->GetCharacterMovement()))
	{
		return;
	}

	EnforceMaxDodgeDuration(*Character);
	if (bStandUpPending && !IsDodging())
	{
		StandUpAfterDodge(*Character);
	}

	// Apply movement input only where the character is locally controlled (owning player, or the server for an AI)
	// during dodge maneuver. Remote clients rely on the standard CharacterMovementComponent networking pipeline.
	if (IsDodging() && !GetDodgeDirection().IsZero() && Character->IsLocallyControlled())
	{
		UpdateDodgeDirection();
		Character->AddMovementInput(GetDodgeDirection(), 1.0f);
	}
}

// ============================================================
// Public Interface
// ============================================================

bool UDodgeSystemComponent::StartDodge()
{
	ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character))
	{
		return false;
	}

	if (IsValid(Character->GetMovementStateMachine()))
	{
		const EMovementStateValue CurrentState = Character->GetMovementStateMachine()->GetCurrentState();

		if (!IsInDodgeableState(CurrentState))
		{
			return false;
		}

		// Cache crouching state and force a crouch transition if needed before dodging. The crouch is refused from
		// Attacking and Blocking: remember whether it happened, so the end of the dodge never toggles it the wrong way.
		bWasCrouchingPreDodge = (CurrentState == EMovementStateValue::CrouchingIdle || CurrentState == EMovementStateValue::CrouchingMoving);
		bCrouchedForDodge = false;
		bStandUpPending = false;
		if (IsValid(Character->CrouchSystem) && !bWasCrouchingPreDodge && !Character->CrouchSystem->IsCrouched())
		{
			Character->CrouchSystem->CrouchPressed(FInputActionValue());
			bCrouchedForDodge = Character->CrouchSystem->IsCrouched();
		}
		UE_LOG(LogMKHMovement, Verbose, TEXT("DodgeSystemComponent: %s starts a dodge from %s (crouched for it: %d)."),
			*GetNameSafe(Character), *UMovementStateTypes::MovementStateToString(CurrentState), bCrouchedForDodge);
	}

	bIsDodging = true;
	DodgeStartTime = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
	Character->GetMovementStateMachine()->TransitionToState(EMovementStateValue::Dodging);

	// Determine dodge direction at startup: an AI's requested direction first, then input/acceleration
	if (!ApplyRequestedDodgeDirection() && !UpdateDodgeDirection())
	{
		DodgeDirection = Character->GetActorForwardVector();
	}

	InitialDodgeDirection = DodgeDirection;

	return true;
}

void UDodgeSystemComponent::ResetDodge()
{
	ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character) || !bIsDodging)
	{
		return;
	}

	bIsDodging = false;
	DodgeDirection = FVector::ZeroVector;
	InitialDodgeDirection = FVector::ZeroVector;
	RequestedDodgeDirection = FVector::ZeroVector;
	bDodgeDirectionLocked = false;

	// Stand up only if the dodge crouched the character (a crouched dodge stays crouched).
	if (bCrouchedForDodge)
	{
		bCrouchedForDodge = false;
		StandUpAfterDodge(*Character);
	}
}

bool UDodgeSystemComponent::UpdateDodgeDirection()
{
	// A requested (AI) direction is kept for the whole dodge, without blending with input.
	if (bDodgeDirectionLocked)
	{
		return true;
	}

	bool bHasInput = false;
	FVector MovementInput = FVector::ZeroVector;

	if (!TryFetchMovementInput(bHasInput, MovementInput))
	{
		return false;
	}

	// If we are starting a dodge or have no direction yet, handle initial calculation
	if (!bIsDodging || InitialDodgeDirection.IsZero())
	{
		return HandleInitialDodgeDirection(bHasInput, MovementInput);
	}

	// Blend with current input if we are already dodging
	UpdateBlendedDodgeDirection(bHasInput, MovementInput);
	
	return true;
}

void UDodgeSystemComponent::SetRequestedDodgeDirection(const FVector& WorldDirection)
{
	RequestedDodgeDirection = FVector(WorldDirection.X, WorldDirection.Y, 0.0f).GetSafeNormal();
}

bool UDodgeSystemComponent::IsInDodgeableState(EMovementStateValue CurrentState) const
{
	switch (CurrentState)
	{
		case EMovementStateValue::Idle:
		case EMovementStateValue::CrouchingIdle:
		case EMovementStateValue::CrouchingMoving:
		case EMovementStateValue::Walking:
		case EMovementStateValue::Sprinting:
		case EMovementStateValue::Blocking:
		case EMovementStateValue::Attacking:
		case EMovementStateValue::Dodging:
			return true;
		default:
			return false;
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

ACharacterBase* UDodgeSystemComponent::GetValidCharacter() const
{
	if (LIKELY(IsValid(OwnerCharacter)))
	{
		return OwnerCharacter.Get();
	}

	return Cast<ACharacterBase>(GetOwner());
}

void UDodgeSystemComponent::OnRep_bWasCrouchingPreDodge()
{
	ACharacterBase* Character = GetValidCharacter();
	if (IsValid(Character) && IsValid(Character->CrouchSystem) && !bWasCrouchingPreDodge)
	{
		Character->CrouchSystem->CrouchPressed(FInputActionValue());
	}
}

bool UDodgeSystemComponent::TryFetchMovementInput(bool& bOutHasInput, FVector& OutMovementInput) const
{
	ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character))
	{
		return false;
	}

	// Detect input source: Enhanced Input for the local player, Acceleration for remote clients and AI
	if (HasLocalPlayerInput())
	{
		if (IsValid(Character->BasicMovementSystem))
		{
			bOutHasInput = Character->BasicMovementSystem->HasMovementInput();
			OutMovementInput = Character->BasicMovementSystem->GetCurrentMovementInput();
			return true;
		}
	}
	else
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			OutMovementInput = MoveComp->GetCurrentAcceleration().GetSafeNormal();
			bOutHasInput = !OutMovementInput.IsNearlyZero();
			return true;
		}
	}

	return false;
}

bool UDodgeSystemComponent::HasLocalPlayerInput() const
{
	const ACharacterBase* Character = GetValidCharacter();

	// An AI is locally controlled on the server but has no Enhanced Input.
	return IsValid(Character) && Character->IsLocallyControlled() && Character->IsPlayerControlled();
}

bool UDodgeSystemComponent::ApplyRequestedDodgeDirection()
{
	if (RequestedDodgeDirection.IsNearlyZero())
	{
		return false;
	}

	DodgeDirection = RequestedDodgeDirection;
	RequestedDodgeDirection = FVector::ZeroVector;
	bDodgeDirectionLocked = true;
	return true;
}

FVector UDodgeSystemComponent::CalculateLocalInputDirection(const FVector& MovementInput) const
{
	ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character) || !IsValid(Character->Controller))
	{
		return FVector::ZeroVector;
	}

	const FRotator Rotation = Character->Controller->GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
	
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	return (Forward * MovementInput.X + Right * MovementInput.Y).GetSafeNormal();
}

bool UDodgeSystemComponent::HandleInitialDodgeDirection(bool bHasInput, const FVector& MovementInput)
{
	if (!(bHasInput && !MovementInput.IsNearlyZero()))
	{
		return false;
	}

	if (HasLocalPlayerInput())
	{
		DodgeDirection = CalculateLocalInputDirection(MovementInput);
	}
	else
	{
		DodgeDirection = MovementInput;
	}

	return true;
}

void UDodgeSystemComponent::StandUpAfterDodge(ACharacterBase& Character)
{
	UCrouchSystemComponent* CrouchSystem = Character.CrouchSystem;
	if (!IsValid(CrouchSystem) || !CrouchSystem->IsCrouched())
	{
		bStandUpPending = false;
		return;
	}

	// ForceUncrouch checks the headroom (and no toggle: it never crouches a standing character).
	CrouchSystem->ForceUncrouch();
	const bool bWasPending = bStandUpPending;
	bStandUpPending = CrouchSystem->IsCrouched();
	if (bStandUpPending != bWasPending)
	{
		UE_LOG(LogMKHMovement, Log, TEXT("DodgeSystemComponent: %s %s after the dodge."), *GetNameSafe(&Character),
			bStandUpPending ? TEXT("has no headroom to stand up, retrying") : TEXT("stands up"));
	}
}

void UDodgeSystemComponent::EnforceMaxDodgeDuration(const ACharacterBase& Character)
{
	const UWorld* World = GetWorld();
	if (!IsDodging() || !IsValid(World) || World->GetTimeSeconds() - DodgeStartTime < MaxDodgeDuration)
	{
		return;
	}
	UE_LOG(LogMKHMovement, Warning, TEXT("DodgeSystemComponent: dodge of %s still running after %.1f s, reset (GA_Dodge did not end)."),
		*GetNameSafe(&Character), MaxDodgeDuration);
	ResetDodge();
}

void UDodgeSystemComponent::UpdateBlendedDodgeDirection(bool bHasInput, const FVector& MovementInput)
{
	FVector CurrentInputDirection = FVector::ZeroVector;
	
	if (bHasInput && !MovementInput.IsNearlyZero())
	{
		CurrentInputDirection = HasLocalPlayerInput() ? CalculateLocalInputDirection(MovementInput) : MovementInput;
	}
	
	if (CurrentInputDirection.IsZero())
	{
		DodgeDirection = InitialDodgeDirection;
	}
	else
	{
		DodgeDirection = FMath::Lerp(InitialDodgeDirection, CurrentInputDirection, InputInfluenceFactor).GetSafeNormal();
	}
}
