// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/HUDOverlay/HUDOverlayWidget.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "UI/HUD/HUDOverlay/MKHInventoryHintWidget.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHConsumablesHUDWidget.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHWeaponHUDWidget.h"

// ============================================================
// Lifecycle
// ============================================================

void UHUDOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		ScoreboardStyleHandle = UserSettings->OnScoreboardStyleChanged.AddUObject(this, &UHUDOverlayWidget::ApplyScoreboardStyle);
	}
	ApplyScoreboardStyle();
}

void UHUDOverlayWidget::NativeDestruct()
{
	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->OnScoreboardStyleChanged.Remove(ScoreboardStyleHandle);
	}

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UHUDOverlayWidget::OnItemQuickSlotted(UInventoryItem* NewItem)
{
	if (UMKHQuickSlotContainerWidget* Container = GetContainerForItem(NewItem); IsValid(Container))
	{
		Container->AddItem(NewItem);
	}
}

void UHUDOverlayWidget::OnItemChanged(UInventoryItem* NewItem)
{
	if (UMKHQuickSlotContainerWidget* Container = GetContainerForItem(NewItem); IsValid(Container))
	{
		Container->SyncItem(NewItem);
	}
}

void UHUDOverlayWidget::OnItemRemoved(const int64 RemovedItemID)
{
	const bool bRemoved = IsValid(ConsumablesHUD) && ConsumablesHUD->RemoveItemByID(RemovedItemID);
	if (!bRemoved && IsValid(WeaponHUD))
	{
		WeaponHUD->RemoveItemByID(RemovedItemID);
	}
}

void UHUDOverlayWidget::OnQuickSlotActivated(const FGameplayTag& QuickSlotTag)
{
	if (QuickSlotTag.MatchesTag(MKHGameplayTags::Input::Inventory))
	{
		if (IsValid(InventoryHint))
		{
			InventoryHint->PlayActivationFlash();
		}
	}
	else if (QuickSlotTag.MatchesTag(MKHGameplayTags::Input::ConsumableQuickSlotCategory))
	{
		if (IsValid(ConsumablesHUD))
		{
			ConsumablesHUD->PlayActivationFlash(QuickSlotTag);
		}
	}
	else if (IsValid(WeaponHUD))
	{
		// Weapon quick slots and weapon skills.
		WeaponHUD->PlayActivationFlash(QuickSlotTag);
	}
}

void UHUDOverlayWidget::OnActiveWeaponSlotChanged(const FGameplayTag& ActiveWeaponSlotTag)
{
	if (IsValid(WeaponHUD))
	{
		WeaponHUD->SetActiveWeaponSlot(ActiveWeaponSlotTag);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UHUDOverlayWidget::ApplyScoreboardStyle()
{
	const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	const bool bEpigraph = IsValid(UserSettings) && UserSettings->GetScoreboardStyle() == EMKHScoreboardStyle::Epigraph;

	// Both stay bound to the game state while collapsed, so a switch during a match shows up to date values.
	if (IsValid(ScoreboardPediment))
	{
		ScoreboardPediment->SetVisibility(bEpigraph ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (IsValid(ScoreboardEpigraph))
	{
		ScoreboardEpigraph->SetVisibility(bEpigraph ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UMKHQuickSlotContainerWidget* UHUDOverlayWidget::GetContainerForItem(const UInventoryItem* Item) const
{
	if (!IsValid(Item))
	{
		return nullptr;
	}

	if (Item->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Weapon))
	{
		return WeaponHUD;
	}
	return ConsumablesHUD;
}
