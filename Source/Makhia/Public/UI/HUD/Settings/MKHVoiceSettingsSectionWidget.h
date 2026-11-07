// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHVoiceSettingsSectionWidget.generated.h"

class UMKHKeybindCategoryWidget;
class UMKHOptionRowWidget;
class UMKHSliderRowWidget;
class UMKHVoiceMicMeterWidget;
class UPanelWidget;

/** Rows of the device and voice chat sections of the Audio page, in display order. */
UENUM()
enum class EMKHVoiceSettingOption : uint8
{
	GameOutputDevice,
	VoiceChat,
	MicMode,
	NonFriends,
	InputVolume,
	MicLevel,
	ActivationThreshold,
	OutputVolume,
	VoiceOutputDevice,
	InputDevice
};

/**
 * Device and Voice chat sections of the Audio settings page (W_AudioSettings, under the volume sliders): output
 * device of the game, then voice chat on/off, microphone mode, voice with non-friends, microphone volume with a
 * live level meter and the open mic activation level, voice volume, voice output device and the microphone in use
 * (read only). Rows are built in C++ from the shared row Blueprints, like the General page.
 *
 * Everything applies and saves at once (the Audio page has no action bar); the voice values go through
 * UMKHVoiceChatSubsystem, the devices through UMKHAudioDeviceSubsystem. The device rows follow the backend
 * capabilities. Full model: see Documentation/VoiceChat.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHVoiceSettingsSectionWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Rebuilds every row from the saved settings and the device list. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void RebuildOptionList();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	/** Builds the rows, reads the devices again and starts the microphone test of the meter. */
	virtual void NativeConstruct() override;

	/** Stops the microphone test and unbinds everything. */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Spawns the header and rows of one section.
	 *
	 * @param SectionLabel  Section title.
	 * @param Options       Rows listed under it (rows the backend cannot offer are skipped).
	 */
	void BuildSection(const FText& SectionLabel, const TArray<EMKHVoiceSettingOption>& Options);

	/** Spawns a stepper row. */
	void BuildOptionRow(EMKHVoiceSettingOption Option);

	/** Spawns a slider row. */
	void BuildSliderRow(EMKHVoiceSettingOption Option);

	/** Spawns the microphone level meter. */
	void BuildMeterRow();

	/**
	 * Adds a row to the list, filling the width.
	 *
	 * @param Row  Row widget.
	 */
	void AddRowToList(UWidget* Row) const;

	/** Returns true when the backend lets the row be shown. */
	bool IsOptionAvailable(EMKHVoiceSettingOption Option) const;

	/** Returns true for the slider rows. */
	static bool IsSliderOption(EMKHVoiceSettingOption Option);

	/** Refreshes the value of every row (after a change or a new device list). */
	void RefreshRowValues() const;

	/** Writes the current value into one stepper row. */
	void RefreshOptionRow(UMKHOptionRowWidget& Row, EMKHVoiceSettingOption Option) const;

	/** Values a stepper row can take. */
	TArray<FText> GetOptionValues(EMKHVoiceSettingOption Option) const;

	/** Current index of a stepper row. */
	int32 GetOptionIndex(EMKHVoiceSettingOption Option) const;

	/** Index a stepper row returns to when reset. */
	static int32 GetDefaultOptionIndex(EMKHVoiceSettingOption Option);

	/** Applies and saves a new index of a stepper row. */
	void SetOptionIndex(EMKHVoiceSettingOption Option, int32 NewIndex) const;

	/** Device ids behind the device stepper: index 0 is the default entry (empty id). */
	TArray<FString> GetDeviceIds() const;

	/** Current value of a slider row, in [0, 100]. */
	float GetSliderValue(EMKHVoiceSettingOption Option) const;

	/** Applies a slider row value in [0, 100] (saved on release). */
	void SetSliderValue(EMKHVoiceSettingOption Option, float Value) const;

	/** Shipped value of a slider row, in [0, 100]. */
	static float GetDefaultSliderValue(EMKHVoiceSettingOption Option);

	/** Player-facing name and description of a row. */
	FText GetOptionDisplayName(EMKHVoiceSettingOption Option) const;
	FText GetOptionDescription(EMKHVoiceSettingOption Option) const;

	/** Converts between the enum and the FName used as row id. */
	static FName ToOptionId(EMKHVoiceSettingOption Option);
	static bool FromOptionId(FName OptionId, EMKHVoiceSettingOption& OutOption);

	/** Row callbacks. */
	void HandleOptionStepped(FName OptionId, int32 Delta);
	void HandleOptionReset(FName OptionId);
	void HandleSliderChanged(FName OptionId, float Value);
	void HandleSliderCommitted(FName OptionId);
	void HandleSliderReset(FName OptionId);

	/** Device list or voice state changed elsewhere: refreshes the values. */
	UFUNCTION()
	void HandleExternalChange();

	/** Unbinds the row callbacks. */
	void UnbindRows();

	// ============================================================
	// Properties - Configuration
	// ============================================================

	/** Row Blueprint for settings picked from a list (W_OptionRow). */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Voice")
	TSubclassOf<UMKHOptionRowWidget> OptionRowClass;

	/** Row Blueprint for settings with a continuous value (W_SliderRow). */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Voice")
	TSubclassOf<UMKHSliderRowWidget> SliderRowClass;

	/** Microphone level meter Blueprint (W_VoiceMicMeter). */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Voice")
	TSubclassOf<UMKHVoiceMicMeterWidget> MicMeterClass;

	/** Section header Blueprint, shared with the other pages. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Voice")
	TSubclassOf<UMKHKeybindCategoryWidget> SectionHeaderClass;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Container the section headers and rows are added to (a vertical box). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UPanelWidget> OptionListContainer;

private:
	/** Stepper rows currently in the list. */
	UPROPERTY()
	TArray<TObjectPtr<UMKHOptionRowWidget>> OptionRows;

	/** Slider rows currently in the list. */
	UPROPERTY()
	TArray<TObjectPtr<UMKHSliderRowWidget>> SliderRows;

	/** Level meter of the microphone. */
	UPROPERTY()
	TObjectPtr<UMKHVoiceMicMeterWidget> MicMeter;
};
