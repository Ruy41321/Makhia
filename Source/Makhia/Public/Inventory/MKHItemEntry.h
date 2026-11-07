// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Equipment/EquipmentTypes.h"
#include "MKHItemEntry.generated.h"

/**
 * Identity of one concrete item instance: the single source of truth passed between
 * world pickups, inventory, equipment and UI. Everything static (name, icon, cooldown,
 * pickup mesh, ...) is read from FMasterItemDefinition through UMKHDataSubsystem.
 *
 * Only the server creates entries (UMKHItemSubsystem); Blueprints can read them but not forge them.
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHItemEntry
{
	GENERATED_BODY()

	/** Gameplay tag identifying the item definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGameplayTag ItemTag = FGameplayTag();

	/** Globally unique identifier of this item instance; 0 means "no item". */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int64 ItemID = 0;

	/** Current stack quantity (always 1 for equipment). */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Quantity = 0;

	/** Rarity tier of this instance (rolled or pre-defined by the item definition). */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGameplayTag RarityTag = FGameplayTag();

	/** Rolled passive stats and abilities of this instance (empty for non-equipment items). */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FEquipmentEffectPackage EffectPackage = FEquipmentEffectPackage();

	/** Returns whether this entry represents an actual item. */
	bool IsValid() const
	{
		return ItemID != 0;
	}
};
