// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Input/MKHInputModifierHalfAxis.h"

#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "UI/Navigation/MKHMenuNavigationSubsystem.h"

FInputActionValue UMKHInputModifierHalfAxis::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	const APlayerController* PlayerController = IsValid(PlayerInput) ? PlayerInput->GetOuterAPlayerController() : nullptr;
	const UMKHMenuNavigationSubsystem* MenuNavigation = UMKHMenuNavigationSubsystem::Get(PlayerController);
	if (IsValid(MenuNavigation) && MenuNavigation->IsGamepadNavigationActive())
	{
		return FInputActionValue(CurrentValue.GetValueType(), FVector::ZeroVector);
	}

	FVector Value = CurrentValue.Get<FVector>();
	const float Signed = bNegativeHalf ? -Value.X : Value.X;
	Value.X = FMath::Max(Signed, 0.0f);

	return FInputActionValue(CurrentValue.GetValueType(), Value);
}
