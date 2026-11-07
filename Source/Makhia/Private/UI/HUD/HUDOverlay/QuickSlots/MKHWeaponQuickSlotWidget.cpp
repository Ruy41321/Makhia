// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/QuickSlots/MKHWeaponQuickSlotWidget.h"

#include "Inventory/InventoryItem/InventoryItem.h"

// ============================================================
// Protected / Internal Logic
// ============================================================

UTexture2D* UMKHWeaponQuickSlotWidget::ResolveSlotIcon() const
{
	return IsValid(InventoryItem) ? InventoryItem->GetWeaponHudIcon() : nullptr;
}
