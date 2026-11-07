// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inventory/MKHItemRarityRule.h"
#include "MKHStartingItem.generated.h"

/** One item a player receives when the match starts (see FMKHStartingLoadoutRow). */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHStartingItem
{
	GENERATED_BODY()

	/** Item definition to generate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (Categories = "Item"))
	FGameplayTag ItemTag;

	/** Quantity: a stack for consumables, the number of separate items for equipment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	/** Rarity of the generated equipment (consumables keep their definition's rarity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FMKHItemRarityRule RarityRule;
};
