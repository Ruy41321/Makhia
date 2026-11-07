// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/Pickup/MKHPickupAutoEquip.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Data/MKHDataSubsystem.h"
#include "Equipment/EquipmentDefinition.h"
#include "Inventory/InventoryComponent.h"
#include "MKHLogChannels.h"
#include "QuickSlot/QuickSlotManagerComponent.h"

EMKHPickupAutoEquipAction FMKHPickupAutoEquip::GetAction(const UObject* WorldContext, const FGameplayTag& ItemTag)
{
	using namespace MKHGameplayTags::Equip;

	if (ItemTag.MatchesTag(Category_Weapon) || ItemTag.MatchesTag(Category_Consumable))
	{
		return EMKHPickupAutoEquipAction::QuickSlot;
	}
	if (!ItemTag.MatchesTag(Category_Equipment))
	{
		return EMKHPickupAutoEquipAction::None;
	}

	// Equipment without an equipment class cannot be equipped (UInventoryComponent::TryUseEquipment).
	const FMasterItemDefinition* Definition = UMKHDataSubsystem::FindItemDefinitionForContext(WorldContext, ItemTag);
	return (Definition && IsValid(Definition->EquipmentItemProps.EquipmentClass)) ? EMKHPickupAutoEquipAction::Equip : EMKHPickupAutoEquipAction::None;
}

void FMKHPickupAutoEquip::ApplyOnServer(UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots, int64 ItemID, int32 Quantity)
{
	const FRPGInventoryEntry Entry = Inventory.FindInventoryEntryByID(ItemID);
	if (!Entry.IsValid())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("Hold to equip: collected item %lld is not in the inventory of %s."), ItemID, *GetNameSafe(Inventory.GetOwner()));
		return;
	}

	const FGameplayTag& ItemTag = Entry.Item.ItemTag;
	switch (GetAction(&Inventory, ItemTag))
	{
	case EMKHPickupAutoEquipAction::Equip:
		Inventory.UseItem(ItemID, 1);
		UE_LOG(LogMKHInventory, Log, TEXT("Hold to equip: %s equipped by %s."), *ItemTag.ToString(), *GetNameSafe(Inventory.GetOwner()));
		return;

	case EMKHPickupAutoEquipAction::QuickSlot:
	{
		// A consumable merged into a stack that is already quick slotted needs nothing more.
		if (QuickSlots.IsItemQuickSlotted(ItemID))
		{
			return;
		}

		// Picked consumables join the quick-slotted stack of their tag rather than taking another slot.
		const bool bIsWeapon = ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Weapon);
		if (const int64 QuickSlottedStackID = bIsWeapon ? 0 : Inventory.FindQuickSlottedStackID(ItemTag); QuickSlottedStackID != 0)
		{
			Inventory.MoveQuantityToStack(ItemID, QuickSlottedStackID, Quantity);
			UE_LOG(LogMKHInventory, Log, TEXT("Hold to equip: %d x %s joined quick-slotted stack %lld for %s."), Quantity, *ItemTag.ToString(),
				QuickSlottedStackID, *GetNameSafe(Inventory.GetOwner()));
			return;
		}

		const FGameplayTag QuickSlotTag = bIsWeapon
			? ChooseWeaponQuickSlot(Inventory, QuickSlots, Entry.Item)
			: ChooseConsumableQuickSlot(QuickSlots);
		Inventory.AssignQuickSlotOnOwner(ItemID, QuickSlotTag);
		UE_LOG(LogMKHInventory, Log, TEXT("Hold to equip: %s -> %s for %s."), *ItemTag.ToString(), *QuickSlotTag.ToString(), *GetNameSafe(Inventory.GetOwner()));
		return;
	}

	default:
		return;
	}
}

FGameplayTag FMKHPickupAutoEquip::ChooseWeaponQuickSlot(const UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots,
	const FMKHItemEntry& Weapon)
{
	using namespace MKHGameplayTags::Equip;

	const FGameplayTag WeaponSlots[] = { WeaponQuickSlot1.GetTag(), WeaponQuickSlot2.GetTag() };
	const FGameplayTag WeaponType = GetWeaponType(&Inventory, Weapon.ItemTag);

	// One weapon per type: a weapon of the same type gives up its slot.
	FGameplayTag FirstEmptySlot;
	FGameplayTag FirstSlotNotInHand;
	for (const FGameplayTag& SlotTag : WeaponSlots)
	{
		const int64 OccupantID = QuickSlots.GetQuickSlotItemID(SlotTag);
		const FRPGInventoryEntry Occupant = OccupantID != 0 ? Inventory.FindInventoryEntryByID(OccupantID) : FRPGInventoryEntry();
		if (!Occupant.IsValid())
		{
			FirstEmptySlot = FirstEmptySlot.IsValid() ? FirstEmptySlot : SlotTag;
			continue;
		}

		if (WeaponType.IsValid() && GetWeaponType(&Inventory, Occupant.Item.ItemTag).MatchesTagExact(WeaponType))
		{
			return SlotTag;
		}
		if (!Occupant.bIsUsed && !FirstSlotNotInHand.IsValid())
		{
			FirstSlotNotInHand = SlotTag;
		}
	}

	if (FirstEmptySlot.IsValid())
	{
		return FirstEmptySlot;
	}
	return FirstSlotNotInHand.IsValid() ? FirstSlotNotInHand : WeaponSlots[0];
}

FGameplayTag FMKHPickupAutoEquip::ChooseConsumableQuickSlot(const UQuickSlotManagerComponent& QuickSlots)
{
	using namespace MKHGameplayTags::Equip;

	const FGameplayTag ConsumableSlots[] = { ConsumableQuickSlot1.GetTag(), ConsumableQuickSlot2.GetTag(), ConsumableQuickSlot3.GetTag() };
	for (const FGameplayTag& SlotTag : ConsumableSlots)
	{
		if (QuickSlots.GetQuickSlotItemID(SlotTag) == 0)
		{
			return SlotTag;
		}
	}
	return ConsumableSlots[0];
}

FGameplayTag FMKHPickupAutoEquip::GetWeaponType(const UObject* WorldContext, const FGameplayTag& ItemTag)
{
	const FMasterItemDefinition* Definition = UMKHDataSubsystem::FindItemDefinitionForContext(WorldContext, ItemTag);
	const UEquipmentDefinition* Equipment = (Definition && IsValid(Definition->EquipmentItemProps.EquipmentClass))
		? GetDefault<UEquipmentDefinition>(Definition->EquipmentItemProps.EquipmentClass)
		: nullptr;
	return IsValid(Equipment) ? Equipment->SlotTag : FGameplayTag();
}
