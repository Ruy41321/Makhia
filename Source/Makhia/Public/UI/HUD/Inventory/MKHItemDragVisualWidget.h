// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHItemDragVisualWidget.generated.h"

class UImage;
class UInventoryItem;

/**
 * Item icon over its rarity square, shown under the cursor while an item or a split is dragged
 * (W_EntryDragVisual). Created by UInventoryDashboardWidget::CreateItemDragVisual.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHItemDragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows an item.
	 *
	 * @param InItem The dragged item (or the preview of a split).
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void SetItem(UInventoryItem* InItem);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Renders the item set before construction. */
	virtual void NativeConstruct() override;

private:
	/** Applies the item's icon and rarity square (MI_Square). */
	void RefreshVisual();

protected:
	// ============================================================
	// Properties
	// ============================================================

	/** Item icon. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (BindWidget))
	TObjectPtr<UImage> SlotIcon;

	/** Rarity background. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SlotBg;

	/** Item shown. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|DragDrop")
	TObjectPtr<UInventoryItem> InventoryItem;
};
