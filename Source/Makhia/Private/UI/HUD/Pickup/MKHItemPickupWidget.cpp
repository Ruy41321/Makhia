// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Pickup/MKHItemPickupWidget.h"

#include "GameUserSettings/MKHGameUserSettings.h"
#include "Inventory/MKHItemEntry.h"
#include "Inventory/Pickup/MKHPickupAutoEquip.h"
#include "UI/Text/MKHTextBlock.h"

#define LOCTEXT_NAMESPACE "MKHPickup"

void UMKHItemPickupWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!IsDesignTime() && IsValid(HoldHintBox))
	{
		HoldHintBox->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMKHItemPickupWidget::SetHoldHint(const FMKHItemEntry& Item)
{
	const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	const bool bHoldEnabled = IsValid(UserSettings) && UserSettings->IsHoldToEquipPickupEnabled();
	const FText HintText = bHoldEnabled ? GetHoldHintText(Item) : FText::GetEmpty();

	if (IsValid(HoldHintText))
	{
		HoldHintText->SetText(HintText);
	}
	if (IsValid(HoldHintBox))
	{
		HoldHintBox->SetVisibility(HintText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

FText UMKHItemPickupWidget::GetHoldHintText(const FMKHItemEntry& Item) const
{
	switch (FMKHPickupAutoEquip::GetAction(this, Item.ItemTag))
	{
	case EMKHPickupAutoEquipAction::Equip:     return LOCTEXT("HoldToEquip", "Hold: equip");
	case EMKHPickupAutoEquipAction::QuickSlot: return LOCTEXT("HoldToQuickSlot", "Hold: quick slot");
	default:                                   return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
