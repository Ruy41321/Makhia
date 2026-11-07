// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHQuickSlotWidget.h"
#include "MKHWeaponQuickSlotWidget.generated.h"

/** Weapon quick slot (W_WeaponQuickSlotHUD): the weapon's HUD icon on the rarity background. */
UCLASS(Abstract)
class MAKHIA_API UMKHWeaponQuickSlotWidget : public UMKHQuickSlotWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** The weapon's HUD icon. */
	virtual UTexture2D* ResolveSlotIcon() const override;
};
