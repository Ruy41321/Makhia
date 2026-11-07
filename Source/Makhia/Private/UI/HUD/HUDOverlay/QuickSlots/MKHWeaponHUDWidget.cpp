// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/QuickSlots/MKHWeaponHUDWidget.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Blueprint/WidgetTree.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHWeaponQuickSlotWidget.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHWeaponSkillRowWidget.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHWeaponHUDWidget::NativeOnInitialized()
{
	CacheRows();

	Super::NativeOnInitialized();

	RefreshRowStates();
}

void UMKHWeaponHUDWidget::NativePreConstruct()
{
	CacheRows();

	Super::NativePreConstruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHWeaponHUDWidget::PlayActivationFlash(const FGameplayTag& QuickSlotTag)
{
	if (QuickSlotTag.MatchesTag(MKHGameplayTags::Input::Skill))
	{
		if (UMKHWeaponSkillRowWidget* Row = GetSkillFlashRow(); IsValid(Row))
		{
			Row->PlayActivationFlash(QuickSlotTag);
		}
		return;
	}

	for (UMKHWeaponSkillRowWidget* Row : Rows)
	{
		if (IsValid(Row) && Row->GetQuickSlotTag().MatchesTagExact(QuickSlotTag))
		{
			Row->PlayActivationFlash(QuickSlotTag);
		}
	}
}

void UMKHWeaponHUDWidget::SetActiveWeaponSlot(const FGameplayTag& InActiveWeaponSlotTag)
{
	ActiveWeaponSlotTag = InActiveWeaponSlotTag;
	RefreshRowStates();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHWeaponHUDWidget::GatherQuickSlots(TArray<UMKHQuickSlotWidget*>& OutSlots) const
{
	for (UMKHWeaponSkillRowWidget* Row : Rows)
	{
		if (!IsValid(Row))
		{
			continue;
		}
		if (UMKHWeaponQuickSlotWidget* WeaponSlot = Row->GetWeaponSlot(); IsValid(WeaponSlot))
		{
			OutSlots.Add(WeaponSlot);
		}
	}
}

void UMKHWeaponHUDWidget::AssignSlotItem(UMKHQuickSlotWidget& QuickSlot, UInventoryItem* Item)
{
	if (UMKHWeaponSkillRowWidget* Row = FindRowBySlot(QuickSlot); IsValid(Row))
	{
		Row->AddItem(Item);
	}
	RefreshRowStates();
}

void UMKHWeaponHUDWidget::ClearSlotItem(UMKHQuickSlotWidget& QuickSlot)
{
	if (UMKHWeaponSkillRowWidget* Row = FindRowBySlot(QuickSlot); IsValid(Row))
	{
		Row->RemoveItemByID(QuickSlot.GetSlotItemID());
	}
	RefreshRowStates();
}

void UMKHWeaponHUDWidget::CacheRows()
{
	if (!Rows.IsEmpty() || !IsValid(WidgetTree))
	{
		return;
	}

	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (UMKHWeaponSkillRowWidget* Row = Cast<UMKHWeaponSkillRowWidget>(Widget))
		{
			Rows.Add(Row);
		}
	});
}

UMKHWeaponSkillRowWidget* UMKHWeaponHUDWidget::FindRowBySlot(const UMKHQuickSlotWidget& QuickSlot) const
{
	for (UMKHWeaponSkillRowWidget* Row : Rows)
	{
		if (IsValid(Row) && Row->GetWeaponSlot() == &QuickSlot)
		{
			return Row;
		}
	}
	return nullptr;
}

UMKHWeaponSkillRowWidget* UMKHWeaponHUDWidget::GetSkillFlashRow() const
{
	for (UMKHWeaponSkillRowWidget* Row : Rows)
	{
		if (IsValid(Row) && ActiveWeaponSlotTag.IsValid() && Row->GetQuickSlotTag().MatchesTagExact(ActiveWeaponSlotTag))
		{
			return Row;
		}
	}
	return Rows.IsEmpty() ? nullptr : Rows[0].Get();
}

void UMKHWeaponHUDWidget::RefreshRowStates() const
{
	for (UMKHWeaponSkillRowWidget* Row : Rows)
	{
		if (!IsValid(Row))
		{
			continue;
		}

		// Only a filled row that is not the active one is dimmed; with no active slot every row looks normal.
		const bool bActive = !ActiveWeaponSlotTag.IsValid()
			|| !Row->HasWeapon()
			|| Row->GetQuickSlotTag().MatchesTagExact(ActiveWeaponSlotTag);
		Row->SetRowActive(bActive);
	}
}
