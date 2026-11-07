// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "MKHInventoryEntryWidget.generated.h"

class UImage;
class UInventoryDashboardWidget;
class UInventoryItem;
class UMKHDropHighlightWidget;
class UTextBlock;

/**
 * One item slot of the inventory dashboard (W_InventoryEntry): a bag row (TileView entry), an equipment slot or a
 * quick slot of the dashboard. Shows the item (icon, rarity background and frame, quantity) and turns the player's
 * input into dashboard requests: right click / gamepad A open the action menu, double click quick-equips,
 * Ctrl+click drops, Shift+click splits, left drag starts an entry drag. Quick slot entries also accept drops.
 * Full model: Documentation/InventoryDashboard.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventoryEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows an item and follows its updates (OnItemUpdatedDelegate) until the entry is cleared or shows another one.
	 *
	 * @param InItem UI view of the item.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Entry")
	void InitEntry(UInventoryItem* InItem);

	/** Drops the item (closing the action menu) and applies the empty look, also to an entry never filled. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Entry")
	void ClearEntry();

	/** Re-renders the entry from its item (icon, rarity, quantity). */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Entry")
	void RefreshEntry();

	/**
	 * Sets the dashboard the entry sends its requests to (set by its panel).
	 *
	 * @param InDashboard The owning dashboard.
	 */
	void SetDashboard(UInventoryDashboardWidget* InDashboard);

	/** Widget the hover tooltip stands next to: the visible frame of the slot. */
	UWidget* GetToolTipAnchor();

	/** Dashboard the entry belongs to, or nullptr before its panel set it. */
	UInventoryDashboardWidget* GetDashboard() const { return Dashboard.Get(); }

	/** Item shown, or nullptr when empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Entry")
	UInventoryItem* GetInventoryItem() const { return InventoryItem; }

	/** ItemID of the item shown, or 0 when empty. */
	int64 GetItemID() const;

	/** True while the entry shows an item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Entry")
	bool IsEntryInitialized() const { return bIsInit; }

	/** Equipment slot or quick slot this entry stands for (empty for bag rows). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Entry")
	FGameplayTag GetSlotTag() const { return SlotTag; }

	/** True for the equipment panel's entries (armor slots and weapon quick slots). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Entry")
	bool IsEquipEntry() const { return bIsEquipEntry; }

	/** True for quick slot entries (weapon slots of the equipment panel, consumable slots). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Entry")
	bool IsQuickSlotEntry() const { return bIsQuickSlot; }

	/**
	 * Hides the content while the item is dragged away (UMKHInventoryEntryDragOperation).
	 *
	 * @param bHidden True while dragging.
	 */
	void SetContentHidden(bool bHidden);

	/**
	 * Tooltip of the entry for the gamepad (ToolTipWidgetDelegate, read by the navigation subsystem); empty with the
	 * mouse, which uses the dashboard's hover tooltip instead of Slate's, and while the action menu is open.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Entry")
	UWidget* GetToolTipWidget() const;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Binds the tooltip delegate to GetToolTipWidget. */
	virtual void NativeOnInitialized() override;

	/** An entry that shows nothing yet gets the empty look (never the designer placeholder). */
	virtual void NativeConstruct() override;

	/** Stops following the item. */
	virtual void NativeDestruct() override;

	/** TileView row: shows the list item (bag rows are never equipment entries). */
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	// ============================================================
	// Protected / Internal Logic (input)
	// ============================================================

	/** Mouse over the entry: the dashboard shows the item tooltip. */
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Mouse away: the dashboard hides the tooltip. */
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	/** Right click: action menu; Ctrl+left: drop the whole item; Shift+left: split; left: drag detection. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Left double click: quick equip / quick slot (QuickEquip). */
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Gamepad A opens the action menu. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Starts an entry drag carrying the item. */
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	/** Highlights the entry when it can receive the dragged item or split. */
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Removes the highlight. */
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Quick slot entries take a split or a quick-slottable item; other drops bubble to the panel. */
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** The drag started here ended nowhere: dropped on the ground when released outside the inventory. */
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	/** Item updated callback: re-renders the entry. */
	UFUNCTION()
	void HandleItemUpdated();

	/** Stops following the current item's updates. */
	void UnbindFromItem();

	/** Double click / fast action: unequip or equip armor, remove from or add to a quick slot. */
	void QuickEquip() const;

	/**
	 * Returns the item of an entry drag this quick slot entry can take: not equipped, a weapon for the equipment
	 * panel's slots, a consumable for the others.
	 *
	 * @param Operation The operation over the entry.
	 * @return The item, or nullptr when the entry cannot take it.
	 */
	UInventoryItem* GetQuickSlottableItem(const UDragDropOperation* Operation) const;

	/** Applies the empty look (no icon, transparent background, grey frame, no quantity). */
	void ApplyEmptyLook();

	/**
	 * Shows the quantity (only above one).
	 *
	 * @param Quantity Quantity to show.
	 */
	void SetQuantity(int32 Quantity);

	/**
	 * Shows or hides the drop highlight.
	 *
	 * @param bActive True to show it.
	 */
	void SetDropHighlight(bool bActive) const;

protected:
	// ============================================================
	// Properties
	// ============================================================

	/** Everything hidden while the item is dragged away. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Entry", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EntryContent;

	/** Item icon. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Entry", meta = (BindWidget))
	TObjectPtr<UImage> SlotIcon;

	/** Rarity background (rarity MI_Banner). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Entry", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SlotBg;

	/** Frame tinted with the rarity colour. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Entry", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SlotFrame;

	/** Stack quantity, shown above one. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Entry", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	/** Highlight shown while a drop is possible. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Entry", meta = (BindWidgetOptional))
	TObjectPtr<UMKHDropHighlightWidget> DropHighlight;

	/** Equipment slot (armor) or quick slot this entry stands for; empty for bag rows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Entry")
	FGameplayTag SlotTag;

	/** Entry of the equipment panel (armor slot or weapon quick slot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Entry")
	bool bIsEquipEntry = false;

	/** Quick slot entry (accepts quick-slottable drops). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Entry")
	bool bIsQuickSlot = false;

	/** Shows the wide HUD picture of a weapon instead of its square icon (weapon rows of W_WeaponLoadout). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Entry")
	bool bUseWeaponHudIcon = false;

	/** Grey tint of the empty frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Entry")
	FLinearColor EmptyFrameTint = FLinearColor(0.5f, 0.5f, 0.5f, 1.f);

private:
	/** Item shown. */
	UPROPERTY(Transient)
	TObjectPtr<UInventoryItem> InventoryItem;

	/** True while the entry shows an item. */
	bool bIsInit = false;

	/** Dashboard receiving the entry's requests. */
	TWeakObjectPtr<UInventoryDashboardWidget> Dashboard;
};
