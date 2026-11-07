// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/Inventory/MKHInventoryPanelWidget.h"
#include "MKHInventoryEquipmentWidget.generated.h"

class UImage;
class UInventoryItem;
class UMKHInventoryWeaponSkillsWidget;

/**
 * An equipment panel. Two instances share this class: W_Equipment (skin portrait, armor slots with
 * SlotTag = Equipment.Slot.Armor.*, stats area) and W_WeaponLoadout (one row per weapon quick slot, with
 * SlotTag = Input.Ability.QuickSlot.Weapon.*, and the skills of the weapon). Dropping equipment on either equips an
 * armor or quick-slots a weapon. Requests of a slot the panel does not show do nothing.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventoryEquipmentWidget : public UMKHInventoryPanelWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows an item in its entry: the quick slot's entry for a quick-slotted weapon, the equipment slot's otherwise.
	 *
	 * @param Item The equipped armor or quick-slotted weapon.
	 */
	void ShowItem(UInventoryItem* Item);

	/**
	 * Empties the entries showing an item: by ItemID when given, else every entry whose item belongs to OffSlotTag.
	 *
	 * @param OffSlotTag Equipment slot that was emptied (used when ItemID is 0).
	 * @param ItemID     Item that left the panel, or 0.
	 */
	void ClearItem(const FGameplayTag& OffSlotTag, int64 ItemID);

	/**
	 * A weapon entered, left or changed quick slot: it leaves its old entry and shows in its new one.
	 *
	 * @param Item The relocated weapon.
	 */
	void HandleQuickSlotRelocated(UInventoryItem* Item);

	/** Also gives the dashboard to the skill rows. */
	virtual void SetDashboard(UInventoryDashboardWidget* InDashboard) override;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Finds the skill rows of the widget tree. */
	virtual void NativeOnInitialized() override;

	/** Shows the portrait of the skin chosen by the player. */
	virtual void NativeConstruct() override;

	// ============================================================
	// Protected / Internal Logic (drag & drop)
	// ============================================================

	/** Highlights the panel for equipment that is not equipped yet. */
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Equips a dropped armor, quick-slots a dropped weapon. */
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	/**
	 * Returns the item of an entry drag the panel can take: equipment that is not equipped.
	 *
	 * @param Operation The operation over the panel.
	 * @return The item, or nullptr.
	 */
	static UInventoryItem* GetEquippableItem(const UDragDropOperation* Operation);

	/** Shows the portrait of the selected skin in SkinPortrait. */
	void RefreshSkinPortrait() const;

protected:
	// ============================================================
	// Properties
	// ============================================================

	/** Portrait of the player's skin (W_Equipment only). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Equipment", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SkinPortrait;

private:
	/** Skill rows of the widget tree. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMKHInventoryWeaponSkillsWidget>> SkillRows;
};
