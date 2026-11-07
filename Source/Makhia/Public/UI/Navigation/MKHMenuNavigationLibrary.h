// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MKHMenuNavigationLibrary.generated.h"

class UWidget;

/**
 * Helpers for widgets that behave differently when the gamepad drives the menu.
 * See Documentation/GamepadMenuNavigation.md.
 */
UCLASS()
class MAKHIA_API UMKHMenuNavigationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns true while the gamepad drives the menus of the widget's player.
	 *
	 * @param Widget  Any widget of the player.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Navigation")
	static bool IsGamepadNavigationActive(const UWidget* Widget);

	/**
	 * Where to open a popup (e.g. the inventory action menu), in viewport units like GetMousePositionOnViewport:
	 * the cursor with the mouse, the top right corner of the anchor with the gamepad (the cursor is hidden).
	 *
	 * @param Anchor  Widget the popup belongs to.
	 * @return The position.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Navigation")
	static FVector2D GetPopupPosition(const UWidget* Anchor);

	/**
	 * Whether a key event is the gamepad Back button (B / Circle), the pad's Esc.
	 *
	 * @param KeyEvent  Event to test.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Navigation")
	static bool IsGamepadBackKey(const FKeyEvent& KeyEvent);

	/**
	 * Whether a key event is the gamepad Accept button (A / Cross), the pad's Enter.
	 *
	 * @param KeyEvent  Event to test.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Navigation")
	static bool IsGamepadAcceptKey(const FKeyEvent& KeyEvent);
};
