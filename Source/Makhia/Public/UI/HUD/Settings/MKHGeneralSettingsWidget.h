// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/Settings/MKHSettingsPageWidget.h"
#include "MKHGeneralSettingsWidget.generated.h"

class UPanelWidget;
class UMKHKeybindCategoryWidget;
class UMKHOptionRowWidget;
class UMKHSliderRowWidget;

/** Settings shown on the General page, in display order. */
UENUM()
enum class EMKHGeneralOption : uint8
{
	Language,

	Brightness,

	MouseSensitivity,
	InvertYAxis,
	CameraShake,

	ControllerLookSensitivity,
	ControllerInvertYAxis,
	ControllerVibration,

	ShowNetworkStats,
	ShowFPS,

	// Appended so the row ids of the options above stay the same.
	HoldToEquipPickup,
	AutoQuickSlotLoadout,
	ScoreboardStyle
};

/**
 * General settings: interface language, gameplay, brightness, mouse camera feel, controller feel and on-screen readouts.
 *
 * Applies and saves as it goes, like the audio page, so it opts out of the action bar. Nothing
 * here can leave the player unable to see the panel, which is what forces the video page to
 * stage its edits instead.
 *
 * The language is not stored in UMKHGameUserSettings: the engine's current language is the source
 * of truth, read and switched through UMKHLocalizationLibrary (see Documentation/Localization.md).
 */
UCLASS(Abstract)
class MAKHIA_API UMKHGeneralSettingsWidget : public UMKHSettingsPageWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (UMKHSettingsPageWidget overrides)
	// ============================================================

	/** Writes the values to disk. Changes are already live; this only persists them. */
	virtual void ApplyChanges() override;

	/** Re-reads the saved values. */
	virtual void RevertChanges() override;

	/** Restores the shipped defaults and saves them. */
	virtual void ResetToDefaults() override;

	/** Always false: values apply and save on the spot. */
	virtual bool UsesActionBar() const override;

	/** Rebuilds every row from the user settings. */
	UFUNCTION(BlueprintCallable, Category = "Settings|General")
	void RebuildOptionList();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UMKHSettingsPageWidget Interface
	virtual void NativeOnPageActivated() override;
	//~ End UMKHSettingsPageWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Spawns the header and rows of one section.
	 *
	 * @param SectionLabel  Section title.
	 * @param Options       Options listed under it.
	 */
	void BuildSection(const FText& SectionLabel, const TArray<EMKHGeneralOption>& Options);

	/**
	 * Spawns one slider row (brightness, mouse or controller look sensitivity).
	 *
	 * @param Option  Continuous option to show.
	 */
	void BuildSliderRow(EMKHGeneralOption Option);

	/** Whether an option is shown as a slider rather than a stepper. */
	static bool IsSliderOption(EMKHGeneralOption Option);

	/** Current value of a slider option, read from the user settings. */
	float GetSliderValue(EMKHGeneralOption Option) const;

	/** Writes a slider option back to the user settings (not saved). */
	void SetSliderValue(EMKHGeneralOption Option, float Value) const;

	/** Shipped value of a slider option, the one its reset returns to. */
	static float GetDefaultSliderValue(EMKHGeneralOption Option);

	/**
	 * Range a slider option can take.
	 *
	 * @param Option    Continuous option.
	 * @param OutMin    Lowest value.
	 * @param OutMax    Highest value.
	 */
	void GetSliderRange(EMKHGeneralOption Option, float& OutMin, float& OutMax) const;

	/** Row callbacks. */
	void HandleOptionStepped(FName OptionId, int32 Delta);
	void HandleOptionReset(FName OptionId);
	void HandleSliderChanged(FName OptionId, float Value);
	void HandleSliderCommitted(FName OptionId);
	void HandleSliderReset(FName OptionId);

	/** Current index of a stepped option, read from the user settings. */
	int32 GetOptionIndex(EMKHGeneralOption Option) const;

	/** Index a stepped option returns to when reset. */
	int32 GetDefaultOptionIndex(EMKHGeneralOption Option) const;

	/** Writes a stepped option back to the user settings. */
	void SetOptionIndex(EMKHGeneralOption Option, int32 NewIndex);

	/** Values a stepped option can take. */
	TArray<FText> GetOptionValues(EMKHGeneralOption Option) const;

	/** Player-facing name and description of an option. */
	FText GetOptionDisplayName(EMKHGeneralOption Option) const;
	FText GetOptionDescription(EMKHGeneralOption Option) const;

	/** Converts between the enum and the FName used as row id. */
	static FName ToOptionId(EMKHGeneralOption Option);
	static bool FromOptionId(FName OptionId, EMKHGeneralOption& OutOption);

	// ============================================================
	// Properties - Configuration
	// ============================================================

	/** Row Blueprint for settings picked from a list. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|General")
	TSubclassOf<UMKHOptionRowWidget> OptionRowClass;

	/** Row Blueprint for settings with a continuous value. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|General")
	TSubclassOf<UMKHSliderRowWidget> SliderRowClass;

	/** Section header Blueprint, shared with the other pages. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|General")
	TSubclassOf<UMKHKeybindCategoryWidget> SectionHeaderClass;

	/** Range of the look sensitivity slider. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|General", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MinMouseSensitivity = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Settings|General", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float MaxMouseSensitivity = 3.0f;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Container the section headers and rows are added to. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UPanelWidget> OptionListContainer;

private:
	// ============================================================
	// Properties - Runtime State
	// ============================================================

	/** Rows currently in the list. */
	UPROPERTY()
	TArray<TObjectPtr<UMKHOptionRowWidget>> OptionRows;

	UPROPERTY()
	TArray<TObjectPtr<UMKHSliderRowWidget>> SliderRows;
};
