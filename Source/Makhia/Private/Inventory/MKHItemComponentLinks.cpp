// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/MKHItemComponentLinks.h"

#include "Equipment/EquipmentManagerComponent.h"
#include "Inventory/InventoryComponent.h"
#include "QuickSlot/QuickSlotManagerComponent.h"

// ============================================================
// Public Interface
// ============================================================

void FMKHItemComponentLinks::Bind(UInventoryComponent& Inventory, UEquipmentManagerComponent& Equipment, UQuickSlotManagerComponent& QuickSlots)
{
	Inventory.EquipmentItemUnequippedDelegate.AddWeakLambda(&Equipment, [&Equipment](int64 ItemID)
	{
		Equipment.UnEquipItemByItemID(ItemID);
	});

	// Equipping an item from the inventory on use.
	Inventory.EquipmentItemUsedDelegate.AddWeakLambda(&Equipment, [&Equipment](const FMKHItemEntry& UsedItem)
	{
		Equipment.EquipItem(UsedItem);
	});

	// Returning the item to the inventory on unequip.
	Equipment.EquipmentList.UnEquippedEntryDelegate.AddWeakLambda(&Inventory, [&Inventory](const FRPGEquipmentEntry& UnEquippedEntry)
	{
		Inventory.AddItemEntry(UnEquippedEntry.Item);
	});

	Inventory.InventoryList.QuickSlotItemRelocatedDelegate.AddWeakLambda(&QuickSlots, [&QuickSlots](const FRPGInventoryEntry& QuickSlottedEntry)
	{
		if (QuickSlottedEntry.bIsQuickSlotted)
		{
			QuickSlots.AddQuickSlot(QuickSlottedEntry.QuickSlotTag, QuickSlottedEntry.Item.ItemID);
		}
		else
		{
			QuickSlots.RemoveQuickSlot(QuickSlottedEntry.Item.ItemID);
		}
	});
}
