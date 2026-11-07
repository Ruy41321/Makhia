// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHItemPickupWidget.generated.h"

class UMKHTextBlock;
class UWidget;
struct FMKHItemEntry;

/**
 * Parent of W_ItemPickUp, the detail prompt of the focused world pickup. The Blueprint keeps the item display
 * (UpdateItemRef / UpdateUI); this class drives the hold-to-equip hint: "Hold: equip" / "Hold: quick slot" next to
 * the Interact keycap. No progress bar: the prompt hides as soon as the press collects the item.
 *
 * Driven by AMKHPickupItem (SetHoldHint when the pickup gains the focus).
 * See Documentation/PickupSystemArchitecture.md, "Hold to equip".
 */
UCLASS(Abstract)
class MAKHIA_API UMKHItemPickupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows the hold hint for an item, or hides it when the setting is off or holding does nothing for the item.
	 *
	 * @param Item The item lying on the pickup.
	 */
	UFUNCTION(BlueprintCallable, Category = "Items|Pickup")
	void SetHoldHint(const FMKHItemEntry& Item);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Starts with the hint hidden at runtime; the designer keeps its preview. */
	virtual void NativePreConstruct() override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Gets the hint label of an item.
	 *
	 * @param Item The item lying on the pickup.
	 * @return "Hold: equip" / "Hold: quick slot", or empty when holding does nothing.
	 */
	FText GetHoldHintText(const FMKHItemEntry& Item) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Row holding the hint text; collapsed when there is no hint. */
	UPROPERTY(BlueprintReadOnly, Category = "Items|Pickup", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HoldHintBox;

	/** "Hold: equip" / "Hold: quick slot". */
	UPROPERTY(BlueprintReadOnly, Category = "Items|Pickup", meta = (BindWidgetOptional))
	TObjectPtr<UMKHTextBlock> HoldHintText;
};
