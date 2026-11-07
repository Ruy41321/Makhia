// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/Settings/MKHSettingsPageWidget.h"
#include "MKHAudioSettingsWidget.generated.h"

class UButton;
class UHorizontalBox;
class USlider;
class UTextBlock;

/**
 * Audio settings panel with one slider per volume category (master / music / SFX / crowd).
 *
 * The Blueprint subclass only needs to place four USlider widgets named exactly
 * MasterVolumeSlider, MusicVolumeSlider, SFXVolumeSlider and CrowdVolumeSlider (BindWidget): all the
 * wiring is handled natively — sliders are initialized from UMKHGameUserSettings,
 * dragging applies the volume live to the Control Buses (audible feedback while
 * dragging), and releasing the slider persists the values to GameUserSettings.ini.
 *
 * No WidgetController is involved on purpose: user settings are a global,
 * engine-owned model with a single writer (this panel), so the Model->View
 * observer indirection used for gameplay data would add nothing here.
 *
 * Hosted by UMKHSettingsScreenWidget as one page among others. Volumes apply live and are
 * persisted on slider release, so the page never reports pending changes to the action bar.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHAudioSettingsWidget : public UMKHSettingsPageWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (UMKHSettingsPageWidget overrides)
	// ============================================================

	/** Volumes apply live and persist on slider release, so the shared action bar has no role here. */
	virtual bool UsesActionBar() const override;

protected:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	//~ Begin UUserWidget Interface

	/** Initializes the sliders from the saved user settings and binds their events. */
	virtual void NativeConstruct() override;

	/** Persists any pending changes and unbinds slider events. */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Applies the master volume live while the slider is being dragged.
	 *
	 * @param NewValue Normalized slider value in [0, 1].
	 */
	UFUNCTION()
	void HandleMasterVolumeChanged(float NewValue);

	/**
	 * Applies the music volume live while the slider is being dragged.
	 *
	 * @param NewValue Normalized slider value in [0, 1].
	 */
	UFUNCTION()
	void HandleMusicVolumeChanged(float NewValue);

	/**
	 * Applies the SFX volume live while the slider is being dragged.
	 *
	 * @param NewValue Normalized slider value in [0, 1].
	 */
	UFUNCTION()
	void HandleSFXVolumeChanged(float NewValue);

	/**
	 * Applies the arena crowd volume live while the slider is being dragged.
	 *
	 * @param NewValue Normalized slider value in [0, 1].
	 */
	UFUNCTION()
	void HandleCrowdVolumeChanged(float NewValue);

	/** Persists the current volumes to GameUserSettings.ini when a slider is released. */
	UFUNCTION()
	void HandleVolumeCaptureEnd();

	/** Restores one channel to its shipped level. */
	UFUNCTION()
	void HandleMasterResetClicked();

	UFUNCTION()
	void HandleMusicResetClicked();

	UFUNCTION()
	void HandleSFXResetClicked();

	UFUNCTION()
	void HandleCrowdResetClicked();

private:

	/** Pushes the saved volume values into the sliders without triggering change events. */
	void InitializeSliderValues() const;

	/**
	 * Writes one channel's readout: percentage text, dimmed when the channel is silent, plus the
	 * enabled state of its reset button.
	 *
	 * @param Volume       Normalized volume in [0, 1].
	 * @param DefaultVolume Level the channel ships with.
	 * @param ValueText    Readout label; ignored when null.
	 * @param ResetButton  Per-channel reset button; ignored when null.
	 */
	void RefreshChannelReadout(float Volume, float DefaultVolume, UTextBlock* ValueText, UButton* ResetButton) const;

	/**
	 * Splits a track row between its filled and empty halves.
	 *
	 * @param TrackRow  Row holding exactly two children; ignored when null.
	 * @param Volume    Normalized volume in [0, 1].
	 */
	void RefreshTrackFill(UHorizontalBox* TrackRow, float Volume) const;

	/** Refreshes the readouts of every channel. */
	void RefreshAllReadouts() const;

	/** Binds value-changed and capture-end events on the sliders. */
	void BindSliderEvents();

	/** Unbinds every event this widget registered on the sliders. */
	void UnbindSliderEvents();

	/** Binds the per-channel reset buttons. */
	void BindResetButtonEvents();

	/** Unbinds the per-channel reset buttons. */
	void UnbindResetButtonEvents();

	// ============================================================
	// Properties
	// ============================================================

	/** Slider controlling the master volume. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<USlider> MasterVolumeSlider;

	/** Slider controlling the music volume. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<USlider> MusicVolumeSlider;

	/** Slider controlling the sound effects volume. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<USlider> SFXVolumeSlider;

	/** Slider controlling the arena crowd volume. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<USlider> CrowdVolumeSlider;

	/** Optional percentage readouts, one per channel. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> MasterValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> MusicValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> SFXValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> CrowdValueText;

	/**
	 * Optional filled track drawn under each slider. Two Border children share the row: the first
	 * is the brass fill, the second the remaining track. USlider draws its own bar from brushes
	 * that stay invisible here, so the level is shown with plain widgets instead.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UHorizontalBox> MasterTrackRow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UHorizontalBox> MusicTrackRow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UHorizontalBox> SFXTrackRow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UHorizontalBox> CrowdTrackRow;

	/** Optional per-channel reset buttons, shown only while the channel differs from its default. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> MasterResetButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> MusicResetButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> SFXResetButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> CrowdResetButton;

	/** Level every channel ships with; the per-channel reset restores this value. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultChannelVolume = 1.0f;

	/** Readout colour at any audible level. Linear value of #EDE3D2. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Audio")
	FLinearColor AudibleValueColor = FLinearColor(0.84687f, 0.76815f, 0.64448f, 1.0f);

	/** Readout colour when the channel is silent. Linear value of #6B6055. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Audio")
	FLinearColor SilentValueColor = FLinearColor(0.14703f, 0.11697f, 0.09084f, 1.0f);
};
