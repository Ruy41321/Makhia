// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "MKHSkillDragOperation.generated.h"

class UMKHInventoryWeaponSkillsWidget;
class UMKHSkillQuickSlotWidget;

/**
 * Drag of a skill between the skill slots of a weapon row. It carries no item data: the row it belongs to rejects any
 * other row's drag and clears the look when the drag ends. See Documentation/InventoryDashboard.md.
 */
UCLASS()
class MAKHIA_API UMKHSkillDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Sets where the drag comes from.
	 *
	 * @param InRow        Row of the slot.
	 * @param InSourceSlot Slot the drag started on.
	 */
	void InitSkillDrag(UMKHInventoryWeaponSkillsWidget* InRow, UMKHSkillQuickSlotWidget* InSourceSlot);

	/** Row the drag belongs to, or nullptr. */
	UMKHInventoryWeaponSkillsWidget* GetRow() const { return Row.Get(); }

	/** Slot the drag started on, or nullptr. */
	UMKHSkillQuickSlotWidget* GetSourceSlot() const { return SourceSlot.Get(); }

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Clears the row's look. */
	virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;

	/** Clears the row's look. */
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;

private:
	// ============================================================
	// Properties
	// ============================================================

	/** Row the drag belongs to. */
	TWeakObjectPtr<UMKHInventoryWeaponSkillsWidget> Row;

	/** Slot the drag started on. */
	TWeakObjectPtr<UMKHSkillQuickSlotWidget> SourceSlot;
};
