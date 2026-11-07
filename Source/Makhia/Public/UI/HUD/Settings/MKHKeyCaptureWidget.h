// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/MKHInputDeviceTypes.h"
#include "UI/MKHSystemWidget.h"
#include "UI/Navigation/MKHNavigableScreen.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "MKHKeyCaptureWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * Modal overlay that captures the next key, mouse button or gamepad button the player produces.
 *
 * It listens to one device family at a time (keyboard and mouse, or gamepad) and ignores the other, so
 * a page can never receive a key it cannot store. Deciding whether the key is already taken or safe to
 * write is the caller's job (UMKHKeyboardSettingsWidget). Escape cancels the capture and, on keyboard
 * captures only, Delete clears the slot, so those keys can never be bound from this screen. Stick
 * directions are never captured: the sticks are reserved for movement and camera.
 *
 * When the caller detects a conflict it calls ShowConflict() instead of closing: the widget then
 * stops listening and waits for the player to confirm or cancel through the Blueprint buttons.
 *
 * Gamepad captures commit on release, since every button can be bound: holding a button for
 * HoldToCancelSeconds cancels instead. On a keyboard capture the gamepad B cancels.
 *
 * For a chorded action the chord button (Alternate Actions) is a modifier, not a candidate: the
 * chord is fixed in the mapping context, so pressing LB + RT stores RT (see SettingsArchitecture.md).
 */
UCLASS(Abstract)
class MAKHIA_API UMKHKeyCaptureWidget : public UMKHSystemWidget, public IMKHNavigableScreen
{
	GENERATED_BODY()

public:
	//~ Begin IMKHNavigableScreen Interface
	/** While listening, the focus stays on the modal so every button reaches NativeOnKeyDown. */
	virtual bool WantsRootFocus() const override { return bIsListening; }
	//~ End IMKHNavigableScreen Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Starts listening for the next input.
	 *
	 * @param InActionDisplayName  Name of the action being rebound, shown in the prompt.
	 * @param InSlot               Slot being rebound, shown in the prompt.
	 * @param InDevice             Device family whose keys are accepted.
	 * @param InChordKey           Button of the chord the action's mapping requires (e.g. LB), shown as
	 *                             "LB + …" and never captured, so the player can press the full combination.
	 *                             Invalid for an action without a chord.
	 */
	void BeginCapture(const FText& InActionDisplayName, EPlayerMappableKeySlot InSlot, EMKHInputDevice InDevice,
		const FKey& InChordKey = EKeys::Invalid);

	/**
	 * Switches the modal from listening to conflict confirmation.
	 *
	 * @param InConflictMessage  Player-facing explanation of which action already owns the key.
	 */
	void ShowConflict(const FText& InConflictMessage);

	/** Confirms the pending conflicting key. Bind this to the Blueprint's Replace button. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Keybinds")
	void ConfirmPendingKey();

	/** Aborts the capture without changing anything. Bind this to the Blueprint's Cancel button. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Keybinds")
	void CancelCapture();

	/** Broadcast with the captured key once the player produces one. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FKeyCapturedSignature, const FKey& /*CapturedKey*/);
	FKeyCapturedSignature OnKeyCaptured;

	/** Broadcast when the player confirms a key that conflicts with an existing binding. */
	FKeyCapturedSignature OnConflictConfirmed;

	/** Broadcast when the player asks to clear the slot instead of binding a key. */
	DECLARE_MULTICAST_DELEGATE(FKeyCaptureSimpleSignature);
	FKeyCaptureSimpleSignature OnClearRequested;

	/** Broadcast when the capture is aborted. */
	FKeyCaptureSimpleSignature OnCaptureCancelled;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Keyboard and gamepad button capture. Escape cancels, Delete clears (keyboard only), any other key
	 * of the accepted device is reported as the new key. Gamepad triggers arrive here as digital keys.
	 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Commits the gamepad button pressed while listening, once it is released before HoldToCancelSeconds. */
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Mouse capture on keyboard captures; on gamepad captures clicks pass through to the Cancel button. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Blueprint hook fired when the modal starts listening, used to reset the visual state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings|Keybinds", meta = (DisplayName = "On Capture Started"))
	void BP_OnCaptureStarted();

	/** Blueprint hook fired when a conflict prompt must be shown instead of the listening state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings|Keybinds", meta = (DisplayName = "On Conflict Shown"))
	void BP_OnConflictShown();

	/**
	 * Reports a captured key to the owner, unless it is one of the reserved control keys.
	 *
	 * @param CapturedKey  Key produced by the player.
	 * @return True when the event was consumed.
	 */
	bool ProcessCapturedKey(const FKey& CapturedKey);

	/**
	 * Whether a key may be bound on the device being captured.
	 *
	 * @param Key  Key produced by the player.
	 * @return False for keys of the other device and, on gamepad, for stick directions and axes.
	 */
	bool IsCapturableKey(const FKey& Key) const;

	/**
	 * Starts timing a gamepad button pressed while listening; it is committed on release.
	 *
	 * @param Key  Button pressed.
	 */
	void BeginGamepadPress(const FKey& Key);

	/** Hold timer callback: the button was held for HoldToCancelSeconds, so the capture is cancelled. */
	void HandleHoldToCancel();

	/**
	 * Prompt under the action name: the slot being rebound, or for the controller the chord to keep
	 * ("LB + … (hold to cancel)").
	 *
	 * @param InSlot  Slot being rebound.
	 * @return The prompt text.
	 */
	FText BuildSlotPrompt(EPlayerMappableKeySlot InSlot) const;

	/** Replace button handler; UButton delegates must be UFUNCTIONs. */
	UFUNCTION()
	void HandleReplaceClicked();

	/** Cancel button handler. */
	UFUNCTION()
	void HandleCancelClicked();

	/**
	 * Shows or hides the conflict half of the modal.
	 *
	 * @param bVisible  True while the player is being asked to confirm stealing a key.
	 */
	void SetConflictUIVisible(bool bVisible) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Optional label naming the action being rebound. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> ActionNameText;

	/** Optional label naming the slot being rebound. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> SlotNameText;

	/** Optional label carrying the conflict explanation. Hidden until a conflict occurs. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> ConflictText;

	/** Confirms a conflicting key. Hidden until a conflict occurs. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> ReplaceButton;

	/** Aborts the capture. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> CancelButton;

	/** Seconds a gamepad button must be held to cancel a gamepad capture instead of binding the button. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds", meta = (ClampMin = "0.2"))
	float HoldToCancelSeconds = 1.f;

private:

	/** Key held while the player is asked to confirm a conflicting binding. */
	FKey PendingKey;

	/** True while the widget is listening for input; false once a conflict prompt is up. */
	bool bIsListening = false;

	/** Device family whose keys are accepted by the current capture. */
	EMKHInputDevice CaptureDevice = EMKHInputDevice::KeyboardMouse;

	/** Gamepad button pressed while listening, committed on release. */
	FKey PressedGamepadKey;

	/** Chord button of the action being rebound, swallowed instead of captured; invalid without a chord. */
	FKey ChordKey;

	/** Fires HandleHoldToCancel while PressedGamepadKey is held. */
	FTimerHandle HoldToCancelTimer;
};
