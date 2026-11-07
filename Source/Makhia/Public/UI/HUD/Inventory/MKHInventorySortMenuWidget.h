// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/MKHBagSorting.h"
#include "MKHInventorySortMenuWidget.generated.h"

class UButton;
class UInventoryDashboardWidget;
class UTextBlock;

/**
 * Menu of the bag sort criteria (W_BagSortMenu), opened by the sort button or gamepad Y through
 * UInventoryDashboardWidget::OpenSortMenu. It covers the screen: a click outside its buttons closes it.
 * Model: Documentation/InventoryDashboard.md, "Bag sorting".
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventorySortMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Sets what the menu needs; call before adding it to the viewport.
	 *
	 * @param InDashboard Dashboard that receives the choice.
	 * @param InAnchor    Widget the menu stands next to.
	 */
	void InitSortMenu(UInventoryDashboardWidget* InDashboard, const UWidget* InAnchor);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Binds the buttons. */
	virtual void NativeOnInitialized() override;

	/** Places the menu, writes the labels and marks the criterion in use. */
	virtual void NativeConstruct() override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Esc, gamepad B and gamepad Y close the menu. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** A click that no button took closes the menu. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	/**
	 * Sends a criterion to the dashboard, which also closes the menu.
	 *
	 * @param SortMode The chosen criterion.
	 */
	void ChooseSortMode(EMKHBagSortMode SortMode) const;

	/** Closes the menu through the dashboard. */
	void CloseSelf() const;

	UFUNCTION()
	void HandleCategoryClicked();

	UFUNCTION()
	void HandleRarityAscendingClicked();

	UFUNCTION()
	void HandleRarityDescendingClicked();

protected:
	// ============================================================
	// Properties
	// ============================================================

	/** Box placed next to the anchor (a Canvas Panel slot). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UWidget> SortBox;

	/** Sorts by category. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UButton> CategoryButton;

	/** Label of CategoryButton. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UTextBlock> CategoryText;

	/** Mark of CategoryButton, shown while its criterion is in use. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UWidget> CategoryMark;

	/** Sorts by rarity from the lowest. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UButton> RarityAscendingButton;

	/** Label of RarityAscendingButton. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UTextBlock> RarityAscendingText;

	/** Mark of RarityAscendingButton, shown while its criterion is in use. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UWidget> RarityAscendingMark;

	/** Sorts by rarity from the highest. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UButton> RarityDescendingButton;

	/** Label of RarityDescendingButton. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UTextBlock> RarityDescendingText;

	/** Mark of RarityDescendingButton, shown while its criterion is in use. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidget))
	TObjectPtr<UWidget> RarityDescendingMark;

private:
	/** Dashboard that receives the choice. */
	TWeakObjectPtr<UInventoryDashboardWidget> Dashboard;

	/** Widget the menu stands next to. */
	TWeakObjectPtr<const UWidget> Anchor;
};
