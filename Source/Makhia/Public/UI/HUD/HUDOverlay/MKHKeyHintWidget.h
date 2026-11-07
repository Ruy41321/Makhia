// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/MKHInputDeviceTypes.h"
#include "InputCoreTypes.h"
#include "UI/MKHSystemWidget.h"
#include "MKHKeyHintWidget.generated.h"

class UEnhancedInputUserSettings;
class UMKHActiveInputDeviceSubsystem;
class UInputAction;
class UTextBlock;

/**
 * Keycap showing the key currently bound to an input action. Parent class of W_KeyCap, the only keycap
 * widget of the project: quick slots, the inventory hint and the pickup prompt all embed it.
 *
 * The key is never authored in the widget: it is read from the owning player's Enhanced Input settings
 * (the player's Primary key on the device in use) when the widget is built, every time the player applies
 * a rebind and every time the player switches between keyboard/mouse and gamepad.
 */
UCLASS()
class MAKHIA_API UMKHKeyHintWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Re-reads the key bound to InputAction and writes it into KeyText. Keeps the designer text at design time. */
	UFUNCTION(BlueprintCallable, Category = "KeyHint")
	void RefreshKeyText();

	/**
	 * Changes the action whose key is shown and refreshes the label.
	 *
	 * @param InInputAction  Action to display; null leaves the keycap empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "KeyHint")
	void SetInputAction(UInputAction* InInputAction);

	/**
	 * Changes the gamepad button shown as it is and refreshes the label.
	 *
	 * @param InKey  Button to display on the gamepad; an invalid key goes back to the actions.
	 */
	void SetFixedGamepadKey(const FKey& InKey);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Applies bShowBackground, also in the designer. */
	virtual void NativePreConstruct() override;

	/** Shows the current key and subscribes to key rebinds and device switches. */
	virtual void NativeConstruct() override;

	/** Drops the rebind and device subscriptions. */
	virtual void NativeDestruct() override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Key rebind callback: shows the newly bound key. */
	UFUNCTION()
	void HandleInputSettingsApplied();

	/**
	 * Device switch callback: shows the key of the device now in use.
	 *
	 * @param NewDevice  Device family the player just switched to.
	 */
	UFUNCTION()
	void HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice);

private:
	/** Subscribes to the owning player's input settings and active device, so a rebind or a switch updates the hint. */
	void BindInputSettings();

	/** Unsubscribes from the input settings and the active device. */
	void UnbindInputSettings();

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** Action whose key is shown (e.g. IA_ToggleInventory). Set per instance, or at runtime via SetInputAction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyHint")
	TObjectPtr<UInputAction> InputAction;

	/** False to collapse KeyBackground, for hosts that draw their own frame around the key (e.g. the pickup prompt). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyHint")
	bool bShowBackground = true;

	/**
	 * Action shown instead of InputAction while the player uses a gamepad (e.g. IA_SwitchWeapon on the weapon slots,
	 * whose IA_WeaponSlot1/2 have no gamepad button). None shows InputAction on every device.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyHint")
	TObjectPtr<UInputAction> GamepadInputAction;

	/**
	 * False to show only the mapping's own button ("RB" instead of "LB + RB"). For keycaps too small for a chord,
	 * whose host shows the chord once elsewhere (the skill slots and the Alternate Actions keycap of W_WeaponSkillRow).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyHint")
	bool bShowChord = true;

	/** True to show the keycap only while the player uses a gamepad (collapsed on keyboard and mouse). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyHint")
	bool bGamepadOnly = false;

	/**
	 * Gamepad button shown as it is while the player uses a gamepad, for menu buttons that are not input actions
	 * (A and B of the menus, GamepadMenuNavigation.md). Wins over the actions on the gamepad.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyHint")
	FKey FixedGamepadKey;

	/** Keycap label filled with the key's display name. Optional so the widget survives a designer rename. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;

	/** Optional backdrop of the keycap, collapsed when bShowBackground is false. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> KeyBackground;

private:
	/** Input settings whose rebinds refresh the hint. */
	TWeakObjectPtr<UEnhancedInputUserSettings> BoundInputSettings;

	/** Active device tracker whose switches refresh the hint. */
	TWeakObjectPtr<UMKHActiveInputDeviceSubsystem> BoundDeviceSubsystem;
};
