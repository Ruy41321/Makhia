// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/MKHSystemWidget.h"
#include "MKHQuickSlotContainerWidget.generated.h"

class UInventoryItem;
class UMKHQuickSlotWidget;

/**
 * Base of the HUD widgets grouping quick slots (W_ConsumablesHUD, W_WeaponSkillRow, W_WeaponHUD).
 *
 * Collects its slots once and routes items to them by quick slot tag: an item always leaves the slot it was in
 * before reaching its new one, updates are copied into the item already shown, and activations flash the slot
 * with the activated tag. Subclasses change what a slot is (GatherQuickSlots) and what showing an item in it
 * means (AssignSlotItem / ClearSlotItem). See Documentation/HUDQuickSlots.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHQuickSlotContainerWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Takes an item whose quick slot changed: removes it from the slot it was in, then shows it in the slot of its
	 * quick slot tag when it is still quick slotted.
	 *
	 * @param Item  UI view of the item.
	 * @return True when a slot now shows the item.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot|Container")
	bool AddItem(UInventoryItem* Item);

	/**
	 * Empties the slot showing an item.
	 *
	 * @param ItemID  Item to remove.
	 * @return True when a slot was showing it.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot|Container")
	bool RemoveItemByID(int64 ItemID);

	/**
	 * Applies an item update: copied into the item already shown (its slot refreshes on its own), or added when
	 * no slot shows it yet.
	 *
	 * @param Item  Updated UI view of the item.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot|Container")
	void SyncItem(UInventoryItem* Item);

	/**
	 * Flashes the slot triggered by an activated tag. Default: the slots whose quick slot tag is exactly the tag.
	 *
	 * @param QuickSlotTag  Activated input / quick slot tag.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot|Container")
	virtual void PlayActivationFlash(const FGameplayTag& QuickSlotTag);

	/** Slot whose quick slot tag is exactly the given tag, or null. */
	UMKHQuickSlotWidget* FindSlotByTag(const FGameplayTag& QuickSlotTag) const;

	/** Slot showing the given item, or null. */
	UMKHQuickSlotWidget* FindSlotByItemID(int64 ItemID) const;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Collects the slots at runtime. */
	virtual void NativeOnInitialized() override;

	/** Collects the slots in the designer (NativeOnInitialized does not run there). */
	virtual void NativePreConstruct() override;

	// ============================================================
	// Protected / Internal Logic (subclass hooks)
	// ============================================================

	/**
	 * Fills the list of slots items are routed to. Default: every UMKHQuickSlotWidget of this widget tree, in tree
	 * order (nested user widgets are not searched).
	 *
	 * @param OutSlots  Receives the slots.
	 */
	virtual void GatherQuickSlots(TArray<UMKHQuickSlotWidget*>& OutSlots) const;

	/** Shows an item in one of the slots. Default: UMKHQuickSlotWidget::SetSlotItem. */
	virtual void AssignSlotItem(UMKHQuickSlotWidget& QuickSlot, UInventoryItem* Item);

	/** Empties one of the slots. Default: UMKHQuickSlotWidget::ResetSlot. */
	virtual void ClearSlotItem(UMKHQuickSlotWidget& QuickSlot);

	/** Collects the slots when not done yet. */
	void EnsureQuickSlotsGathered();

	// ============================================================
	// Properties
	// ============================================================

	/** Slots items are routed to, from GatherQuickSlots. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "QuickSlot|Container")
	TArray<TObjectPtr<UMKHQuickSlotWidget>> QuickSlots;
};
