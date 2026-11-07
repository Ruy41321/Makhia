// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "MKHSplitStackDragOperation.generated.h"

/**
 * Drag operation carrying part of an inventory stack after a Split. The split is only virtual while dragging:
 * dropping it on a consumable quick slot asks the server to create the new stack there
 * (UInventoryDashboardWidget::TryDropSplitOnQuickSlot); released outside the inventory window it drops the split
 * quantity on the ground (UInventoryDashboardWidget::NativeOnDragCancelled); anywhere else (bag included) it changes
 * nothing, so the quantity simply stays in the original stack. Payload is a preview UInventoryItem with the split quantity.
 */
UCLASS()
class MAKHIA_API UMKHSplitStackDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Sets the stack being split and the quantity carried by the drag.
	 *
	 * @param InItemID   Source inventory stack.
	 * @param InQuantity Quantity taken from it.
	 */
	void InitSplit(int64 InItemID, int32 InQuantity);

	/** Gets the source inventory stack. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Split")
	int64 GetItemID() const { return ItemID; }

	/** Gets the quantity carried by the drag. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Split")
	int32 GetQuantity() const { return Quantity; }

private:
	// ============================================================
	// Properties
	// ============================================================

	/** Source inventory stack. */
	UPROPERTY()
	int64 ItemID = 0;

	/** Quantity carried by the drag. */
	UPROPERTY()
	int32 Quantity = 0;
};
