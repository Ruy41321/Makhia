// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "QuickSlot/MKHDefaultQuickSlots.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Data/MKHDataSubsystem.h"
#include "Inventory/InventoryComponent.h"
#include "QuickSlot/MKHQuickSlotPreferenceComponent.h"
#include "QuickSlot/QuickSlotManagerComponent.h"

namespace
{
	/**
	 * Keeps the rarest item of each kind, the first on a tie.
	 *
	 * @param Inventory    Owner's inventory.
	 * @param ItemIDs      Candidates, in loadout order.
	 * @param GetKind      Kind of an item tag.
	 * @return One item per kind, kinds in the order they first appear.
	 */
	TArray<int64> PickRarestOfEachKind(const UInventoryComponent& Inventory, TConstArrayView<int64> ItemIDs, FGameplayTag (*GetKind)(const FGameplayTag&))
	{
		const UMKHDataSubsystem* Data = UMKHDataSubsystem::Get(&Inventory);

		TArray<FGameplayTag> Kinds;
		TArray<int64> BestItems;
		TArray<int32> BestRanks;
		for (const int64 ItemID : ItemIDs)
		{
			const FRPGInventoryEntry Entry = Inventory.FindInventoryEntryByID(ItemID);
			const int32 Rank = IsValid(Data) ? Data->GetRarityRank(Entry.Item.RarityTag) : INDEX_NONE;
			const FGameplayTag Kind = GetKind(Entry.Item.ItemTag);

			const int32 KindIndex = Kinds.IndexOfByKey(Kind);
			if (KindIndex == INDEX_NONE)
			{
				Kinds.Add(Kind);
				BestItems.Add(ItemID);
				BestRanks.Add(Rank);
			}
			else if (Rank > BestRanks[KindIndex])
			{
				BestItems[KindIndex] = ItemID;
				BestRanks[KindIndex] = Rank;
			}
		}
		return BestItems;
	}
}

// ============================================================
// Public Interface
// ============================================================

TConstArrayView<FGameplayTag> FMKHDefaultQuickSlots::GetWeaponSlots()
{
	// Function-local: native tags are registered after static initialization.
	using namespace MKHGameplayTags::Equip;
	static const FGameplayTag Slots[] = { WeaponQuickSlot1.GetTag(), WeaponQuickSlot2.GetTag() };
	return Slots;
}

TConstArrayView<FGameplayTag> FMKHDefaultQuickSlots::GetConsumableSlots()
{
	using namespace MKHGameplayTags::Equip;
	static const FGameplayTag Slots[] = { ConsumableQuickSlot1.GetTag(), ConsumableQuickSlot2.GetTag(), ConsumableQuickSlot3.GetTag() };
	return Slots;
}

TArray<FMKHQuickSlotAssignment> FMKHDefaultQuickSlots::PickForCategory(const UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots,
	TConstArrayView<int64> ItemIDs, const FGameplayTag& CategoryTag, TConstArrayView<FGameplayTag> SlotTags)
{
	TArray<int64> Candidates;
	for (const int64 ItemID : ItemIDs)
	{
		const FRPGInventoryEntry Entry = Inventory.FindInventoryEntryByID(ItemID);
		if (Entry.IsValid() && Entry.Item.ItemTag.MatchesTag(CategoryTag) && !QuickSlots.IsItemQuickSlotted(ItemID))
		{
			Candidates.Add(ItemID);
		}
	}

	TArray<FMKHQuickSlotAssignment> Assignments;
	int32 SlotIndex = 0;
	for (const int64 ItemID : PickRarestOfEachKind(Inventory, Candidates, &UMKHQuickSlotPreferenceComponent::GetItemKindTag))
	{
		// A remote owner's server mirror only fills once the owner confirms: slots picked in this pass are skipped by index.
		while (SlotTags.IsValidIndex(SlotIndex) && QuickSlots.GetQuickSlotItemID(SlotTags[SlotIndex]) != 0)
		{
			++SlotIndex;
		}
		if (!SlotTags.IsValidIndex(SlotIndex))
		{
			break;
		}

		Assignments.Add({ ItemID, SlotTags[SlotIndex] });
		++SlotIndex;
	}
	return Assignments;
}

FGameplayTag FMKHDefaultQuickSlots::GetArmorType(const FGameplayTag& ItemTag)
{
	using namespace MKHGameplayTags::Equip;

	if (!ItemTag.MatchesTag(Category_Armor) || ItemTag.MatchesTagExact(Category_Armor))
	{
		return FGameplayTag();
	}

	FGameplayTag ArmorType = ItemTag;
	for (FGameplayTag Parent = ArmorType.RequestDirectParent(); Parent.IsValid() && !Parent.MatchesTagExact(Category_Armor); Parent = Parent.RequestDirectParent())
	{
		ArmorType = Parent;
	}
	return ArmorType;
}

TArray<int64> FMKHDefaultQuickSlots::PickArmorToEquip(const UInventoryComponent& Inventory, TConstArrayView<int64> ItemIDs, TConstArrayView<FGameplayTag> SkippedArmorTypes)
{
	TArray<int64> Candidates;
	for (const int64 ItemID : ItemIDs)
	{
		const FRPGInventoryEntry Entry = Inventory.FindInventoryEntryByID(ItemID);
		const FGameplayTag ArmorType = Entry.IsValid() ? GetArmorType(Entry.Item.ItemTag) : FGameplayTag();
		if (ArmorType.IsValid() && !SkippedArmorTypes.Contains(ArmorType))
		{
			Candidates.Add(ItemID);
		}
	}
	return PickRarestOfEachKind(Inventory, Candidates, &FMKHDefaultQuickSlots::GetArmorType);
}
