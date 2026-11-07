// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/MKHBagSorting.h"
#include "UI/HUD/Inventory/MKHInventoryPanelWidget.h"
#include "MKHInventoryBagWidget.generated.h"

class UInventoryItem;
class UListView;
class UUserWidget;

/**
 * The bag (W_Bag): every item that is neither equipped nor quick-slotted, one TileView row each
 * (UMKHInventoryEntryWidget). Dropping an equipped armor or a quick-slotted item on it puts it back in the bag.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventoryBagWidget : public UMKHInventoryPanelWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Also gives the dashboard to the rows already generated. */
	virtual void SetDashboard(UInventoryDashboardWidget* InDashboard) override;

	/**
	 * Adds an item, or copies it into the row already showing the same ItemID.
	 *
	 * @param Item UI view of the bag entry.
	 */
	void SyncItem(UInventoryItem* Item);

	/**
	 * Removes the row of an item.
	 *
	 * @param ItemID Item to remove; ignored when the bag does not show it.
	 */
	void RemoveItemByID(int64 ItemID);

	/**
	 * An item entered or left a quick slot: a quick-slotted item leaves the bag, an item back from its slot returns.
	 *
	 * @param Item The relocated item (IsQuickSlotted tells where it is now).
	 */
	void HandleQuickSlotRelocated(UInventoryItem* Item);

	/**
	 * Sorts the bag and keeps it sorted when items arrive. Only the order on this screen changes: nothing reaches
	 * the server.
	 *
	 * @param InSortMode The criterion.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Bag")
	void SetSortMode(EMKHBagSortMode InSortMode);

	/** Gets the criterion in use. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Bag")
	EMKHBagSortMode GetSortMode() const { return SortMode; }

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Follows the TileView's row generation and release. */
	virtual void NativeOnInitialized() override;

	// ============================================================
	// Protected / Internal Logic (drag & drop)
	// ============================================================

	/** Highlights the bag for an equipped armor or a quick-slotted consumable. */
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Unequips a dropped armor, takes a dropped weapon or consumable out of its quick slot. */
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	/** New row widget: gives it the dashboard. */
	void HandleEntryGenerated(UUserWidget& EntryWidget) const;

	/** Row widget released: closes the action menu (it may point at that row). */
	void HandleEntryReleased(UUserWidget& EntryWidget) const;

	/**
	 * Row double clicked (the TileView row takes the double click, the entry never sees it): equips an armor,
	 * quick-slots anything else.
	 *
	 * @param ListItem The row's item (a UInventoryItem).
	 */
	void HandleItemDoubleClicked(UObject* ListItem) const;

	/** Puts the rows in the order of SortMode. */
	void ApplySort();

	/**
	 * Reads what the sort compares of an item.
	 *
	 * @param Item The item.
	 * @return Its sort key.
	 */
	FMKHBagSortKey MakeSortKey(const UInventoryItem& Item) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Rows of the bag (a TileView of W_InventoryEntry). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Bag", meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UListView> InventorySlots;

	/** Item shown by each row, by ItemID. */
	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<UInventoryItem>> ItemsMap;

	/** Criterion in use. */
	EMKHBagSortMode SortMode = EMKHBagSortMode::None;
};
