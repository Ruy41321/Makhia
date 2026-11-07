// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/QuickSlots/MKHQuickSlotContainerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "MKHLogChannels.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHQuickSlotWidget.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHQuickSlotContainerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureQuickSlotsGathered();
}

void UMKHQuickSlotContainerWidget::NativePreConstruct()
{
	EnsureQuickSlotsGathered();

	Super::NativePreConstruct();
}

// ============================================================
// Public Interface
// ============================================================

bool UMKHQuickSlotContainerWidget::AddItem(UInventoryItem* Item)
{
	if (!IsValid(Item))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: AddItem called without an item."), *GetName());
		return false;
	}

	// An item lives in one slot only: drop it from the one it leaves before showing it in the new one.
	RemoveItemByID(Item->GetItemID());

	if (!Item->IsQuickSlotted())
	{
		return false;
	}

	UMKHQuickSlotWidget* TargetSlot = FindSlotByTag(Item->GetQuickSlotTag());
	if (!IsValid(TargetSlot))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: no slot for quick slot tag %s."), *GetName(), *Item->GetQuickSlotTag().ToString());
		return false;
	}

	AssignSlotItem(*TargetSlot, Item);
	return true;
}

bool UMKHQuickSlotContainerWidget::RemoveItemByID(const int64 ItemID)
{
	UMKHQuickSlotWidget* QuickSlot = FindSlotByItemID(ItemID);
	if (!IsValid(QuickSlot))
	{
		return false;
	}

	ClearSlotItem(*QuickSlot);
	return true;
}

void UMKHQuickSlotContainerWidget::SyncItem(UInventoryItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	const UMKHQuickSlotWidget* QuickSlot = FindSlotByItemID(Item->GetItemID());
	if (!IsValid(QuickSlot) || !IsValid(QuickSlot->GetSlotItem()))
	{
		AddItem(Item);
		return;
	}

	// The shown item broadcasts its update, which refreshes every slot showing it.
	QuickSlot->GetSlotItem()->CopyFrom(Item);
}

void UMKHQuickSlotContainerWidget::PlayActivationFlash(const FGameplayTag& QuickSlotTag)
{
	if (UMKHQuickSlotWidget* QuickSlot = FindSlotByTag(QuickSlotTag); IsValid(QuickSlot))
	{
		QuickSlot->PlayActivationFlash();
	}
}

UMKHQuickSlotWidget* UMKHQuickSlotContainerWidget::FindSlotByTag(const FGameplayTag& QuickSlotTag) const
{
	for (UMKHQuickSlotWidget* QuickSlot : QuickSlots)
	{
		if (IsValid(QuickSlot) && QuickSlot->GetQuickSlotTag().MatchesTagExact(QuickSlotTag))
		{
			return QuickSlot;
		}
	}
	return nullptr;
}

UMKHQuickSlotWidget* UMKHQuickSlotContainerWidget::FindSlotByItemID(const int64 ItemID) const
{
	if (ItemID == 0)
	{
		return nullptr;
	}

	for (UMKHQuickSlotWidget* QuickSlot : QuickSlots)
	{
		if (IsValid(QuickSlot) && QuickSlot->GetSlotItemID() == ItemID)
		{
			return QuickSlot;
		}
	}
	return nullptr;
}

// ============================================================
// Protected / Internal Logic (subclass hooks)
// ============================================================

void UMKHQuickSlotContainerWidget::GatherQuickSlots(TArray<UMKHQuickSlotWidget*>& OutSlots) const
{
	if (!IsValid(WidgetTree))
	{
		return;
	}

	WidgetTree->ForEachWidget([&OutSlots](UWidget* Widget)
	{
		if (UMKHQuickSlotWidget* QuickSlot = Cast<UMKHQuickSlotWidget>(Widget))
		{
			OutSlots.Add(QuickSlot);
		}
	});
}

void UMKHQuickSlotContainerWidget::AssignSlotItem(UMKHQuickSlotWidget& QuickSlot, UInventoryItem* Item)
{
	QuickSlot.SetSlotItem(Item);
}

void UMKHQuickSlotContainerWidget::ClearSlotItem(UMKHQuickSlotWidget& QuickSlot)
{
	QuickSlot.ResetSlot();
}

void UMKHQuickSlotContainerWidget::EnsureQuickSlotsGathered()
{
	if (!QuickSlots.IsEmpty())
	{
		return;
	}

	TArray<UMKHQuickSlotWidget*> Gathered;
	GatherQuickSlots(Gathered);
	QuickSlots.Append(Gathered);
}
