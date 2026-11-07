// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/Components/BasicMovement/BasicMovementComponent.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "InputActionValue.h"

// ============================================================
// Lifecycle
// ============================================================

UBasicMovementComponent::UBasicMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	MouseSensitivity = 1.0f;
	bInvertYAxis = false;
	MovementSmoothing = 0.1f;
}

void UBasicMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacterBase>(GetOwner());

	if (!IsValid(OwnerCharacter))
	{
		UE_LOG(LogMKHMovement, Error, TEXT("BasicMovementComponent: Owner is not a CharacterBase! Owner class: %s"),
			GetOwner() ? *GetOwner()->GetClass()->GetName() : TEXT("NULL"));
	}
}

void UBasicMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateMovementVelocity();
}

// ============================================================
// Public Interface - Input
// ============================================================

void UBasicMovementComponent::SetupInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!IsValid(EnhancedInputComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("BasicMovementComponent: EnhancedInputComponent is null"));
		return;
	}

	BindDirectionalAction(EnhancedInputComponent, MoveForwardAction, EMovementInputDirection::Forward);
	BindDirectionalAction(EnhancedInputComponent, MoveBackwardAction, EMovementInputDirection::Backward);
	BindDirectionalAction(EnhancedInputComponent, MoveRightAction, EMovementInputDirection::Right);
	BindDirectionalAction(EnhancedInputComponent, MoveLeftAction, EMovementInputDirection::Left);

	if (IsValid(LookGamepadAction))
	{
		EnhancedInputComponent->BindActionValueLambda(LookGamepadAction, ETriggerEvent::Triggered,
			[this](const FInputActionValue& Value) {
				LookGamepad(Value);
			});
	}

	if (!IsValid(LookAction))
	{
		UE_LOG(LogTemp, Warning, TEXT("BasicMovementComponent: LookAction is not set"));
		return;
	}

	EnhancedInputComponent->BindActionValueLambda(LookAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value) {
			Look(Value);
		});
}

void UBasicMovementComponent::MoveForward(const FInputActionValue& Value)
{
	HandleDirectionalInput(EMovementInputDirection::Forward, Value);
}

void UBasicMovementComponent::MoveBackward(const FInputActionValue& Value)
{
	HandleDirectionalInput(EMovementInputDirection::Backward, Value);
}

void UBasicMovementComponent::MoveRight(const FInputActionValue& Value)
{
	HandleDirectionalInput(EMovementInputDirection::Right, Value);
}

void UBasicMovementComponent::MoveLeft(const FInputActionValue& Value)
{
	HandleDirectionalInput(EMovementInputDirection::Left, Value);
}

void UBasicMovementComponent::Look(const FInputActionValue& Value)
{
	// Sensitivity and inversion belong to the player, not to this component: the values below
	// are only the fallback used when the user settings are unavailable.
	float Sensitivity = MouseSensitivity;
	bool bInvertY = bInvertYAxis;

	if (const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		Sensitivity = UserSettings->GetMouseSensitivity();
		bInvertY = UserSettings->IsYAxisInverted();
	}

	ApplyLookInput(Value.Get<FVector2D>(), Sensitivity, bInvertY);
}

void UBasicMovementComponent::LookGamepad(const FInputActionValue& Value)
{
	// Same fallback values as the mouse when the user settings are unavailable.
	float Sensitivity = MouseSensitivity;
	bool bInvertY = bInvertYAxis;

	if (const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		Sensitivity = UserSettings->GetGamepadLookSensitivity();
		bInvertY = UserSettings->IsGamepadYAxisInverted();
	}

	ApplyLookInput(Value.Get<FVector2D>(), Sensitivity, bInvertY);
}

void UBasicMovementComponent::ApplyLookInput(const FVector2D& LookAxis, float Sensitivity, bool bInvertY) const
{
	ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character) || !IsValid(Character->GetController()))
		return;

	const float YawInput = LookAxis.X * Sensitivity;
	float PitchInput = LookAxis.Y * Sensitivity;

	if (bInvertY)
		PitchInput *= -1.0f;

	Character->AddControllerYawInput(YawInput);
	Character->AddControllerPitchInput(PitchInput);
}

void UBasicMovementComponent::OnDirectionalInputCompleted(EMovementInputDirection Direction)
{
	SetDirectionalInputMagnitude(Direction, 0.0f);
}

// ============================================================
// Public Interface - Queries
// ============================================================

void UBasicMovementComponent::UpdateMovementVelocity()
{
	const ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character))
		return;

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!IsValid(Movement))
		return;

	MovementSpeed2D = FVector2D(Movement->Velocity.X, Movement->Velocity.Y).Size();

	// No velocity means no held direction can be producing movement anymore.
	if (MovementSpeed2D <= 0.0f)
	{
		CurrentMovementInput = FVector::ZeroVector;
		bHasMovementInput = false;
	}
}

ACharacterBase* UBasicMovementComponent::GetValidCharacter() const
{
	if (LIKELY(OwnerCharacter != nullptr))
	{
		return OwnerCharacter.Get();
	}

	// Fallback: re-cache if necessary (should be very rare)
	return Cast<ACharacterBase>(GetOwner());
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UBasicMovementComponent::HandleDirectionalInput(EMovementInputDirection Direction, const FInputActionValue& Value)
{
	ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character))
		return;

	// Each action is one-way: only its magnitude matters, the sign comes from the direction.
	const float Magnitude = FMath::Clamp(FMath::Abs(Value.Get<float>()), 0.0f, 1.0f);
	SetDirectionalInputMagnitude(Direction, Magnitude);

	if (Magnitude <= 0.0f || !CanApplyMovementInput())
		return;

	const FVector WorldDirection = GetWorldDirection(Direction);
	if (!WorldDirection.IsNearlyZero())
	{
		Character->AddMovementInput(WorldDirection, Magnitude);
	}
}

void UBasicMovementComponent::SetDirectionalInputMagnitude(EMovementInputDirection Direction, float Magnitude)
{
	const float ClampedMagnitude = FMath::Clamp(Magnitude, 0.0f, 1.0f);

	switch (Direction)
	{
	case EMovementInputDirection::Forward:	ForwardInputMagnitude = ClampedMagnitude;	break;
	case EMovementInputDirection::Backward:	BackwardInputMagnitude = ClampedMagnitude;	break;
	case EMovementInputDirection::Right:	RightInputMagnitude = ClampedMagnitude;		break;
	case EMovementInputDirection::Left:		LeftInputMagnitude = ClampedMagnitude;		break;
	}

	RefreshCompositeMovementInput();
}

void UBasicMovementComponent::RefreshCompositeMovementInput()
{
	// Opposing directions cancel out, matching the old single-axis value.
	CurrentMovementInput.X = ForwardInputMagnitude - BackwardInputMagnitude;
	CurrentMovementInput.Y = RightInputMagnitude - LeftInputMagnitude;
	CurrentMovementInput.Z = 0.0f;

	bHasMovementInput = !CurrentMovementInput.IsNearlyZero();
}

FVector UBasicMovementComponent::GetWorldDirection(EMovementInputDirection Direction) const
{
	const ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character) || !IsValid(Character->GetController()))
		return FVector::ZeroVector;

	const FRotator YawRotation(0.f, Character->GetController()->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix RotationMatrix(YawRotation);

	switch (Direction)
	{
	case EMovementInputDirection::Forward:	return RotationMatrix.GetUnitAxis(EAxis::X);
	case EMovementInputDirection::Backward:	return -RotationMatrix.GetUnitAxis(EAxis::X);
	case EMovementInputDirection::Right:	return RotationMatrix.GetUnitAxis(EAxis::Y);
	case EMovementInputDirection::Left:		return -RotationMatrix.GetUnitAxis(EAxis::Y);
	}

	return FVector::ZeroVector;
}

bool UBasicMovementComponent::CanApplyMovementInput() const
{
	const ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character) || !IsValid(Character->MovementStateMachine))
		return false;

	return Character->MovementStateMachine->GetCurrentState() != EMovementStateValue::Dodging;
}

void UBasicMovementComponent::BindDirectionalAction(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* Action, EMovementInputDirection Direction)
{
	if (!IsValid(Action))
	{
		UE_LOG(LogTemp, Warning, TEXT("BasicMovementComponent: directional input action for %s is not set"),
			*UEnum::GetValueAsString(Direction));
		return;
	}

	EnhancedInputComponent->BindActionValueLambda(Action, ETriggerEvent::Triggered,
		[this, Direction](const FInputActionValue& Value) {
			HandleDirectionalInput(Direction, Value);
		});
	EnhancedInputComponent->BindActionValueLambda(Action, ETriggerEvent::Completed,
		[this, Direction](const FInputActionValue& Value) {
			OnDirectionalInputCompleted(Direction);
		});
}
