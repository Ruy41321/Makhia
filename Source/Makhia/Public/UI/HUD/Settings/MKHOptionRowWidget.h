// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHOptionRowWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * One setting whose value is picked from a fixed list, shown as a stepper: previous arrow,
 * current value, next arrow.
 *
 * A stepper rather than a combo box on purpose: it needs no popup, works from a gamepad with
 * the same two inputs used everywhere else, and keeps the row height shared with the keybind
 * and volume lists.
 *
 * The row holds no model of its own. The owning page pushes a value in and is told when the
 * player steps; what the options mean is entirely the page's business.
 *
 * A value wider than its box (an audio device name) is cut with an ellipsis, carries the full text as
 * tooltip, and scrolls back and forth while the row is hovered or holds the focus, so it can be read with
 * the gamepad too. Needs OptionValueText inside a clipping wrapper: see Documentation/SettingsArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHOptionRowWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Fills the row with a setting's identity.
	 *
	 * @param InOptionId     Identifier the page uses to recognise this row in its callbacks.
	 * @param InDisplayName  Player-facing setting name.
	 * @param InDescription  One-line explanation shown under the name; may be empty.
	 */
	void SetupRow(FName InOptionId, const FText& InDisplayName, const FText& InDescription);

	/**
	 * Shows a value and tells the row where it sits in the list, so the arrows can disable
	 * themselves at the ends.
	 *
	 * @param InValueText   Value to display.
	 * @param InIndex       Index of the value.
	 * @param InNumOptions  How many values the setting has.
	 * @param bIsModified   True when the value differs from the saved one; drives the reset button.
	 * @param bIsCustom     True to highlight the value, used by a preset showing "Custom".
	 */
	void SetValue(const FText& InValueText, int32 InIndex, int32 InNumOptions, bool bIsModified, bool bIsCustom);

	/** Identifier this row was set up with. */
	UFUNCTION(BlueprintPure, Category = "Settings|Options")
	FName GetOptionId() const { return OptionId; }

	/** Broadcast when the player steps the value: -1 for the left arrow, +1 for the right one. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOptionSteppedSignature, FName /*OptionId*/, int32 /*Delta*/);
	FOptionSteppedSignature OnOptionStepped;

	/** Broadcast when the player asks to restore this setting's saved value. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOptionResetSignature, FName /*OptionId*/);
	FOptionResetSignature OnOptionReset;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Remembers that the cursor is over the row (the row's own hover flag stays off: it is not hit-testable). */
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Forgets the cursor over the row. */
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	/** Detects a value wider than its box and scrolls it while the row is hovered or focused. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Re-measures the value against its box; when the value starts or stops overflowing, switches the tooltip and the
	 * alignment (left with an ellipsis when it overflows, centred when it fits).
	 *
	 * @param bForceApply  True to re-apply tooltip and alignment even without a change (the value text changed).
	 */
	void RefreshValueOverflow(bool bForceApply);

	/**
	 * Moves the scrolling value one frame: pause at each end, then slide towards the other one.
	 *
	 * @param DeltaTime  Seconds since the last frame.
	 */
	void AdvanceMarquee(float DeltaTime);

	/** Puts the value back at its start, cut with an ellipsis. */
	void StopMarquee();

	/** @return True while the focus is on one of the row's buttons, or (keyboard and mouse) the cursor is over the row. */
	bool IsRowActive() const;

	/** @return Width of the current value text at the value font, in slate units. */
	float MeasureValueTextWidth() const;

	/** Click handlers; UButton delegates must be UFUNCTIONs. */
	UFUNCTION()
	void HandlePreviousClicked();

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleResetClicked();

	// ============================================================
	// Properties
	// ============================================================

	/** Setting name. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> OptionNameText;

	/** Current value. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> OptionValueText;

	/** Stepper arrows. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UButton> PreviousButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UButton> NextButton;

	/** Optional one-line explanation under the name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> OptionDescriptionText;

	/** Optional per-row reset, shown only while the value is modified. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> ResetButton;

	/** Value colour in the usual case. Linear value of #EDE3D2. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Options")
	FLinearColor ValueColor = FLinearColor(0.84687f, 0.76815f, 0.64448f, 1.0f);

	/** Value colour when the row reports a custom state. Linear value of the #C9973E brass accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Options")
	FLinearColor CustomValueColor = FLinearColor(0.58408f, 0.30947f, 0.04817f, 1.0f);

	/** Speed of a value scrolling inside its box, in slate units per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Options", meta = (ClampMin = "1"))
	float MarqueeSpeed = 45.0f;

	/** Seconds a scrolling value rests at each end before turning back. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Options", meta = (ClampMin = "0"))
	float MarqueeEndPause = 1.2f;

private:

	/** Identifier used in the delegates. */
	FName OptionId = NAME_None;

	/** Width of the current value text, measured when the value is set. */
	float ValueTextWidth = 0.0f;

	/** How much wider the value is than its box (0 when it fits). */
	float ValueOverflow = 0.0f;

	/** True while the value does not fit its box (tooltip shown). */
	bool bValueOverflows = false;

	/** True between mouse enter and mouse leave on the row. */
	bool bCursorOverRow = false;

	/** True while the value scrolls. */
	bool bMarqueeRunning = false;

	/** Current scroll of the value, from 0 to ValueOverflow. */
	float MarqueeOffset = 0.0f;

	/** +1 while scrolling towards the end, -1 on the way back. */
	float MarqueeDirection = 1.0f;

	/** Seconds left in the current pause at one end. */
	float MarqueePauseRemaining = 0.0f;
};
