// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/Components/Sprint/SprintSystemComponent.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

USprintSystemComponent::USprintSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bSprintInterrupted = true;
	bIsSprinting = false;

	SetIsReplicatedByDefault(true);
}

void USprintSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USprintSystemComponent, bIsSprinting);
}

void USprintSystemComponent::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	
	if (!OwnerCharacter)
	{
		UE_LOG(LogMKHMovement, Error, TEXT("SprintSystemComponent: Owner is not a CharacterBase! Owner class: %s"),
			GetOwner() ? *GetOwner()->GetClass()->GetName() : TEXT("NULL"));
	}

	if (bSprintAsDefault)
	{
		bIsSprinting = true;
		bSprintInterrupted = false;
	}
}

void USprintSystemComponent::SetupInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SprintSystemComponent: EnhancedInputComponent is null"));
		return;
	}

	if (!SprintAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("SprintSystemComponent: SprintAction is not set"));
		return;
	}

	// Bind sprint input
	EnhancedInputComponent->BindActionValueLambda(SprintAction, ETriggerEvent::Started, 
		[this](const FInputActionValue& Value) {
			SprintPressed(Value);
		});
	EnhancedInputComponent->BindActionValueLambda(SprintAction, ETriggerEvent::Completed, 
		[this](const FInputActionValue& Value) {
			SprintPressed(Value);
		});


}

void USprintSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USprintSystemComponent::SprintPressed(const FInputActionValue& Value, const bool bOverrideSprint, const bool bValueToOverride)
{
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
		return;

	const bool bSprintValue = bOverrideSprint ? bValueToOverride : Value.Get<bool>();

	if (!Character->HasAuthority())
		ServerSprintPressed(bSprintValue);

	const bool bIntentToSprint = bSprintAsDefault ? !bSprintValue : bSprintValue;

	if (bIntentToSprint)
	{
		bSprintInterrupted = false;
		bIsSprinting = true;
		// if (Character->HasAuthority())
		// 	UE_LOG(LogTemp, Log, TEXT("Sprint started on server"));
		// Don't sprint if dodging
		if (Character->GetMovementStateMachine())
		{
			EMovementStateValue CurrentState = Character->GetMovementStateMachine()->GetCurrentState();
			if (CurrentState == EMovementStateValue::Dodging)
			{
				bIsSprinting = false;
				return;
			}

			// If crouched, try to uncrouch first
			if (CurrentState == EMovementStateValue::CrouchingIdle || CurrentState == EMovementStateValue::CrouchingMoving)
			{
				if (!Character->CrouchSystem->CanUncrouchSafely())
				{
					bIsSprinting = false;
					return;
				}
				Character->CrouchSystem->CrouchPressed(Value);
			}
		}
	}
	else
	{
		bSprintInterrupted = true;
		bIsSprinting = false;
	}
}

void USprintSystemComponent::ServerSprintPressed_Implementation(bool bSprintValue)
{
	SprintPressed(FInputActionValue(), true, bSprintValue);
}

ACharacterBase* USprintSystemComponent::GetValidCharacter() const
{
	if (LIKELY(OwnerCharacter != nullptr))
	{
		return OwnerCharacter.Get();
	}

	// Fallback: re-cache if necessary (should be very rare)
	return Cast<ACharacterBase>(GetOwner());
}
