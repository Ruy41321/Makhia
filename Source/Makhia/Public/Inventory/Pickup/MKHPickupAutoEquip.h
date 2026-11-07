// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MKHPickupAutoEquip.generated.h"

class UInventoryComponent;
class UQuickSlotManagerComponent;
struct FMKHItemEntry;

/** What a hold-to-equip pickup does with the collected item. */
UENUM(BlueprintType)
enum class EMKHPickupAutoEquipAction : uint8
{
	/** The item is only picked up. */
	None,
	/** Armor (any equipment that is not a weapon): equipped, the previous piece goes back to the bag. */
	Equip,
	/** Weapon or consumable: put in a quick slot, the previous occupant goes back to the bag. */
	QuickSlot
};

/**
 * Rules of the hold-to-equip pickup: pressing Interact (IA_Interact) on a pickup collects the item at once; keeping
 * it held for the hold duration then equips it or puts it in a quick slot. Pure rules, no state: the server applies
 * them to the item it collected last (AMKHPlayerController::Server_EquipLastPickup), the pickup prompt reads GetAction
 * for its hint.
 *
 * Quick slot choice:
 * - weapon: the slot holding a weapon of the same type (UEquipmentDefinition::SlotTag, the "one weapon per type"
 *   rule of the inventory), else the first empty weapon slot, else the slot whose weapon is not in hand, else slot 1;
 * - consumable: the picked quantity moves into a quick-slotted stack of its tag if there is one, else the stack that
 *   received it goes to the first empty consumable slot, else slot 1.
 *
 * See Documentation/PickupSystemArchitecture.md, "Hold to equip".
 */
struct MAKHIA_API FMKHPickupAutoEquip
{
	/**
	 * Returns what holding Interact does with an item.
	 *
	 * @param WorldContext  Any object of the world (definition lookup).
	 * @param ItemTag       Item definition tag.
	 * @return Equip for armor, QuickSlot for weapons and consumables, None otherwise.
	 */
	static EMKHPickupAutoEquipAction GetAction(const UObject* WorldContext, const FGameplayTag& ItemTag);

	/**
	 * Server: equips or quick slots an item that was just collected. Quick slots are assigned on the owning
	 * machine (UInventoryComponent::AssignQuickSlotOnOwner); armor goes through UInventoryComponent::UseItem.
	 *
	 * @param Inventory   Collector's inventory.
	 * @param QuickSlots  Collector's quick slot manager (server mirror of the owner's quick slots).
	 * @param ItemID      Entry now holding the item (a consumable stack the pickup merged into keeps its own ID).
	 * @param Quantity    Quantity the pickup held (what moves into a quick-slotted stack).
	 */
	static void ApplyOnServer(UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots, int64 ItemID, int32 Quantity);

	/**
	 * Chooses the weapon quick slot for a new weapon (see the class comment).
	 *
	 * @param Inventory   Owner's inventory.
	 * @param QuickSlots  Owner's quick slot manager.
	 * @param Weapon      The weapon to quick slot.
	 * @return The weapon quick slot tag.
	 */
	static FGameplayTag ChooseWeaponQuickSlot(const UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots,
		const FMKHItemEntry& Weapon);

	/**
	 * Chooses the consumable quick slot for a stack that is not quick slotted yet: first empty slot, else slot 1.
	 *
	 * @param QuickSlots Owner's quick slot manager.
	 * @return The consumable quick slot tag.
	 */
	static FGameplayTag ChooseConsumableQuickSlot(const UQuickSlotManagerComponent& QuickSlots);

private:
	/**
	 * Gets the weapon type of an item: the equipment slot of its UEquipmentDefinition.
	 *
	 * @param WorldContext  Any object of the world (definition lookup).
	 * @param ItemTag       Weapon definition tag.
	 * @return The type, or an invalid tag when the item has no equipment definition.
	 */
	static FGameplayTag GetWeaponType(const UObject* WorldContext, const FGameplayTag& ItemTag);
};
