// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MKHItemRarityRule.generated.h"

/** How the rarity of a newly generated equipment item is decided. */
UENUM(BlueprintType)
enum class EMKHItemRarityMode : uint8
{
	/** The pre-defined rarity of the item definition, otherwise a roll on the whole rarity table. */
	Default,
	/** Always the given rarity. */
	Fixed,
	/** A weighted roll restricted to the listed rarities. */
	Weighted
};

/**
 * Optional override of the rarity of a generated item, passed to UMKHItemSubsystem::GenerateItem.
 * Only equipment rolls a rarity: any other item (e.g. consumables) always keeps the rarity of its definition.
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHItemRarityRule
{
	GENERATED_BODY()

	/** How the rarity is decided. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rarity")
	EMKHItemRarityMode Mode = EMKHItemRarityMode::Default;

	/** Rarity given to the item when Mode is Fixed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rarity",
		meta = (Categories = "Item.Rarity", EditCondition = "Mode == EMKHItemRarityMode::Fixed", EditConditionHides))
	FGameplayTag FixedRarityTag;

	/** Relative weight of each allowed rarity when Mode is Weighted; rarities not listed (or weighted 0) are never rolled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rarity",
		meta = (Categories = "Item.Rarity", ForceInlineRow, EditCondition = "Mode == EMKHItemRarityMode::Weighted", EditConditionHides))
	TMap<FGameplayTag, float> RarityWeights;
};
