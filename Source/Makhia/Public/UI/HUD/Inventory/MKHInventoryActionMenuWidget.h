// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHInventoryActionMenuWidget.generated.h"

class UButton;
class UInventoryDashboardWidget;
class UInventoryItem;
class UMKHInventoryEntryWidget;
class UTextBlock;

/**
 * Context menu of an inventory entry (W_ActionMenu), opened by right click or gamepad A through
 * UInventoryDashboardWidget::OpenActionMenu: quick slot, use / equip / unequip, split, drop. With the gamepad the
 * generic quick slot button gives way to one button per quick slot (GamepadMenuNavigation.md, "Inventory").
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventoryActionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Sets the entry the menu acts on; call before adding the menu to the viewport.
	 *
	 * @param InEntry Entry the menu was opened on.
	 */
	void InitActionMenu(UMKHInventoryEntryWidget* InEntry);

	/** Entry the menu acts on, or nullptr. */
	UMKHInventoryEntryWidget* GetEntry() const { return Entry.Get(); }

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Binds the buttons. */
	virtual void NativeOnInitialized() override;

	/** Places the menu next to its entry and sets every button for the entry's item. */
	virtual void NativeConstruct() override;

	/** Gamepad B closes the menu. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Main action: "Unequip" on equipment entries, "Equip" for equipment, "Use" otherwise; hidden for weapons. */
	void SetupMainAction() const;

	/** Generic quick slot button: "Remove QuickSlot" / "Add QuickSlot", only for weapons and consumables. */
	void SetupQuickSlotButton() const;

	/** Split button, only for splittable stacks. */
	void SetupSplitButton() const;

	/** Gamepad: one button per quick slot choice; the generic button stays only to remove. */
	void SetupGamepadQuickSlotButtons() const;

	/**
	 * Shows a per-slot quick slot button when the gamepad offers that choice.
	 *
	 * @param Button      The button.
	 * @param Label       Its text.
	 * @param ChoiceIndex Index of the choice it stands for.
	 */
	void SetupQuickSlotChoiceButton(UButton* Button, UTextBlock* Label, int32 ChoiceIndex) const;

	/**
	 * Assigns the item to the ChoiceIndex-th gamepad quick slot choice, then closes the menu.
	 *
	 * @param ChoiceIndex Index of the choice.
	 */
	void AssignQuickSlotChoice(int32 ChoiceIndex);

	/** Closes the menu through the dashboard. */
	void CloseSelf() const;

	/** Dashboard of the entry, or nullptr. */
	UInventoryDashboardWidget* GetDashboard() const;

	/** Item of the entry, or nullptr. */
	UInventoryItem* GetItem() const;

	UFUNCTION()
	void HandleMainActionClicked();

	UFUNCTION()
	void HandleQuickSlotClicked();

	UFUNCTION()
	void HandleQuickSlot1Clicked();

	UFUNCTION()
	void HandleQuickSlot2Clicked();

	UFUNCTION()
	void HandleQuickSlot3Clicked();

	UFUNCTION()
	void HandleSplitClicked();

	UFUNCTION()
	void HandleDropClicked();

protected:
	// ============================================================
	// Properties
	// ============================================================

	/** Box placed next to the entry (a Canvas Panel slot). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidget))
	TObjectPtr<UWidget> ActionBox;

	/** Use / equip / unequip. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidget))
	TObjectPtr<UButton> MainActionButton;

	/** Label of MainActionButton. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidget))
	TObjectPtr<UTextBlock> MainActionText;

	/** Generic add to / remove from quick slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidget))
	TObjectPtr<UButton> QuickSlotButton;

	/** Label of QuickSlotButton. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidget))
	TObjectPtr<UTextBlock> QuickSlotText;

	/** Gamepad: first quick slot choice. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuickSlot1Button;

	/** Label of QuickSlot1Button. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuickSlot1Text;

	/** Gamepad: second quick slot choice. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuickSlot2Button;

	/** Label of QuickSlot2Button. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuickSlot2Text;

	/** Gamepad: third quick slot choice. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuickSlot3Button;

	/** Label of QuickSlot3Button. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuickSlot3Text;

	/** Opens the split quantity popup. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> SplitButton;

	/** Drops the item (quantity popup for stacks). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|ActionMenu", meta = (BindWidget))
	TObjectPtr<UButton> DropButton;

private:
	/** Entry the menu acts on. */
	TWeakObjectPtr<UMKHInventoryEntryWidget> Entry;
};
