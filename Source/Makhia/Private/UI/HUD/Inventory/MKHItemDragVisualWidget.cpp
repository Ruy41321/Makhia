// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHItemDragVisualWidget.h"

#include "Components/Image.h"
#include "Data/MKHDataSubsystem.h"
#include "Equipment/Rarity/RarityDefinition.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "Materials/MaterialInstance.h"

void UMKHItemDragVisualWidget::SetItem(UInventoryItem* InItem)
{
	InventoryItem = InItem;
	RefreshVisual();
}

void UMKHItemDragVisualWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshVisual();
}

void UMKHItemDragVisualWidget::RefreshVisual()
{
	if (!IsValid(InventoryItem) || !IsValid(SlotIcon))
	{
		return;
	}

	SlotIcon->SetBrushFromTexture(InventoryItem->GetIcon());
	SlotIcon->SetBrushTintColor(FSlateColor(FLinearColor::White));

	FRarityDefinition Rarity;
	if (IsValid(SlotBg) && UMKHDataSubsystem::FindRarityDefinitionForContext(this, InventoryItem->GetRarityTag(), Rarity))
	{
		SlotBg->SetBrushTintColor(FSlateColor(FLinearColor::White));
		SlotBg->SetBrushFromMaterial(Rarity.RarityColors.MI_Square);
	}
}
