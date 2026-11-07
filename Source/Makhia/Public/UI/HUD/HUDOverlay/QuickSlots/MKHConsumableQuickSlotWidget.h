// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHQuickSlotWidget.h"
#include "MKHConsumableQuickSlotWidget.generated.h"

/**
 * Consumable quick slot (W_ConsumableQuickSlotHUD): item icon on the rarity background, stack counter and
 * the consumable's cooldown.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHConsumableQuickSlotWidget : public UMKHQuickSlotWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** The item's inventory icon. */
	virtual UTexture2D* ResolveSlotIcon() const override;

	/** The cooldown tag granted when the consumable is used. */
	virtual FGameplayTag ResolveCooldownTag() const override;

	/** The stack size. */
	virtual int32 ResolveDisplayedQuantity() const override;
};
