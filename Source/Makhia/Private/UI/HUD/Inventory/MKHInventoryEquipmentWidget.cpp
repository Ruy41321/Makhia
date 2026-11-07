// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventoryEquipmentWidget.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Cosmetics/MKHSkinCatalog.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "MKHLogChannels.h"
#include "UI/HUD/Inventory/InventoryDashboardWidget.h"
#include "UI/HUD/Inventory/MKHInventoryEntryDragOperation.h"
#include "UI/HUD/Inventory/MKHInventoryEntryWidget.h"
#include "UI/HUD/Inventory/MKHInventoryWeaponSkillsWidget.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHInventoryEquipmentWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SkillRows.Reset();
	if (IsValid(WidgetTree))
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (UMKHInventoryWeaponSkillsWidget* SkillRow = Cast<UMKHInventoryWeaponSkillsWidget>(Widget))
			{
				SkillRows.Add(SkillRow);
			}
		});
	}
}

void UMKHInventoryEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshSkinPortrait();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHInventoryEquipmentWidget::ShowItem(UInventoryItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	const FGameplayTag& TargetSlot = Item->IsQuickSlotted() ? Item->GetQuickSlotTag() : Item->GetSlotTag();
	if (UMKHInventoryEntryWidget* Entry = FindEntryBySlotTag(TargetSlot))
	{
		Entry->InitEntry(Item);
	}
	for (UMKHInventoryWeaponSkillsWidget* SkillRow : SkillRows)
	{
		if (IsValid(SkillRow) && Item->IsQuickSlotted() && SkillRow->GetQuickSlotTag().MatchesTagExact(TargetSlot))
		{
			SkillRow->SetWeapon(Item);
		}
	}
}

void UMKHInventoryEquipmentWidget::ClearItem(const FGameplayTag& OffSlotTag, int64 ItemID)
{
	for (UMKHInventoryEntryWidget* Entry : GetEntries())
	{
		const UInventoryItem* ShownItem = IsValid(Entry) ? Entry->GetInventoryItem() : nullptr;
		if (!IsValid(ShownItem))
		{
			continue;
		}

		const bool bMatches = ItemID != 0
			? ShownItem->GetItemID() == ItemID
			: ShownItem->GetSlotTag().MatchesTagExact(OffSlotTag);
		if (bMatches)
		{
			Entry->ClearEntry();
		}
	}
	for (UMKHInventoryWeaponSkillsWidget* SkillRow : SkillRows)
	{
		if (IsValid(SkillRow) && ItemID != 0 && SkillRow->GetWeaponItemID() == ItemID)
		{
			SkillRow->ClearWeapon();
		}
	}
}

void UMKHInventoryEquipmentWidget::HandleQuickSlotRelocated(UInventoryItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	ClearItem(FGameplayTag(), Item->GetItemID());
	if (Item->IsQuickSlotted())
	{
		ShowItem(Item);
	}
}

void UMKHInventoryEquipmentWidget::SetDashboard(UInventoryDashboardWidget* InDashboard)
{
	Super::SetDashboard(InDashboard);
	for (UMKHInventoryWeaponSkillsWidget* SkillRow : SkillRows)
	{
		if (IsValid(SkillRow))
		{
			SkillRow->SetDashboard(InDashboard);
		}
	}
}

// ============================================================
// Protected / Internal Logic (drag & drop)
// ============================================================

void UMKHInventoryEquipmentWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
	SetDropHighlight(IsValid(GetEquippableItem(InOperation)));
}

bool UMKHInventoryEquipmentWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetDropHighlight(false);

	UInventoryDashboardWidget* OwningDashboard = GetDashboard();
	UInventoryItem* Item = GetEquippableItem(InOperation);
	if (!IsValid(OwningDashboard) || !IsValid(Item))
	{
		return false;
	}

	if (Item->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Armor))
	{
		OwningDashboard->RequestUseItem(Item->GetItemID(), 1);
	}
	else
	{
		OwningDashboard->RequestAddToQuickSlot(Item->GetItemID(), OwningDashboard->FindQuickSlotForItem(Item));
	}
	return true;
}

// ============================================================
// Private helpers
// ============================================================

UInventoryItem* UMKHInventoryEquipmentWidget::GetEquippableItem(const UDragDropOperation* Operation)
{
	UInventoryItem* Item = UMKHInventoryEntryDragOperation::GetDraggedItem(Operation);
	const bool bEquippable = IsValid(Item) && !Item->IsEquipped()
		&& Item->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Equipment);
	return bEquippable ? Item : nullptr;
}

void UMKHInventoryEquipmentWidget::RefreshSkinPortrait() const
{
	if (!IsValid(SkinPortrait))
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UMKHSkinSubsystem* SkinSubsystem = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHSkinSubsystem>() : nullptr;
	const FMKHSkinDefinition* Skin = IsValid(SkinSubsystem) ? SkinSubsystem->FindSkin(SkinSubsystem->GetSelectedSkinId()) : nullptr;
	if (Skin == nullptr || !IsValid(Skin->Portrait))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHInventoryEquipmentWidget: no portrait for the selected skin."));
		return;
	}
	SkinPortrait->SetBrushFromTexture(Skin->Portrait);
}
