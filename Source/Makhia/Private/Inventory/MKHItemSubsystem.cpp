// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/MKHItemSubsystem.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Data/EquipmentStatEffects.h"
#include "Data/MKHDataSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Equipment/EquipmentDefinition.h"
#include "Equipment/Rarity/RarityDefinition.h"
#include "Inventory/Pickup/MKHPickupItem.h"
#include "Libraries/EquipmentRollLibrary.h"
#include "Libraries/MKHAbilitySystemLibrary.h"
#include "MKHLogChannels.h"

UMKHItemSubsystem* UMKHItemSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(GEngine)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHItemSubsystem>() : nullptr;
}

int64 UMKHItemSubsystem::GenerateItemID()
{
	if (!HasItemAuthority())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHItemSubsystem::GenerateItemID: ignored, item IDs are server only."));
		return 0;
	}

	return ++LastItemID;
}

FMKHItemEntry UMKHItemSubsystem::GenerateItem(const FGameplayTag& ItemTag, int32 Quantity, const FMKHItemRarityRule& RarityRule)
{
	if (!HasItemAuthority())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHItemSubsystem::GenerateItem: ignored '%s', items are generated on the server only."), *ItemTag.ToString());
		return FMKHItemEntry();
	}

	const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(this);
	const FMasterItemDefinition* ItemDef = IsValid(DataSubsystem) ? DataSubsystem->FindItemDefinition(ItemTag) : nullptr;
	if (!ItemDef)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHItemSubsystem::GenerateItem: no item definition for '%s'."), *ItemTag.ToString());
		return FMKHItemEntry();
	}

	FMKHItemEntry NewItem;
	NewItem.ItemTag = ItemTag;
	NewItem.ItemID = GenerateItemID();
	NewItem.Quantity = Quantity;
	NewItem.RarityTag = PickDefinitionRarity(*ItemDef, *DataSubsystem);

	if (ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Equipment)
		&& IsValid(DataSubsystem->GetEquipmentStatEffects())
		&& IsValid(DataSubsystem->GetRarityTable()))
	{
		RollEquipmentEntry(NewItem, *ItemDef, *DataSubsystem, RarityRule);
	}

	return NewItem;
}

void UMKHItemSubsystem::RegisterPickup(AMKHPickupItem* Pickup)
{
	if (!IsValid(Pickup) || !HasItemAuthority())
	{
		return;
	}

	RegisteredPickups.RemoveAllSwap([](const TWeakObjectPtr<AMKHPickupItem>& Registered) { return !Registered.IsValid(); });
	RegisteredPickups.AddUnique(Pickup);
}

void UMKHItemSubsystem::UnregisterPickup(AMKHPickupItem* Pickup)
{
	RegisteredPickups.RemoveSwap(Pickup);
}

void UMKHItemSubsystem::DestroyAllPickups()
{
	if (!HasItemAuthority())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHItemSubsystem::DestroyAllPickups: ignored, pickups are destroyed by the server only."));
		return;
	}

	// Destroy() unregisters each pickup from EndPlay: iterate a detached copy of the registry.
	const TArray<TWeakObjectPtr<AMKHPickupItem>> PickupsToDestroy = MoveTemp(RegisteredPickups);
	RegisteredPickups.Reset();

	int32 NumDestroyed = 0;
	for (const TWeakObjectPtr<AMKHPickupItem>& Pickup : PickupsToDestroy)
	{
		if (Pickup.IsValid() && Pickup->Destroy())
		{
			++NumDestroyed;
		}
	}

	UE_LOG(LogMKHInventory, Log, TEXT("UMKHItemSubsystem::DestroyAllPickups: destroyed %d pickups."), NumDestroyed);
}

bool UMKHItemSubsystem::HasItemAuthority() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	return IsValid(World) && World->GetNetMode() != NM_Client;
}

void UMKHItemSubsystem::RollEquipmentEntry(FMKHItemEntry& NewItem, const FMasterItemDefinition& ItemDef, const UMKHDataSubsystem& DataSubsystem,
	const FMKHItemRarityRule& RarityRule) const
{
	if (!ItemDef.EquipmentItemProps.EquipmentClass)
	{
		return;
	}

	const UEquipmentDefinition* EquipmentCDO = GetDefault<UEquipmentDefinition>(ItemDef.EquipmentItemProps.EquipmentClass);
	if (!IsValid(EquipmentCDO))
	{
		return;
	}

	const FRarityDefinition* Rarity = ResolveEntryRarity(NewItem, DataSubsystem.GetRarityTable(), RarityRule);
	if (!Rarity)
	{
		return;
	}

	const UEquipmentStatEffects* StatData = DataSubsystem.GetEquipmentStatEffects();
	NewItem.RarityTag = Rarity->RarityTag;
	NewItem.EffectPackage.StatEffects = UEquipmentRollLibrary::RollPassiveStats(EquipmentCDO, StatData, Rarity->NumPassiveStats);

	if (EquipmentCDO->SlotTag.MatchesTag(MKHGameplayTags::Equip::WeaponSlot))
	{
		NewItem.EffectPackage.Abilities = UEquipmentRollLibrary::ResolveAbilitiesByTags(EquipmentCDO->BasicAbilitiesGranted, StatData);

		const TArray<FEquipmentAbilityDefinition> RolledAbilities = UEquipmentRollLibrary::RollActiveAbilities(
			EquipmentCDO, StatData, Rarity->NumActiveAbilities);

		// Optional abilities from rarity are independent of guaranteed basic abilities.
		NewItem.EffectPackage.Abilities.Append(RolledAbilities);
		UMKHAbilitySystemLibrary::AssignDynamicSkillInputTag(NewItem);
	}
}

FGameplayTag UMKHItemSubsystem::PickDefinitionRarity(const FMasterItemDefinition& ItemDef, const UMKHDataSubsystem& DataSubsystem) const
{
	if (ItemDef.RarityTags.IsEmpty())
	{
		return FGameplayTag();
	}

	const FRarityDefinition* Rarity = UEquipmentRollLibrary::RollRarityAmong(DataSubsystem.GetRarityTable(), ItemDef.RarityTags);
	if (!Rarity)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHItemSubsystem: none of the rarities '%s' of '%s' is in the rarity table, using the default rarity."),
			*ItemDef.RarityTags.ToStringSimple(), *ItemDef.ItemTag.ToString());
		return FGameplayTag();
	}

	return Rarity->RarityTag;
}

const FRarityDefinition* UMKHItemSubsystem::ResolveEntryRarity(const FMKHItemEntry& Item, const UDataTable* RarityTable,
	const FMKHItemRarityRule& RarityRule) const
{
	if (RarityRule.Mode != EMKHItemRarityMode::Default)
	{
		if (const FRarityDefinition* RuleRarity = ApplyRarityRule(RarityTable, RarityRule))
		{
			return RuleRarity;
		}

		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHItemSubsystem: the rarity rule of '%s' names no rarity of the rarity table, using the default rarity."),
			*Item.ItemTag.ToString());
	}

	// A rarity already picked from the definition's RarityTags skips the free roll.
	if (Item.RarityTag.IsValid())
	{
		const FRarityDefinition* Rarity = UEquipmentRollLibrary::GetRarityByTag(RarityTable, Item.RarityTag);
		if (!Rarity)
		{
			UE_LOG(LogMKHInventory, Warning, TEXT("UMKHItemSubsystem: pre-defined rarity tag '%s' not found in the rarity table."),
				*Item.RarityTag.ToString());
		}
		return Rarity;
	}

	return UEquipmentRollLibrary::RollRarity(RarityTable);
}

const FRarityDefinition* UMKHItemSubsystem::ApplyRarityRule(const UDataTable* RarityTable, const FMKHItemRarityRule& RarityRule) const
{
	switch (RarityRule.Mode)
	{
	case EMKHItemRarityMode::Fixed:
		return UEquipmentRollLibrary::GetRarityByTag(RarityTable, RarityRule.FixedRarityTag);
	case EMKHItemRarityMode::Weighted:
		return UEquipmentRollLibrary::RollRarityWeighted(RarityTable, RarityRule.RarityWeights);
	default:
		return nullptr;
	}
}
