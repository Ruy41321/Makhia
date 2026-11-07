// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventoryEntryDragOperation.h"

#include "Inventory/InventoryItem/InventoryItem.h"
#include "UI/HUD/Inventory/MKHInventoryEntryWidget.h"

void UMKHInventoryEntryDragOperation::InitEntryDrag(UMKHInventoryEntryWidget* InSourceEntry, UInventoryItem* Item)
{
	SourceEntry = InSourceEntry;
	Payload = Item;
}

UInventoryItem* UMKHInventoryEntryDragOperation::GetDraggedItem(const UDragDropOperation* Operation)
{
	const UMKHInventoryEntryDragOperation* EntryDrag = Cast<UMKHInventoryEntryDragOperation>(Operation);
	return IsValid(EntryDrag) ? Cast<UInventoryItem>(EntryDrag->Payload) : nullptr;
}

void UMKHInventoryEntryDragOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Dragged_Implementation(PointerEvent);
	SetSourceContentHidden(true);
}

void UMKHInventoryEntryDragOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Drop_Implementation(PointerEvent);
	SetSourceContentHidden(false);
}

void UMKHInventoryEntryDragOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);
	SetSourceContentHidden(false);
}

void UMKHInventoryEntryDragOperation::SetSourceContentHidden(bool bHidden) const
{
	if (UMKHInventoryEntryWidget* Entry = SourceEntry.Get())
	{
		Entry->SetContentHidden(bHidden);
	}
}
