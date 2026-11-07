// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHSliderRowWidget.generated.h"

class UButton;
class UHorizontalBox;
class USlider;
class UTextBlock;

/**
 * One setting with a continuous value, shown as a filled track with a readout.
 *
 * The visible bar is drawn by two Border widgets inside TrackRow — filled and remaining — and
 * not by the slider, whose own bar is left invisible; the slider stays on top purely for the
 * interaction. See the slider entry in Documentation/SettingsArchitecture.md for why.
 *
 * Like UMKHOptionRowWidget, this row holds no model: the page pushes a value in and is told
 * when the player moves it.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSliderRowWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Fills the row with a setting's identity and range.
	 *
	 * @param InOptionId     Identifier used in the callbacks.
	 * @param InDisplayName  Player-facing setting name.
	 * @param InDescription  One-line explanation; may be empty.
	 * @param InMinValue     Lowest value the slider can reach.
	 * @param InMaxValue     Highest value the slider can reach.
	 */
	void SetupRow(FName InOptionId, const FText& InDisplayName, const FText& InDescription,
		float InMinValue, float InMaxValue);

	/**
	 * Shows a value.
	 *
	 * @param InValue      Value in the range given to SetupRow.
	 * @param bIsModified  True when it differs from the saved one; drives the reset button.
	 */
	void SetValue(float InValue, bool bIsModified);

	/**
	 * Overrides the readout format of this row (e.g. a percentage), before the first SetValue.
	 *
	 * @param InFormat    Readout format; {0} is replaced with the value.
	 * @param InDecimals  Decimals shown, in [0, 3].
	 */
	void SetValueFormat(const FText& InFormat, int32 InDecimals);

	/** Identifier this row was set up with. */
	UFUNCTION(BlueprintPure, Category = "Settings|Options")
	FName GetOptionId() const { return OptionId; }

	/** Broadcast continuously while the player drags. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FSliderRowChangedSignature, FName /*OptionId*/, float /*Value*/);
	FSliderRowChangedSignature OnValueChanged;

	/** Broadcast when the drag ends, the moment to persist. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FSliderRowCommittedSignature, FName /*OptionId*/);
	FSliderRowCommittedSignature OnValueCommitted;

	/** Broadcast when the player asks to restore the saved value. */
	FSliderRowCommittedSignature OnValueReset;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Slider callbacks; USlider delegates must be UFUNCTIONs. */
	UFUNCTION()
	void HandleSliderChanged(float NewValue);

	UFUNCTION()
	void HandleCaptureEnd();

	UFUNCTION()
	void HandleResetClicked();

	/** Splits TrackRow between its filled and empty halves. */
	void RefreshTrackFill(float Normalized) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Setting name. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> OptionNameText;

	/** The slider itself. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<USlider> ValueSlider;

	/** Optional readout, description, filled track and reset. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> OptionValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> OptionDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UHorizontalBox> TrackRow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> ResetButton;

	/** Readout format; {0} is replaced with the value. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Options")
	FText ValueFormat = NSLOCTEXT("MKHSettings", "SliderValueFormat", "{0}");

	/** Decimals shown in the readout. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Options", meta = (ClampMin = "0", ClampMax = "3"))
	int32 ValueDecimals = 2;

private:

	/** Identifier used in the delegates. */
	FName OptionId = NAME_None;

	/** Range given to SetupRow. */
	float MinValue = 0.0f;
	float MaxValue = 1.0f;
};
