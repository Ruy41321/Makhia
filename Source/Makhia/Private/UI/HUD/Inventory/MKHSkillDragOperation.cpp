// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHSkillDragOperation.h"

#include "UI/HUD/HUDOverlay/QuickSlots/MKHSkillQuickSlotWidget.h"
#include "UI/HUD/Inventory/MKHInventoryWeaponSkillsWidget.h"

void UMKHSkillDragOperation::InitSkillDrag(UMKHInventoryWeaponSkillsWidget* InRow, UMKHSkillQuickSlotWidget* InSourceSlot)
{
	Row = InRow;
	SourceSlot = InSourceSlot;
}

void UMKHSkillDragOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Drop_Implementation(PointerEvent);
	if (UMKHInventoryWeaponSkillsWidget* OwningRow = Row.Get())
	{
		OwningRow->EndSkillDrag();
	}
}

void UMKHSkillDragOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);
	if (UMKHInventoryWeaponSkillsWidget* OwningRow = Row.Get())
	{
		OwningRow->EndSkillDrag();
	}
}
