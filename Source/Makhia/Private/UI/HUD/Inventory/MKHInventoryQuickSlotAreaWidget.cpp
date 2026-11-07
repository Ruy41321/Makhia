// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventoryQuickSlotAreaWidget.h"

#include "Inventory/InventoryItem/InventoryItem.h"
#include "MKHLogChannels.h"
#include "UI/HUD/Inventory/MKHInventoryEntryWidget.h"

void UMKHInventoryQuickSlotAreaWidget::AddItem(UInventoryItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	UMKHInventoryEntryWidget* Entry = FindEntryBySlotTag(Item->GetQuickSlotTag());
	if (!IsValid(Entry))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHInventoryQuickSlotAreaWidget::AddItem: no entry for quick slot '%s'."),
			*Item->GetQuickSlotTag().ToString());
		return;
	}
	Entry->InitEntry(Item);
}

void UMKHInventoryQuickSlotAreaWidget::RemoveItemByID(int64 ItemID)
{
	for (UMKHInventoryEntryWidget* Entry : GetEntries())
	{
		if (IsValid(Entry) && Entry->GetItemID() != 0 && Entry->GetItemID() == ItemID)
		{
			Entry->ClearEntry();
		}
	}
}

void UMKHInventoryQuickSlotAreaWidget::SyncItem(UInventoryItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	const UMKHInventoryEntryWidget* Entry = FindEntryShowingItem(Item->GetItemID());
	if (IsValid(Entry))
	{
		Entry->GetInventoryItem()->CopyFrom(Item);
		return;
	}
	AddItem(Item);
}

void UMKHInventoryQuickSlotAreaWidget::HandleQuickSlotRelocated(UInventoryItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	RemoveItemByID(Item->GetItemID());
	if (Item->IsQuickSlotted())
	{
		AddItem(Item);
	}
}
