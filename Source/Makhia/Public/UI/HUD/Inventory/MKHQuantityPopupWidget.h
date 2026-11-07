// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Navigation/MKHNavigableScreen.h"
#include "MKHQuantityPopupWidget.generated.h"

class UButton;
class UInventoryItem;
class USlider;
class UTextBlock;
class UWidget;

/**
 * Broadcast when the player confirms the quantity chosen in the popup.
 *
 * @param ItemID   Inventory stack the quantity refers to.
 * @param Quantity Selected quantity, between 1 and the stack size.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQuantityPopupConfirmedSignature, int64, ItemID, int32, Quantity);

/**
 * Full-screen popup that lets the player pick how many items of a stack an action applies to (drop, split).
 * Opened by UInventoryDashboardWidget; the Blueprint subclass only provides the layout (BindWidget members).
 * Confirming broadcasts OnQuantityConfirmed and closes the popup; clicking outside the panel or pressing
 * Escape cancels it. With the gamepad the focus stays on the popup itself: left / right change the quantity,
 * A confirms, B cancels.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHQuantityPopupWidget : public UUserWidget, public IMKHNavigableScreen
{
	GENERATED_BODY()

public:
	//~ Begin IMKHNavigableScreen Interface
	virtual bool WantsRootFocus() const override { return true; }
	//~ End IMKHNavigableScreen Interface

protected:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Binds the slider and buttons and makes the whole popup focusable and hit-testable. */
	virtual void NativeOnInitialized() override;

	/** Takes keyboard focus so Escape / Enter / arrows reach the popup. */
	virtual void NativeConstruct() override;

	/** Escape / gamepad B cancel, Enter / gamepad A confirm, Left / Right arrows change the quantity by one. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Gamepad directions: left / right change the quantity by one, and the focus never leaves the popup. */
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent,
		const FNavigationReply& InDefaultReply) override;

	/** A click outside PanelBox (on the backdrop) cancels the popup; clicks are never passed to the widgets below. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Sets the popup up for an inventory stack: item name, confirm label, slider range 1..stack size, starting at 1.
	 *
	 * @param Item         The stack the action applies to.
	 * @param ConfirmLabel Text of the confirm button (e.g. "Drop", "Split").
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quantity")
	void InitQuantityPopup(const UInventoryItem* Item, const FText& ConfirmLabel);

	/**
	 * Selects a quantity (clamped to 1..stack size) and updates the slider and text.
	 *
	 * @param NewQuantity Requested quantity.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quantity")
	void SetSelectedQuantity(int32 NewQuantity);

	/** Gets the quantity currently selected. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quantity")
	int32 GetSelectedQuantity() const { return SelectedQuantity; }

	/** Broadcasts OnQuantityConfirmed with the selected quantity, then closes the popup. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quantity")
	void ConfirmQuantity();

	/** Closes the popup without applying anything. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quantity")
	void CancelQuantity();

	/** Broadcast when the player confirms the quantity. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Quantity")
	FQuantityPopupConfirmedSignature OnQuantityConfirmed;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Snaps the slider to whole quantities and stores the selection. */
	UFUNCTION()
	void HandleSliderValueChanged(float Value);

	/** Confirm button handler. */
	UFUNCTION()
	void HandleConfirmClicked();

	/** Cancel button handler. */
	UFUNCTION()
	void HandleCancelClicked();

	// ============================================================
	// Properties
	// ============================================================

	/** The visible panel: clicks inside it keep the popup open, clicks outside cancel it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Quantity", meta = (BindWidget))
	TObjectPtr<UWidget> PanelBox;

	/** Chooses the quantity (whole steps, 1..stack size). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Quantity", meta = (BindWidget))
	TObjectPtr<USlider> QuantitySlider;

	/** Shows "selected / stack size". */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Quantity", meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;

	/** Shows the item name and stack size (optional). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Quantity", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	/** Applies the action with the selected quantity. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Quantity", meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	/** Label of the confirm button, set from InitQuantityPopup (optional). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Quantity", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmText;

	/** Closes the popup without applying anything. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Quantity", meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

private:
	/** Stack the popup refers to. */
	int64 ItemID = 0;

	/** Stack size when the popup was opened: upper bound of the selection. */
	int32 MaxQuantity = 1;

	/** Quantity currently selected. */
	int32 SelectedQuantity = 1;
};
