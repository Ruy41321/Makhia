// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHVoiceMicMeterWidget.generated.h"

class UHorizontalBox;
class UProgressBar;
class UTextBlock;

/**
 * Live level meter of the microphone in the Voice chat section of the Audio page, with a marker at the open mic
 * activation level: the bar turns brass when the voice would be sent. While visible it keeps a test capture of
 * the microphone open (UMKHVoiceChatSubsystem::StartMicrophoneTest), apart from the network voice.
 * Full model: see Documentation/VoiceChat.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHVoiceMicMeterWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Fills the row texts.
	 *
	 * @param InDisplayName  Row name.
	 * @param InDescription  One-line explanation; may be empty.
	 */
	void SetupRow(const FText& InDisplayName, const FText& InDescription);

	/**
	 * Moves the activation marker.
	 *
	 * @param InThreshold  Activation level in [0, 1], the scale of the meter.
	 * @param bInVisible   False hides the marker (push-to-talk sends whatever is said while the key is held).
	 */
	void SetThreshold(float InThreshold, bool bInVisible);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	/** Starts the microphone test. */
	virtual void NativeConstruct() override;

	/** Stops the microphone test. */
	virtual void NativeDestruct() override;

	/** Reads the level and moves the bar. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Splits the marker row around the threshold. */
	void RefreshMarker() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Row name. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> OptionNameText;

	/** Optional one-line explanation. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> OptionDescriptionText;

	/** Level of the microphone. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UProgressBar> LevelBar;

	/** Optional marker row over the bar: three children (left spacer, marker, right spacer) sized by fill. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UHorizontalBox> ThresholdRow;

	/** Bar colour below the activation level. Linear value of #6B6055. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Voice")
	FLinearColor QuietColor = FLinearColor(0.14703f, 0.11697f, 0.09084f, 1.0f);

	/** Bar colour above the activation level. Linear value of the #C9973E brass accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Voice")
	FLinearColor ActiveColor = FLinearColor(0.58408f, 0.30947f, 0.04817f, 1.0f);

	/** How fast the bar falls back, in levels per second (it rises at once). */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Voice", meta = (ClampMin = "0.1"))
	float FallSpeed = 1.5f;

private:
	/** Activation level in [0, 1]. */
	float Threshold = 0.2f;

	/** Level shown, smoothed. */
	float ShownLevel = 0.f;

	/** True while the marker is shown. */
	bool bThresholdVisible = true;
};
