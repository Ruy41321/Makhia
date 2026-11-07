// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Input/MKHInputDeviceTypes.h"
#include "MKHButton.generated.h"

class UMKHButtonStyle;

/**
 * Button whose look comes from a shared UMKHButtonStyle asset instead of its own Style.
 *
 * Use it for every menu button: pick the style that matches the button's role and leave Style,
 * Background Color and Color and Opacity alone, since the style overwrites them. With no style
 * assigned it behaves like a plain UButton. Catalogue and rules: see Documentation/GameMenuArchitecture.md.
 *
 * With the gamepad the focus plays the part of the hover: the focused button shows its hovered look and
 * broadcasts OnHovered / OnUnhovered, and the hover of the hidden cursor is ignored
 * (Documentation/GamepadMenuNavigation.md).
 *
 * Every MKH Button plays the interface sound effects SFX.UI.Hover and SFX.UI.Click (DT_SoundEffects), so the
 * button sounds are set once for the whole game (Documentation/AudioSystemArchitecture.md, Sound effects).
 */
UCLASS(meta = (DisplayName = "MKH Button"))
class MAKHIA_API UMKHButton : public UButton
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	//~ End UWidget Interface
#endif

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Switches this button to another style and applies it immediately.
	 *
	 * @param InButtonStyle  Style to use; null keeps the current look.
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance|Button")
	void SetButtonStyle(UMKHButtonStyle* InButtonStyle);

	/** Returns the style this button takes its look from (may be null). */
	UFUNCTION(BlueprintPure, Category = "Appearance|Button")
	UMKHButtonStyle* GetButtonStyle() const { return ButtonStyle; }

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Binds the hover, focus and input device callbacks, on runtime instances only. */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Takes the brushes and background tint from ButtonStyle (when set), then applies the current state. */
	void ApplyButtonStyle();

	/**
	 * Applies the look of the current state: with the gamepad the focused button looks hovered and the others idle;
	 * with the mouse the base style as is. Also picks the content tint of ButtonStyle.
	 */
	void ApplyVisualState();

	/** Broadcasts OnHovered / OnUnhovered when the gamepad focus starts or stops counting as a hover. */
	void UpdateGamepadHover();

	/** Hover callback (mouse, or gamepad focus): switches the content to the hovered tint and plays SFX.UI.Hover. */
	UFUNCTION()
	void HandleHovered();

	/** Click callback (mouse or gamepad confirm): plays SFX.UI.Click. */
	UFUNCTION()
	void HandleClicked();

	/** Unhover callback: switches the content back to the idle tint. */
	UFUNCTION()
	void HandleUnhovered();

	/** Focus callback: shows the focus while the gamepad is the active device. */
	void HandleReceivedFocus();

	/** Focus lost callback. */
	void HandleLostFocus();

	/**
	 * Active input device callback: switches between the focus look (gamepad) and the hover look (mouse).
	 *
	 * @param NewDevice  Device the player now uses.
	 */
	UFUNCTION()
	void HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice);

	// ============================================================
	// Properties
	// ============================================================

	/** Look of this button. Overrides Style, Background Color and Color and Opacity when set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UMKHButtonStyle> ButtonStyle;

private:
	/** Style the state looks are derived from: ButtonStyle's, or the designer Style when there is no asset. */
	FButtonStyle BaseStyle;

	/** True once BaseStyle holds a style. */
	bool bHasBaseStyle = false;

	/** True while the cursor is over the button; picks the content tint. */
	bool bIsHoveredForTint = false;

	/** True while the button has the keyboard / gamepad focus. */
	bool bHasFocus = false;

	/** True while the gamepad is the active input device. */
	bool bGamepadActive = false;

	/** True while OnHovered was broadcast for the gamepad focus and OnUnhovered is still owed. */
	bool bGamepadHoverBroadcast = false;
};
