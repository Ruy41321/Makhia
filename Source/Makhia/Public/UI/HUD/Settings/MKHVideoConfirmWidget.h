// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHVideoConfirmWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * Modal asking the player to confirm a video mode that has already been applied.
 *
 * A resolution or window mode the display cannot show leaves a black screen, and a player who
 * cannot see the panel cannot undo it. So the change is applied first, then this modal counts
 * down: staying silent reverts. It is the same safety net every PC game ships with, and the
 * engine supports it directly through ConfirmVideoMode() and RevertVideoMode().
 */
UCLASS(Abstract)
class MAKHIA_API UMKHVideoConfirmWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Starts the countdown. The modal reverts on its own when it reaches zero. */
	void BeginCountdown();

	/** Keeps the applied video mode. Bind this to the Blueprint's Keep button. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Video")
	void ConfirmMode();

	/** Restores the previous video mode. Bind this to the Blueprint's Revert button. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Video")
	void RevertMode();

	/** Broadcast when the player keeps the new mode. */
	DECLARE_MULTICAST_DELEGATE(FVideoConfirmSignature);
	FVideoConfirmSignature OnModeConfirmed;

	/** Broadcast when the mode is rejected, either by the player or by the countdown. */
	FVideoConfirmSignature OnModeReverted;

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

	/** Click handlers; UButton delegates must be UFUNCTIONs. */
	UFUNCTION()
	void HandleKeepClicked();

	UFUNCTION()
	void HandleRevertClicked();

	/** Ticks one second off the countdown and reverts when it runs out. */
	void HandleCountdownTick();

	/** Writes the remaining seconds into the label. */
	void RefreshCountdownText() const;

	/** Stops the timer, whatever the outcome. */
	void StopCountdown();

	// ============================================================
	// Properties
	// ============================================================

	/** Countdown label. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> CountdownText;

	/** Optional buttons; without them the countdown alone decides. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> KeepButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> RevertButton;

	/** Seconds the player has to keep the new mode before it is undone. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Video", meta = (ClampMin = "3", ClampMax = "60"))
	int32 CountdownSeconds = 10;

	/** Countdown message; {0} is replaced with the seconds left. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Video")
	FText CountdownFormat = NSLOCTEXT("MKHSettings", "VideoConfirmCountdown",
		"Reverting in {0} seconds…");

private:

	/** Seconds still to go. */
	int32 SecondsLeft = 0;

	/** Handle of the one-second repeating timer. */
	FTimerHandle CountdownTimer;
};
