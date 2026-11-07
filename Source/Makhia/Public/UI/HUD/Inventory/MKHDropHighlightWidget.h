// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHDropHighlightWidget.generated.h"

class UTextBlock;

/**
 * Overlay an inventory panel or entry shows while something it accepts is dragged over it (W_FadedVFX), with an
 * optional hint ("Drop to Equip"). The Blueprint owns the look; see Documentation/InventoryDashboard.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHDropHighlightWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows or hides the highlight.
	 *
	 * @param bActive True while a drag the host accepts is over it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void SetHighlightActive(bool bActive);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Applies Text and bShowText to FadedText, also in the designer. */
	virtual void NativePreConstruct() override;

	// ============================================================
	// Properties
	// ============================================================

	/** Part shown while highlighted (collapsed otherwise). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (BindWidget))
	TObjectPtr<UWidget> FadeVFX;

	/** Hint printed over the highlight. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FadedText;

	/** Whether the hint is shown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|DragDrop")
	bool bShowText = false;

	/** Hint shown when bShowText is set (e.g. "Drop to Equip"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|DragDrop")
	FText Text;
};
