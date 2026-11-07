// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHQuickSlotContainerWidget.h"
#include "MKHWeaponHUDWidget.generated.h"

class UMKHWeaponSkillRowWidget;

/**
 * Weapon block of the HUD (W_WeaponHUD): its W_WeaponSkillRow children, one per weapon quick slot.
 *
 * Routes weapons to the row of their quick slot tag, keeps the active weapon slot given by the HUD overlay
 * controller and marks the rows: the active one and empty ones keep the normal look, a filled row that is not
 * active is dimmed. Skill activations go to the active row only. See Documentation/HUDQuickSlots.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHWeaponHUDWidget : public UMKHQuickSlotContainerWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Skill tags flash the active row (the first row when there is none); weapon slot tags flash their row.
	 *
	 * @param QuickSlotTag  Activated weapon quick slot or skill input tag.
	 */
	virtual void PlayActivationFlash(const FGameplayTag& QuickSlotTag) override;

	/**
	 * Sets the weapon quick slot whose row is active and updates every row.
	 *
	 * @param InActiveWeaponSlotTag  Active weapon quick slot; invalid when no weapon is quick slotted.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot|WeaponHUD")
	void SetActiveWeaponSlot(const FGameplayTag& InActiveWeaponSlotTag);

	/** Active weapon quick slot, invalid when there is none. */
	UFUNCTION(BlueprintPure, Category = "QuickSlot|WeaponHUD")
	FGameplayTag GetActiveWeaponSlot() const { return ActiveWeaponSlotTag; }

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Caches the rows before the slots are gathered. */
	virtual void NativeOnInitialized() override;

	/** Caches the rows in the designer. */
	virtual void NativePreConstruct() override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** The weapon slot of every row. */
	virtual void GatherQuickSlots(TArray<UMKHQuickSlotWidget*>& OutSlots) const override;

	/** Shows the weapon through its row (weapon + skills), then refreshes the row states. */
	virtual void AssignSlotItem(UMKHQuickSlotWidget& QuickSlot, UInventoryItem* Item) override;

	/** Empties the row of the slot, then refreshes the row states. */
	virtual void ClearSlotItem(UMKHQuickSlotWidget& QuickSlot) override;

private:
	/** Finds the W_WeaponSkillRow children, in tree order. */
	void CacheRows();

	/** Row whose weapon slot is the given slot, or null. */
	UMKHWeaponSkillRowWidget* FindRowBySlot(const UMKHQuickSlotWidget& QuickSlot) const;

	/** Row of the active weapon slot, or the first row when there is none. */
	UMKHWeaponSkillRowWidget* GetSkillFlashRow() const;

	/** Applies the active/inactive look of every row from ActiveWeaponSlotTag and their content. */
	void RefreshRowStates() const;

	// ============================================================
	// Properties
	// ============================================================

	/** W_WeaponSkillRow children, in tree order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMKHWeaponSkillRowWidget>> Rows;

	/** Active weapon quick slot from the HUD overlay controller. */
	FGameplayTag ActiveWeaponSlotTag;
};
