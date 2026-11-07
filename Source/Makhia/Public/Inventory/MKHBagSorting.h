// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MKHBagSorting.generated.h"

/** Criterion the bag is sorted by. Full model: Documentation/InventoryDashboard.md, "Bag sorting". */
UENUM(BlueprintType)
enum class EMKHBagSortMode : uint8
{
	/** Order of arrival: the bag was never sorted. */
	None,
	/** Swords, hatchets, hammers, helmets, chests, legs, consumables; ties by rarity from the highest. */
	Category,
	/** From the lowest rarity; ties by category. */
	RarityAscending,
	/** From the highest rarity; ties by category. */
	RarityDescending
};

/** What the sort compares of one item. */
struct FMKHBagSortKey
{
	/** Position of the item's category (FMKHBagSorting::GetCategoryRank). */
	int32 CategoryRank = 0;

	/** Position of the item's rarity (UMKHDataSubsystem::GetRarityRank). */
	int32 RarityRank = 0;

	/** The item's ID, the last tie break. */
	int64 ItemID = 0;
};

/** Pure rules of the bag order, covered by the Makhia.Inventory.BagSorting spec. */
struct MAKHIA_API FMKHBagSorting
{
	/**
	 * Gets the position of an item's category in the bag order.
	 *
	 * @param ItemTag Tag of the item definition.
	 * @return The lower, the earlier: swords, hatchets, hammers, helmets, chests, legs, consumables, anything else.
	 */
	static int32 GetCategoryRank(const FGameplayTag& ItemTag);

	/**
	 * Compares two items: the criterion first, the other criterion on a tie, then the ItemID from the lowest.
	 *
	 * @param A    First item.
	 * @param B    Second item.
	 * @param Mode Criterion; None keeps the ItemID alone.
	 * @return True when A comes before B.
	 */
	static bool IsBefore(const FMKHBagSortKey& A, const FMKHBagSortKey& B, EMKHBagSortMode Mode);
};
