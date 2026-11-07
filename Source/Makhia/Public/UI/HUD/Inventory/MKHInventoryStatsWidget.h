// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHInventoryStatsWidget.generated.h"

class UAbilitySystemComponent;
class UTextBlock;
struct FOnAttributeChangeData;

/**
 * Stats area of the equipment panel (W_StatsArea): the owning pawn's health ("current/max") and armor rating with
 * its damage reduction ("armor (-reduction%)"), kept up to date from the ability system.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHInventoryStatsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Follows the owning pawn's Health, MaxHealth and Armor and shows their current values. */
	virtual void NativeConstruct() override;

	/** Stops following the ability system. */
	virtual void NativeDestruct() override;

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Health or MaxHealth changed. */
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData) const;

	/** Armor changed. */
	void HandleArmorChanged(const FOnAttributeChangeData& ChangeData) const;

	/** Writes "current/max" health in HpValueText. */
	void RefreshHealth() const;

	/** Writes "armor (-reduction%)" in ArmorValueText. */
	void RefreshArmor() const;

	/** Removes the attribute listeners from the followed ability system. */
	void UnbindFromAbilitySystem();

protected:
	// ============================================================
	// Properties
	// ============================================================

	/** Health value ("current/max"). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Stats", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HpValueText;

	/** Armor value with its damage reduction. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Stats", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ArmorValueText;

private:
	/** Ability system of the owning pawn (the player's lives on the PlayerState). */
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
};
