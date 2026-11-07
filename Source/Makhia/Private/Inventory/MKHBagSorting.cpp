// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/MKHBagSorting.h"

#include "AbilitySystem/MKHGameplayTags.h"

// ============================================================
// Public Interface
// ============================================================

int32 FMKHBagSorting::GetCategoryRank(const FGameplayTag& ItemTag)
{
	using namespace MKHGameplayTags::Equip;

	// The generic roots catch a new weapon or armor type until it gets its own place here.
	static const TArray<FGameplayTag> CategoryOrder = {
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Weapon.Sword")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Weapon.Hatchet")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Weapon.Hammer")),
		Category_Weapon,
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Armor.Headwear")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Armor.Chestwear")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Armor.Legwear")),
		Category_Armor,
		Category_Consumable
	};

	const int32 Rank = CategoryOrder.IndexOfByPredicate([&ItemTag](const FGameplayTag& Category) { return ItemTag.MatchesTag(Category); });
	return Rank != INDEX_NONE ? Rank : CategoryOrder.Num();
}

bool FMKHBagSorting::IsBefore(const FMKHBagSortKey& A, const FMKHBagSortKey& B, EMKHBagSortMode Mode)
{
	if (Mode != EMKHBagSortMode::None)
	{
		const bool bCategoryFirst = Mode == EMKHBagSortMode::Category;
		const bool bRarityUp = Mode == EMKHBagSortMode::RarityAscending;

		const int32 RarityOrder = bRarityUp ? A.RarityRank - B.RarityRank : B.RarityRank - A.RarityRank;
		const int32 CategoryOrder = A.CategoryRank - B.CategoryRank;

		const int32 First = bCategoryFirst ? CategoryOrder : RarityOrder;
		const int32 Second = bCategoryFirst ? RarityOrder : CategoryOrder;
		if (First != 0)
		{
			return First < 0;
		}
		if (Second != 0)
		{
			return Second < 0;
		}
	}
	return A.ItemID < B.ItemID;
}
