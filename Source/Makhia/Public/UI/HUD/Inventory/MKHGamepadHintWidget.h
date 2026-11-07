// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/MKHInputDeviceTypes.h"
#include "InputCoreTypes.h"
#include "UI/MKHSystemWidget.h"
#include "MKHGamepadHintWidget.generated.h"

class UMKHActiveInputDeviceSubsystem;
class UMKHKeyHintWidget;
class UTextBlock;

/**
 * A menu button hint shown only while the player uses a gamepad (W_GamepadHint): a W_KeyCap showing GamepadKey and
 * a label, e.g. "A Actions". Collapsed on keyboard and mouse.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHGamepadHintWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Changes what the hint says, for hints that follow a state.
	 *
	 * @param InHintText What the button does now.
	 */
	void SetHintText(const FText& InHintText);

	/**
	 * Shows or hides the hint whatever the device, for hints that follow a state. Stays collapsed on keyboard and mouse.
	 *
	 * @param bEnabled False keeps the hint collapsed also with the gamepad.
	 */
	void SetHintEnabled(bool bEnabled);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Shows HintText and GamepadKey, also in the designer. */
	virtual void NativePreConstruct() override;

	/** Applies the visibility of the current device and follows the switches. */
	virtual void NativeConstruct() override;

	/** Stops following the device switches. */
	virtual void NativeDestruct() override;

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Device switch callback.
	 *
	 * @param NewDevice Device family the player just switched to.
	 */
	UFUNCTION()
	void HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice);

	/**
	 * Shows the hint on the gamepad only.
	 *
	 * @param Device Device family in use.
	 */
	void ApplyDevice(EMKHInputDevice Device);

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** What the button does. Set on the instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GamepadHint")
	FText HintText;

	/** Button the hint stands for. Set on the instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GamepadHint")
	FKey GamepadKey;

	/** Keycap showing GamepadKey. */
	UPROPERTY(BlueprintReadOnly, Category = "GamepadHint", meta = (BindWidget))
	TObjectPtr<UMKHKeyHintWidget> KeyCap;

	/** Label showing HintText. */
	UPROPERTY(BlueprintReadOnly, Category = "GamepadHint", meta = (BindWidget))
	TObjectPtr<UTextBlock> HintLabel;

private:
	/** False keeps the hint collapsed also with the gamepad (SetHintEnabled). */
	bool bHintEnabled = true;

	/** Active device tracker followed while constructed. */
	TWeakObjectPtr<UMKHActiveInputDeviceSubsystem> BoundDeviceSubsystem;
};
