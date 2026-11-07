// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/InventoryItem/InventoryItem.h"

#include "Data/MKHDataSubsystem.h"
#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Inventory/InventoryComponent.h"

UInventoryItem* UInventoryItem::CreateFromInventoryEntry(UObject* WorldContextObject, const FRPGInventoryEntry& InEntry)
{
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}

	UInventoryItem* NewItem = NewObject<UInventoryItem>(WorldContextObject);
	if (IsValid(NewItem))
	{
		NewItem->InitFromInventoryEntry(InEntry);
	}
	return NewItem;
}

UInventoryItem* UInventoryItem::CreateFromEquipmentEntry(UObject* WorldContextObject, const FRPGEquipmentEntry& InEntry)
{
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}

	UInventoryItem* NewItem = NewObject<UInventoryItem>(WorldContextObject);
	if (IsValid(NewItem))
	{
		NewItem->InitFromEquipmentEntry(InEntry);
	}
	return NewItem;
}

UInventoryItem* UInventoryItem::CreateFromItemEntry(UObject* WorldContextObject, const FMKHItemEntry& InItem)
{
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}

	UInventoryItem* NewItem = NewObject<UInventoryItem>(WorldContextObject);
	if (IsValid(NewItem))
	{
		NewItem->InitFromItemEntry(InItem);
	}
	return NewItem;
}

void UInventoryItem::InitFromItemEntry(const FMKHItemEntry& InItem)
{
	Item = InItem;
	SlotTag = FGameplayTag();
	bIsEquipped = false;
	bIsQuickSlotted = false;
	QuickSlotTag = FGameplayTag();
	ResolveItemDefinition();
}

void UInventoryItem::InitFromInventoryEntry(const FRPGInventoryEntry& InEntry)
{
	Item = InEntry.Item;
	SlotTag = FGameplayTag();
	bIsEquipped = InEntry.bIsUsed;
	bIsQuickSlotted = InEntry.bIsQuickSlotted;
	QuickSlotTag = InEntry.QuickSlotTag;
	ResolveItemDefinition();
}

void UInventoryItem::InitFromEquipmentEntry(const FRPGEquipmentEntry& InEquipEntry)
{
	Item = InEquipEntry.Item;
	Item.Quantity = 1;
	SlotTag = InEquipEntry.SlotTag;
	bIsEquipped = true;
	bIsQuickSlotted = false;
	QuickSlotTag = FGameplayTag();
	ResolveItemDefinition();
}

void UInventoryItem::CopyFrom(const UInventoryItem* Other)
{
	if (!IsValid(Other))
	{
		return;
	}

	CopyCoreDataFrom(*Other);
	OnItemUpdatedDelegate.Broadcast();
}

void UInventoryItem::CopyCoreDataFrom(const UInventoryItem& Other)
{
	Item = Other.Item;
	bIsEquipped = Other.bIsEquipped;
	bIsQuickSlotted = Other.bIsQuickSlotted;
	QuickSlotTag = Other.QuickSlotTag;
	ItemDefinition = Other.ItemDefinition;
	ItemName = Other.ItemName;
	SlotTag = Other.SlotTag;
	Icon = Other.Icon;
	WeaponHudIcon = Other.WeaponHudIcon;
	BaseWeaponDamage = Other.BaseWeaponDamage;
	BlockingStability = Other.BlockingStability;
	BaseArmorGiven = Other.BaseArmorGiven;
	Description = Other.Description;
	CategoryName = Other.CategoryName;
	ConsumableCooldownTime = Other.ConsumableCooldownTime;
	ConsumableCooldownTag = Other.ConsumableCooldownTag;
}

bool UInventoryItem::TryResolveItemDefinition(FMasterItemDefinition& OutItemDefinition) const
{
	const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(this);
	if (!IsValid(DataSubsystem))
	{
		return false;
	}

	return DataSubsystem->GetItemDefinition(Item.ItemTag, OutItemDefinition);
}

void UInventoryItem::CacheDerivedDataFromItemDefinition()
{
	ItemName = ItemDefinition.ItemName;
	Icon = ItemDefinition.Icon;
	WeaponHudIcon = ItemDefinition.WeaponHudIcon;
	Description = ItemDefinition.ItemDescription;
	ConsumableCooldownTime = ItemDefinition.ConsumableProps.CooldownTime;
	ConsumableCooldownTag = ItemDefinition.ConsumableProps.CooldownTag;
	BaseWeaponDamage = 0.f;
	BlockingStability = 0.f;
	BaseArmorGiven = 0.f;
	CategoryName = UMKHDataSubsystem::GetItemCategoryNameForContext(this, Item.ItemTag);

	if (IsValid(ItemDefinition.EquipmentItemProps.EquipmentClass))
	{
		if (const UEquipmentDefinition* EquipmentDefinition = GetDefault<UEquipmentDefinition>(ItemDefinition.EquipmentItemProps.EquipmentClass))
		{
			BaseWeaponDamage = EquipmentDefinition->BaseDamage;
			BlockingStability = EquipmentDefinition->BlockStabilityPercent;
			BaseArmorGiven = EquipmentDefinition->BaseArmorGiven;
			SlotTag = EquipmentDefinition->SlotTag;
		}
	}
}

void UInventoryItem::ResetDerivedData()
{
	ItemDefinition = FMasterItemDefinition();
	ItemName = FText();
	Icon = nullptr;
	WeaponHudIcon = nullptr;
	BaseWeaponDamage = 0.f;
	BlockingStability = 0.f;
	BaseArmorGiven = 0.f;
	Description = FText();
	CategoryName = FText();
	ConsumableCooldownTime = 0.f;
	ConsumableCooldownTag = FGameplayTag();
}

void UInventoryItem::ResolveItemDefinition()
{
	FMasterItemDefinition ResolvedDefinition;
	if (!TryResolveItemDefinition(ResolvedDefinition))
	{
		ResetDerivedData();
		return;
	}

	ItemDefinition = ResolvedDefinition;
	CacheDerivedDataFromItemDefinition();
}
