// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/QuickSlots/MKHConsumableQuickSlotWidget.h"

#include "Inventory/InventoryItem/InventoryItem.h"

// ============================================================
// Protected / Internal Logic
// ============================================================

UTexture2D* UMKHConsumableQuickSlotWidget::ResolveSlotIcon() const
{
	return IsValid(InventoryItem) ? InventoryItem->GetIcon() : nullptr;
}

FGameplayTag UMKHConsumableQuickSlotWidget::ResolveCooldownTag() const
{
	return IsValid(InventoryItem) ? InventoryItem->GetConsumableCooldownTag() : FGameplayTag();
}

int32 UMKHConsumableQuickSlotWidget::ResolveDisplayedQuantity() const
{
	return IsValid(InventoryItem) ? InventoryItem->GetQuantity() : 0;
}
