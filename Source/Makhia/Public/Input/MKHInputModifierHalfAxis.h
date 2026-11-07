// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputModifiers.h"
#include "MKHInputModifierHalfAxis.generated.h"

/**
 * Keeps one half of a 1D axis and reports it as a positive magnitude; the other half reads zero.
 *
 * Movement uses one action per direction (IA_MoveForward, IA_MoveBackWard...), each reading only the
 * magnitude of its value. A stick axis spans -1..1, so without this modifier pulling the stick back would
 * also drive "forward". Put it on the gamepad mappings of the directional actions: positive half for
 * forward/right, negative half for backward/left.
 *
 * While the gamepad drives a menu the axis reads zero: the stick navigates the menu, it must not also move the
 * character. The UI context keeps the same stick mappings as gameplay (so closing a menu does not make them "new"
 * mappings that Enhanced Input ignores while the stick is off centre); see Documentation/GamepadMenuNavigation.md.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Half Axis (Makhia)"))
class MAKHIA_API UMKHInputModifierHalfAxis : public UInputModifier
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Keep the negative half (stick down / left) instead of the positive one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bNegativeHalf = false;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Clamps the X component to the kept half and flips it to positive; zero while the gamepad drives a menu.
	 *
	 * @param PlayerInput   Owning player input, used to find the local player's menu navigation.
	 * @param CurrentValue  Value produced so far by the key and the previous modifiers.
	 * @param DeltaTime     Frame time (unused).
	 * @return The kept half as a magnitude in 0..1 (0 when the axis points the other way).
	 */
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};
