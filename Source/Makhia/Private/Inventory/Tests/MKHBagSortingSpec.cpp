// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

// Automation Spec of the bag order (FMKHBagSorting). Run from Session Frontend (Automation, filter "Makhia.Inventory").
// See Documentation/InventoryDashboard.md.

#include "Inventory/MKHBagSorting.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FMKHBagSortingSpec, "Makhia.Inventory.BagSorting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

	/** Sorts sample items and returns their IDs in the resulting order. */
	TArray<int64> SortedIDs(TArray<FMKHBagSortKey> Keys, EMKHBagSortMode Mode) const
	{
		Keys.Sort([Mode](const FMKHBagSortKey& A, const FMKHBagSortKey& B) { return FMKHBagSorting::IsBefore(A, B, Mode); });

		TArray<int64> IDs;
		for (const FMKHBagSortKey& Key : Keys)
		{
			IDs.Add(Key.ItemID);
		}
		return IDs;
	}
END_DEFINE_SPEC(FMKHBagSortingSpec)

void FMKHBagSortingSpec::Define()
{
	// Category rank, rarity rank, ItemID. Rank -1 is a consumable with no rarity.
	const TArray<FMKHBagSortKey> Items = {
		{ 1, 4, 10 },	// Legendary armor
		{ 0, 0, 20 },	// Common weapon
		{ 2, -1, 30 },	// Potion
		{ 0, 4, 40 },	// Legendary weapon
		{ 1, 0, 50 },	// Common armor
		{ 0, 4, 5 }		// Older legendary weapon
	};

	Describe("GetCategoryRank", [this]()
	{
		It("keeps each item type together, in the bag order", [this]()
		{
			const TArray<FName> ItemTags = {
				TEXT("Item.Equipment.Weapon.Sword.BronzeSword"),
				TEXT("Item.Equipment.Weapon.Hatchet.BronzeHatchet"),
				TEXT("Item.Equipment.Weapon.Hammer.BronzeHammer"),
				TEXT("Item.Equipment.Armor.Headwear.BronzeHelmet"),
				TEXT("Item.Equipment.Armor.Chestwear.BronzeChest"),
				TEXT("Item.Equipment.Armor.Legwear.BronzeLeggins"),
				TEXT("Item.Consumable.HealthPotion")
			};

			for (int32 Index = 1; Index < ItemTags.Num(); ++Index)
			{
				const int32 Previous = FMKHBagSorting::GetCategoryRank(FGameplayTag::RequestGameplayTag(ItemTags[Index - 1]));
				const int32 Current = FMKHBagSorting::GetCategoryRank(FGameplayTag::RequestGameplayTag(ItemTags[Index]));
				TestTrue(ItemTags[Index - 1].ToString() + TEXT(" comes first"), Previous < Current);
			}
		});

		It("gives the same rank to two items of one type", [this]()
		{
			TestEqual("Swords",
				FMKHBagSorting::GetCategoryRank(FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Weapon.Sword.BronzeSword"))),
				FMKHBagSorting::GetCategoryRank(FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Weapon.Sword.Champion"))));
		});
	});

	Describe("IsBefore", [this, Items]()
	{
		It("Category: by category, ties by rarity from the highest, then by ItemID", [this, Items]()
		{
			const TArray<int64> Expected = { 5, 40, 20, 10, 50, 30 };
			TestTrue("Order", SortedIDs(Items, EMKHBagSortMode::Category) == Expected);
		});

		It("RarityAscending: from the lowest rarity, ties by category, then by ItemID", [this, Items]()
		{
			const TArray<int64> Expected = { 30, 20, 50, 5, 40, 10 };
			TestTrue("Order", SortedIDs(Items, EMKHBagSortMode::RarityAscending) == Expected);
		});

		It("RarityDescending: from the highest rarity, ties by category, then by ItemID", [this, Items]()
		{
			const TArray<int64> Expected = { 5, 40, 10, 20, 50, 30 };
			TestTrue("Order", SortedIDs(Items, EMKHBagSortMode::RarityDescending) == Expected);
		});

		It("None: by ItemID alone", [this, Items]()
		{
			const TArray<int64> Expected = { 5, 10, 20, 30, 40, 50 };
			TestTrue("Order", SortedIDs(Items, EMKHBagSortMode::None) == Expected);
		});
	});
}

#endif
