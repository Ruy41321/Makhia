// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/Inventory/MKHInventoryPanelWidget.h"
#include "MKHInventoryQuickSlotAreaWidget.generated.h"

class UInventoryItem;

/**
 * The consumable quick slots of the dashboard (W_QuickSlotArea): one entry per consumable quick slot
 * (SlotTag = Input.Ability.QuickSlot.Consumable.*). The entries themselves take the drops.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventoryQuickSlotAreaWidget : public UMKHInventoryPanelWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows a quick-slotted item in the entry of its quick slot.
	 *
	 * @param Item The quick-slotted consumable.
	 */
	void AddItem(UInventoryItem* Item);

	/**
	 * Empties the entry showing an item.
	 *
	 * @param ItemID Item that left its quick slot.
	 */
	void RemoveItemByID(int64 ItemID);

	/**
	 * Copies an item into the entry already showing it (e.g. a new quantity), or adds it when no entry does.
	 *
	 * @param Item UI view of the quick-slotted entry.
	 */
	void SyncItem(UInventoryItem* Item);

	/**
	 * An item entered, left or changed quick slot: it leaves its old entry and shows in its new one.
	 *
	 * @param Item The relocated consumable.
	 */
	void HandleQuickSlotRelocated(UInventoryItem* Item);
};
