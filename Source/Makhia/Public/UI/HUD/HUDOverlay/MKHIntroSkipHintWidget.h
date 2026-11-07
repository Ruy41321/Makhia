// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/MKHInputDeviceTypes.h"
#include "UI/MKHSystemWidget.h"
#include "MKHIntroSkipHintWidget.generated.h"

class UInputAction;
class UProgressBar;
class UTextBlock;

/**
 * "Hold [key] to skip" hint shown by AMKHMatchIntro while the intro flies (C++ base of W_IntroSkipHint): one clean
 * line of text with the key name inside it (PromptFormat, {Key} = the player's key for the skip action, read like
 * the preparation-skip prompt) and an optional bar that fills while the key is held.
 * See Documentation/Handoff/MakhionCinematics.md.
 */
UCLASS()
class MAKHIA_API UMKHIntroSkipHintWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Sets the action whose key the prompt names and refreshes the text.
	 *
	 * @param InSkipAction The skip-intro action (IA_SkipIntro).
	 */
	UFUNCTION(BlueprintCallable, Category = "Intro")
	void SetSkipAction(UInputAction* InSkipAction);

	/**
	 * Shows how long the skip key has been held.
	 *
	 * @param HoldFraction 0 (released) to 1 (skipping).
	 */
	UFUNCTION(BlueprintCallable, Category = "Intro")
	void SetHoldProgress(float HoldFraction);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Shows the prompt with a sample key in the designer. */
	virtual void NativePreConstruct() override;

	/** Starts with an empty bar and follows device switches. */
	virtual void NativeConstruct() override;

	/** Stops following device switches. */
	virtual void NativeDestruct() override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Device switch callback: shows the key of the device now in use.
	 *
	 * @param NewDevice  Device family the player just switched to.
	 */
	UFUNCTION()
	void HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice);

	/** Writes PromptFormat with the current key into Txt_SkipHint. */
	void RefreshPrompt() const;

	/**
	 * Blueprint hook on every progress change (e.g. to highlight the hint while held).
	 *
	 * @param HoldFraction 0 (released) to 1 (skipping).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Intro")
	void BP_OnHoldProgressChanged(float HoldFraction);

	// ============================================================
	// Properties
	// ============================================================

	/** Prompt text; {Key} is replaced by the display name of the key bound to the skip action. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intro")
	FText PromptFormat = NSLOCTEXT("MKHIntro", "SkipHintFormat", "Hold {Key} to skip");

	/** The prompt line. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SkipHint;

	/** Bar filled with the held fraction; hidden while the key is up. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HoldProgressBar;

private:
	/** Action whose key the prompt names. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SkipAction;
};
