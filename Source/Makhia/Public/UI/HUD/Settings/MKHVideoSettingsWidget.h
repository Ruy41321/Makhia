// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/Settings/MKHSettingsPageWidget.h"
#include "MKHVideoSettingsWidget.generated.h"

class UPanelWidget;
class UMKHKeybindCategoryWidget;
class UMKHOptionRowWidget;
class UMKHVideoConfirmWidget;

/**
 * Settings that this page can edit. Order here is the order shown on screen.
 *
 * Foliage and View Distance are deliberately absent: the first only affects instanced foliage
 * and landscape grass, of which the project has none, and the second scales cull distances and
 * skeletal mesh LOD bias, which do next to nothing in a closed arena. Both would be options
 * that change nothing, which is worse than an option that is missing.
 */
UENUM()
enum class EMKHVideoOption : uint8
{
	WindowMode,
	Resolution,
	VSync,
	FrameRateLimit,

	QualityPreset,
	AntiAliasing,
	PostProcessing,
	Shadows,
	GlobalIllumination,
	Reflections,
	Textures,
	Effects,
	Shading,

	ResolutionScale
};

/**
 * Video settings page.
 *
 * Unlike Keyboard and Audio, nothing here is live: every change is staged in StagedValues and
 * only reaches UGameUserSettings on Apply. Video modes cannot be tried out safely one keypress
 * at a time — a resolution the monitor cannot display would leave the player unable to see the
 * panel they need in order to undo it.
 *
 * When Apply changes the resolution or the window mode, UMKHVideoConfirmWidget asks the player
 * to keep it and reverts on its own if nobody answers, through the engine's ConfirmVideoMode()
 * and RevertVideoMode().
 */
UCLASS(Abstract)
class MAKHIA_API UMKHVideoSettingsWidget : public UMKHSettingsPageWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (UMKHSettingsPageWidget overrides)
	// ============================================================

	/** Writes the staged values to the user settings and applies them. */
	virtual void ApplyChanges() override;

	/** Drops the staged values and re-reads the applied ones. */
	virtual void RevertChanges() override;

	/** Restores the engine's default video settings, staged like any other change. */
	virtual void ResetToDefaults() override;

	/** True while any staged value differs from what is applied. */
	virtual bool HasPendingChanges() const override;

	/** True while the confirmation modal is up: leaving mid-countdown must not be possible. */
	virtual bool IsBlockingClose() const override;

	/** Rebuilds every row from the staged values. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Video")
	void RebuildOptionList();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UMKHSettingsPageWidget Interface
	virtual void NativeOnPageActivated() override;
	virtual void NativeOnPageDeactivated() override;
	//~ End UMKHSettingsPageWidget Interface

	//~ Begin UUserWidget Interface
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Reads every option out of the user settings into StagedValues. */
	void ReadFromUserSettings();

	/** Fills ResolutionOptions with the modes the display reports as supported. */
	void CollectSupportedResolutions();

	/**
	 * Spawns the header and the rows of one section.
	 *
	 * @param SectionLabel  Section title.
	 * @param Options       Options listed under it.
	 */
	void BuildSection(const FText& SectionLabel, const TArray<EMKHVideoOption>& Options);

	/** Row callback: steps one option and restages the dependent ones. */
	void HandleOptionStepped(FName OptionId, int32 Delta);

	/** Row callback: restores one option to its applied value. */
	void HandleOptionReset(FName OptionId);

	/** Confirmation callbacks. */
	void HandleModeConfirmed();
	void HandleModeReverted();

	/** Tears the confirmation modal down and unbinds it. */
	void CloseConfirmModal();

	/**
	 * Values a given option can take, in order.
	 *
	 * @param Option  Option to describe.
	 * @return The list shown in the stepper.
	 */
	TArray<FText> GetOptionValues(EMKHVideoOption Option) const;

	/** Player-facing name of an option. */
	FText GetOptionDisplayName(EMKHVideoOption Option) const;

	/** One-line explanation of an option; may be empty. */
	FText GetOptionDescription(EMKHVideoOption Option) const;

	/** Converts between the enum and the FName used as row id. */
	static FName ToOptionId(EMKHVideoOption Option);
	static bool FromOptionId(FName OptionId, EMKHVideoOption& OutOption);

	/** True when the option is one of the eight scalability groups. */
	static bool IsQualityGroup(EMKHVideoOption Option);

	/**
	 * Recomputes the preset row from the eight groups: it reads Custom unless they all match.
	 * Mirrors what GetOverallScalabilityLevel() reports when the levels are not uniform.
	 */
	void RefreshPresetFromGroups();

	// ============================================================
	// Properties - Configuration
	// ============================================================

	/** Row Blueprint spawned once per option. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Video")
	TSubclassOf<UMKHOptionRowWidget> OptionRowClass;

	/** Section header Blueprint, shared with the keybind page. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Video")
	TSubclassOf<UMKHKeybindCategoryWidget> SectionHeaderClass;

	/** Modal shown after a resolution or window mode change. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Video")
	TSubclassOf<UMKHVideoConfirmWidget> ConfirmWidgetClass;

	/** Frame rate caps offered, in order (30 and 40 for the Steam Deck). The last entry is shown as unlimited. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Video")
	TArray<int32> FrameRateLimits = { 30, 40, 60, 120, 144, 0 };

	/** Resolution scale steps, as percentages of the native resolution. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Video")
	TArray<int32> ResolutionScaleSteps = { 50, 60, 70, 80, 90, 100 };

	/** Viewport Z-order of the confirmation modal. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Video")
	int32 ConfirmModalZOrder = 1000;

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
	TArray<TObjectPtr<UMKHOptionRowWidget>> RowWidgets;

	/** Confirmation modal while a video mode is awaiting an answer. */
	UPROPERTY()
	TObjectPtr<UMKHVideoConfirmWidget> ActiveConfirmWidget;

	/** Staged index per option: what the player has chosen but not applied. */
	TMap<EMKHVideoOption, int32> StagedValues;

	/** Applied index per option: what the user settings currently hold. */
	TMap<EMKHVideoOption, int32> AppliedValues;

	/** Resolutions the display supports, filled on activation. */
	TArray<FIntPoint> ResolutionOptions;
};
