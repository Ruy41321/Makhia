// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UInventoryComponent;
class UQuickSlotManagerComponent;

/** One quick slot assignment picked by FMKHDefaultQuickSlots. */
struct FMKHQuickSlotAssignment
{
	/** Inventory item to quick slot. */
	int64 ItemID = 0;

	/** Quick slot the item goes to. */
	FGameplayTag QuickSlotTag;
};

/**
 * The starting loadout's default rules, shared by the player (UMKHQuickSlotPreferenceComponent) and the duel bot
 * (AMKHDuelBot). Quick slots: the rarest item of each kind fills its category's free slots, kinds in loadout order
 * (weapons: Primary, Secondary; consumables: First, Second, Third). Armor: the rarest piece of each type is worn.
 *
 * See Documentation/HUDQuickSlots.md, "Starting loadout quick slot preferences".
 */
struct MAKHIA_API FMKHDefaultQuickSlots
{
	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The weapon quick slots, in fill order. */
	static TConstArrayView<FGameplayTag> GetWeaponSlots();

	/** @return The consumable quick slots, in fill order. */
	static TConstArrayView<FGameplayTag> GetConsumableSlots();

	/**
	 * Picks the free slot of the rarest item of each kind of one category, kinds in loadout order. Items already quick
	 * slotted, of another category or missing are skipped; slots already bound in QuickSlots are skipped too.
	 *
	 * @param Inventory    Owner's inventory.
	 * @param QuickSlots   Owner's quick slot manager (on the server, the mirror of the owner's quick slots).
	 * @param ItemIDs      The loadout's items, in loadout order (UInventoryComponent::OnStartingLoadoutGranted).
	 * @param CategoryTag  Item category root (Item.Equipment.Weapon or Item.Consumable).
	 * @param SlotTags     The category's quick slots, in fill order.
	 * @return The assignments to apply, at most one per slot.
	 */
	static TArray<FMKHQuickSlotAssignment> PickForCategory(const UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots,
		TConstArrayView<int64> ItemIDs, const FGameplayTag& CategoryTag, TConstArrayView<FGameplayTag> SlotTags);

	/**
	 * Returns the armor type an item belongs to: the tag right below Item.Equipment.Armor
	 * (Item.Equipment.Armor.Headwear.BronzeHelmet → Item.Equipment.Armor.Headwear).
	 *
	 * @param ItemTag  Item definition tag.
	 * @return The type tag, or an invalid tag for items that are not armor.
	 */
	static FGameplayTag GetArmorType(const FGameplayTag& ItemTag);

	/**
	 * Picks the armor to wear: the rarest piece of each type, the first on a tie.
	 *
	 * @param Inventory           Owner's inventory.
	 * @param ItemIDs             The loadout's items, in loadout order.
	 * @param SkippedArmorTypes   Armor types left unworn (the player's choice, none for the bot).
	 * @return The items to use, at most one per type.
	 */
	static TArray<int64> PickArmorToEquip(const UInventoryComponent& Inventory, TConstArrayView<int64> ItemIDs, TConstArrayView<FGameplayTag> SkippedArmorTypes);
};
