// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "MKHInventoryWeaponSkillsWidget.generated.h"

class UDragDropOperation;
class UInventoryDashboardWidget;
class UInventoryItem;
class UMKHGamepadHintWidget;
class UMKHSkillDragOperation;
class UMKHSkillQuickSlotWidget;

/**
 * The three skills of a weapon row in the inventory (W_InventoryWeaponSkills): the same W_SkillSlotHUD of the HUD,
 * with icon, key and cooldown. The player reorders them here: mouse drag, or pad A (take), arrows, A (place), B (cancel).
 * The slots forward their input to this row; requests go to the dashboard. See Documentation/InventoryDashboard.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventoryWeaponSkillsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows the skills of a weapon.
	 *
	 * @param Weapon The weapon in the row's quick slot.
	 */
	void SetWeapon(UInventoryItem* Weapon);

	/** Empties the three skill slots. */
	void ClearWeapon();

	/** Weapon quick slot of the row. */
	const FGameplayTag& GetQuickSlotTag() const { return QuickSlotTag; }

	/** ItemID of the weapon shown, or 0 when empty. */
	int64 GetWeaponItemID() const;

	/**
	 * Sets the dashboard receiving the skill swap requests.
	 *
	 * @param InDashboard The owning dashboard.
	 */
	void SetDashboard(UInventoryDashboardWidget* InDashboard);

	// --- Reorder, called by the slots and the drag ---

	/** A slot was pressed with the mouse: closes the action menu, like the entries. */
	void HandleSlotPressed() const;

	/**
	 * Starts a drag from a filled slot.
	 *
	 * @param SourceSlot Slot the drag starts on.
	 * @return The operation, or nullptr when the slot has no skill.
	 */
	UMKHSkillDragOperation* BeginSkillDrag(UMKHSkillQuickSlotWidget& SourceSlot);

	/** The drag ended, dropped or cancelled: the slots show their own content again. */
	void EndSkillDrag();

	/**
	 * A drag entered a slot: when it comes from this row, shows the outcome of a drop.
	 *
	 * @param TargetSlot Slot entered.
	 * @param Operation  The drag.
	 */
	void HandleSkillDragEnter(UMKHSkillQuickSlotWidget& TargetSlot, const UDragDropOperation* Operation);

	/**
	 * A drag left a slot: removes the outcome shown.
	 *
	 * @param TargetSlot Slot left.
	 * @param Operation  The drag.
	 */
	void HandleSkillDragLeave(UMKHSkillQuickSlotWidget& TargetSlot, const UDragDropOperation* Operation);

	/**
	 * A drag was dropped on a slot: moves the skill when the drag comes from this row.
	 *
	 * @param TargetSlot Slot dropped on.
	 * @param Operation  The drag.
	 * @return True when the drop was this row's.
	 */
	bool HandleSkillDrop(const UMKHSkillQuickSlotWidget& TargetSlot, const UDragDropOperation* Operation);

	/**
	 * Pad A on a slot: takes its skill, places the held one here, or puts it back on its own slot.
	 *
	 * @param FocusedSlot Slot with the focus.
	 */
	void HandlePadAccept(UMKHSkillQuickSlotWidget& FocusedSlot);

	/**
	 * Pad B: puts the held skill back.
	 *
	 * @return True when a skill was held (the dashboard must not close).
	 */
	bool HandlePadBack();

	/** The focus entered or left a slot: refreshes the look; on the next tick a hold ends when it left the row. */
	void HandleSlotFocusChanged();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Finds the skill slots of the tree, numbers them (SkillIndex 0, 1, 2 in tree order) and turns on their reorder. */
	virtual void NativeOnInitialized() override;

	// ============================================================
	// Properties
	// ============================================================

	/** Weapon quick slot of the row (Input.Ability.QuickSlot.Weapon.*). Set on the instance in W_WeaponLoadout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|WeaponSkills", meta = (Categories = "Input.Ability.QuickSlot.Weapon"))
	FGameplayTag QuickSlotTag;

	/** Pad hint of A: take the skill, or place it during a hold. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|WeaponSkills", meta = (BindWidgetOptional))
	TObjectPtr<UMKHGamepadHintWidget> AcceptHint;

	/** Pad hint of B during a hold. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|WeaponSkills", meta = (BindWidgetOptional))
	TObjectPtr<UMKHGamepadHintWidget> CancelHint;

private:
	// ============================================================
	// Private helpers
	// ============================================================

	/** Returns the row's source slot of a drag, or nullptr when the drag is another row's or not a skill drag. */
	UMKHSkillQuickSlotWidget* GetOwnDragSource(const UDragDropOperation* Operation) const;

	/** Lets the mouse reach the slots through the row and its parents, which are hit test invisible by design. */
	void OpenHitTestPathToSlots();

	/** Sends the swap request of the row's weapon. */
	void RequestSwap(const UMKHSkillQuickSlotWidget& From, const UMKHSkillQuickSlotWidget& To) const;

	/** Applies the pad look to every slot: highlight on the focus and the hold, lift on the hold. */
	void RefreshPadLook() const;

	/**
	 * Shows the pad hints of the focused slot: take on a filled slot; place and cancel during a hold.
	 *
	 * @param bFocusedSlotFilled True when the pad focus is on a slot holding a skill.
	 */
	void RefreshPadHints(bool bFocusedSlotFilled) const;

	/** Ends a hold when the focus is on no slot of the row. */
	void EndHoldIfFocusLeft();

	// ============================================================
	// Properties
	// ============================================================

	/** Dashboard receiving the swap requests. */
	TWeakObjectPtr<UInventoryDashboardWidget> Dashboard;

	/** Slot whose skill the pad holds, or nullptr. */
	TWeakObjectPtr<UMKHSkillQuickSlotWidget> HeldSlot;

	/** Skill slots of the tree, in tree order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMKHSkillQuickSlotWidget>> SkillSlots;

	/** Opacity of the icon a drop would send back to the drag's source slot. */
	static constexpr float PreviewDisplacedOpacity = 0.45f;
};
