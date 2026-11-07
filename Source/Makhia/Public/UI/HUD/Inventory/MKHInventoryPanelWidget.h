// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "MKHInventoryPanelWidget.generated.h"

class UInventoryDashboardWidget;
class UMKHDropHighlightWidget;
class UMKHInventoryEntryWidget;

/**
 * Base of the inventory dashboard's panels (bag, equipment, quick slot area): knows the dashboard, the entries of
 * its own widget tree (nested user widgets excluded) and its drop highlight. The Blueprints own only the layout.
 * Full model: Documentation/InventoryDashboard.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventoryPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Sets the dashboard the panel and its entries send their requests to.
	 *
	 * @param InDashboard The owning dashboard.
	 */
	virtual void SetDashboard(UInventoryDashboardWidget* InDashboard);

	/**
	 * Shows or hides the panel's drop highlight.
	 *
	 * @param bActive True while the dragged item can be dropped on the panel.
	 */
	void SetDropHighlight(bool bActive) const;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Gathers the entries of the widget tree. */
	virtual void NativeOnInitialized() override;

	/** Removes the highlight when the drag leaves the panel. */
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Dashboard set by SetDashboard, or nullptr. */
	UInventoryDashboardWidget* GetDashboard() const { return Dashboard.Get(); }

	/** Entries of the panel's own widget tree, in tree order. */
	const TArray<TObjectPtr<UMKHInventoryEntryWidget>>& GetEntries() const { return Entries; }

	/**
	 * Finds the entry standing for a slot.
	 *
	 * @param InSlotTag Equipment slot or quick slot (exact match).
	 * @return The entry, or nullptr.
	 */
	UMKHInventoryEntryWidget* FindEntryBySlotTag(const FGameplayTag& InSlotTag) const;

	/**
	 * Finds the entry currently showing an item.
	 *
	 * @param ItemID Item to look for.
	 * @return The entry, or nullptr.
	 */
	UMKHInventoryEntryWidget* FindEntryShowingItem(int64 ItemID) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Highlight shown while a drop on the panel is possible (W_FadedVFX). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Panel", meta = (BindWidgetOptional))
	TObjectPtr<UMKHDropHighlightWidget> DropHighlight;

private:
	/** Entries of the widget tree, gathered once. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMKHInventoryEntryWidget>> Entries;

	/** Dashboard receiving the requests. */
	TWeakObjectPtr<UInventoryDashboardWidget> Dashboard;
};
