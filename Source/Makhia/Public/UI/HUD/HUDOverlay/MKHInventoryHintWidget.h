// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHInventoryHintWidget.generated.h"

class UImage;
class UMKHKeyHintWidget;

/**
 * On-screen inventory hint (W_InventoryHint): the inventory icon over the keycap of IA_ToggleInventory.
 * It flashes when the inventory opens (UHUDOverlayWidget::OnQuickSlotActivated with Input.Inventory).
 * The icon and the keycap's action are set in the designer; the Blueprint owns the layout and the flash.
 * See Documentation/HUDQuickSlots.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventoryHintWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Plays the activation flash (BP_PlayActivationFlash). */
	UFUNCTION(BlueprintCallable, Category = "HUD|InventoryHint")
	void PlayActivationFlash();

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Blueprint hook playing the hint's flash (W_FlashVFX). */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|InventoryHint")
	void BP_PlayActivationFlash();

	// ============================================================
	// Properties
	// ============================================================

	/** Optional inventory icon. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> SlotIcon;

	/** Optional keycap (W_KeyCap) showing the key of IA_ToggleInventory. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHKeyHintWidget> KeyCap;
};
