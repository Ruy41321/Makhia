// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "MKHInventoryEntryDragOperation.generated.h"

class UInventoryItem;
class UMKHInventoryEntryWidget;

/**
 * Drag of a whole item out of an inventory entry (bag, equipment or quick slot area). The payload is the item;
 * the source entry hides its content while the item is dragged and shows it again when the drag ends.
 * Splits use UMKHSplitStackDragOperation instead.
 */
UCLASS()
class MAKHIA_API UMKHInventoryEntryDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Sets the dragged item and the entry it comes from.
	 *
	 * @param InSourceEntry Entry the drag started on.
	 * @param Item          Item carried by the drag (also the Payload).
	 */
	void InitEntryDrag(UMKHInventoryEntryWidget* InSourceEntry, UInventoryItem* Item);

	/**
	 * Returns the item carried by an operation when it is an entry drag.
	 *
	 * @param Operation Any drag operation.
	 * @return The dragged item, or nullptr for other operations (e.g. a split).
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	static UInventoryItem* GetDraggedItem(const UDragDropOperation* Operation);

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Hides the source entry's content while dragging. */
	virtual void Dragged_Implementation(const FPointerEvent& PointerEvent) override;

	/** Shows the source entry's content again. */
	virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;

	/** Shows the source entry's content again. */
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;

private:
	/** Shows or hides the source entry's content, if it still exists. */
	void SetSourceContentHidden(bool bHidden) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Entry the drag started on. */
	TWeakObjectPtr<UMKHInventoryEntryWidget> SourceEntry;
};
