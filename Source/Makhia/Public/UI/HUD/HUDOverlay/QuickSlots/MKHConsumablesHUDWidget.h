// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHQuickSlotContainerWidget.h"
#include "MKHConsumablesHUDWidget.generated.h"

/**
 * Consumable quick slots of the HUD (W_ConsumablesHUD): its W_ConsumableQuickSlotHUD children, each with its
 * quick slot tag and input action set on the instance. The default container routing is all it needs.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHConsumablesHUDWidget : public UMKHQuickSlotContainerWidget
{
	GENERATED_BODY()
};
