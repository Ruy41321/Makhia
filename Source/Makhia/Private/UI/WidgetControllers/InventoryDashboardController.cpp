// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetControllers/InventoryDashboardController.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Data/MKHDataSubsystem.h"
#include "Equipment/EquipmentDefinition.h"
#include "QuickSlot/MKHQuickSlotPreferenceComponent.h"
#include "QuickSlot/QuickSlotManagerComponent.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Interfaces/InventoryInterface.h"
#include "Interfaces/EquipmentInterface.h"
#include "Interfaces/QuickSlotInterface.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "Inventory/MKHSkillLayout.h"
#include "UI/HUD/Inventory/InventoryDashboardWidget.h"

void UInventoryDashboardController::SetOwningInventory()
{
	OwningInventoryComp = IInventoryInterface::Execute_GetInventoryComponent(OwningActor);
}

void UInventoryDashboardController::SetOwningEquipmentManagerComp()
{
	OwningEquipmentManagerComp = IEquipmentInterface::Execute_GetEquipmentManagerComponent(OwningActor);
}

void UInventoryDashboardController::SetOwningQuickSlotManagerComp()
{
	OwningQuickSlotManagerComp = IQuickSlotInterface::Execute_GetQuickSlotManagerComponent(OwningActor);
}

void UInventoryDashboardController::SetInventoryWidget(UInventoryDashboardWidget* InInventoryWidget)
{
	OwningInventoryDashboardWidget = InInventoryWidget;

	// The widget routes native actions (e.g. split to quick slot) back through its controller.
	if (IsValid(OwningInventoryDashboardWidget))
	{
		OwningInventoryDashboardWidget->SetWidgetController(this);
	}

	if (IsValid(OwningInventoryDashboardWidget) && IsValid(OwningInventoryComp))
	{
		BindDelegatesToWidget();
	}
}

bool UInventoryDashboardController::EnsureOwningInventory()
{
	if (!IsValid(OwningInventoryComp))
	{
		SetOwningInventory();
	}
	return IsValid(OwningInventoryComp);
}

bool UInventoryDashboardController::EnsureOwningEquipmentManagerComp()
{
	if (!IsValid(OwningEquipmentManagerComp))
	{
		SetOwningEquipmentManagerComp();
	}
	return IsValid(OwningEquipmentManagerComp);	
}

bool UInventoryDashboardController::EnsureOwningQuickSlotManagerComp()
{
	if (!IsValid(OwningQuickSlotManagerComp))
	{
		SetOwningQuickSlotManagerComp();
	}
	return IsValid(OwningQuickSlotManagerComp);
}

void UInventoryDashboardController::BindCallbacksToDependencies()
{
	if (EnsureOwningInventory())
	{
		OwningInventoryComp->InventoryList.InventoryItemChangedDelegate.AddLambda(
			[this](const FRPGInventoryEntry& DirtyItem)
			{
				UInventoryItem* Item = UInventoryItem::CreateFromInventoryEntry(this, DirtyItem);
				DashboardBagItemChangedDelegate.Broadcast(Item);
			});

		OwningInventoryComp->InventoryList.InventoryItemRemovedDelegate.AddLambda(
			[this](const int64 RemovedItemID)
			{
				DashboardBagItemRemovedDelegate.Broadcast(RemovedItemID);
			});
		
		OwningInventoryComp->InventoryList.QuickSlotItemRelocatedDelegate.AddLambda(
		[this](const FRPGInventoryEntry& InventoryEntry)
			{
				UInventoryItem* Item = UInventoryItem::CreateFromInventoryEntry(this, InventoryEntry);
				QuickSlotItemRelocatedDelegate.Broadcast(Item);
			});
		
		OwningInventoryComp->InventoryList.QuickSlotItemChangeDelegate.AddLambda(
		[this](const FRPGInventoryEntry& InventoryEntry)			{
				UInventoryItem* Item = UInventoryItem::CreateFromInventoryEntry(this, InventoryEntry);
				QuickSlotItemChangedDelegate.Broadcast(Item);
			});
		
		OwningInventoryComp->InventoryList.QuickSlotItemRemovedDelegate.AddLambda(
			[this](const int64 RemovedItemID)
			{
				QuickSlotItemRemovedDelegate.Broadcast(RemovedItemID);
			});
	}

	if (EnsureOwningEquipmentManagerComp())
	{
		OwningEquipmentManagerComp->EquipmentList.EquipmentEntryDelegate.AddLambda(
			[this](const FRPGEquipmentEntry& EquipmentEntry)
			{
				UInventoryItem* Item = UInventoryItem::CreateFromEquipmentEntry(this, EquipmentEntry);
				DashboardEquipmentChangeDelegate.Broadcast(Item);
			});
		OwningEquipmentManagerComp->EquipmentList.UnEquippedEntryDelegate.AddLambda(
			[this](const FRPGEquipmentEntry& UnEquippedEntry)
			{
				DashboardEquipmentRemovedDelegate.Broadcast(UnEquippedEntry.SlotTag);
			});
	}
}

void UInventoryDashboardController::BroadcastInitialValues()
{
	// Bag and equipment are broadcast independently: a missing component only skips its own part.
	if (EnsureOwningInventory())
	{
		// Entries quick-slotted before the first opening (starting loadout preferences) go to their quick slot.
		for (const FRPGInventoryEntry& Entry : OwningInventoryComp->GetInventoryEntries())
		{
			UInventoryItem* Item = UInventoryItem::CreateFromInventoryEntry(this, Entry);
			if (Entry.bIsQuickSlotted)
			{
				QuickSlotItemRelocatedDelegate.Broadcast(Item);
			}
			else
			{
				DashboardBagItemChangedDelegate.Broadcast(Item);
			}
		}
	}

	if (EnsureOwningEquipmentManagerComp())
	{
		TArray<FRPGEquipmentEntry> FoundEntries;
		OwningEquipmentManagerComp->EquipmentList.GetEntries(FoundEntries);
		for (const FRPGEquipmentEntry& Entry : FoundEntries)
		{
			UInventoryItem* Item = UInventoryItem::CreateFromEquipmentEntry(this, Entry);
			DashboardEquipmentChangeDelegate.Broadcast(Item);
		}
	}
}

void UInventoryDashboardController::BindDelegatesToWidget_Implementation()
{
	if (!IsValid(OwningInventoryDashboardWidget))
	{
		return;
	}
	
	DashboardBagItemChangedDelegate.AddDynamic(OwningInventoryDashboardWidget, &UInventoryDashboardWidget::OnInventoryItemChanged);
	DashboardBagItemRemovedDelegate.AddDynamic(OwningInventoryDashboardWidget, &UInventoryDashboardWidget::OnInventoryItemRemoved);
	DashboardEquipmentChangeDelegate.AddDynamic(OwningInventoryDashboardWidget, &UInventoryDashboardWidget::OnItemEquipped);
	DashboardEquipmentRemovedDelegate.AddDynamic(OwningInventoryDashboardWidget, &UInventoryDashboardWidget::OnItemUnequipped);
	QuickSlotItemRelocatedDelegate.AddDynamic(OwningInventoryDashboardWidget, &UInventoryDashboardWidget::OnQuickSlotItemRelocated);
	QuickSlotItemChangedDelegate.AddDynamic(OwningInventoryDashboardWidget, &UInventoryDashboardWidget::OnQuickSlotItemChanged);
	QuickSlotItemRemovedDelegate.AddDynamic(OwningInventoryDashboardWidget, &UInventoryDashboardWidget::OnQuickSlotItemRemoved);
}

void UInventoryDashboardController::SplitStackToQuickSlot(int64 ItemID, int32 Quantity, const FGameplayTag& QuickSlotTag)
{
	if (EnsureOwningInventory())
	{
		OwningInventoryComp->SplitStackToQuickSlot(ItemID, Quantity, QuickSlotTag);
	}
}

FGameplayTag UInventoryDashboardController::FindConsumableQuickSlotForSplit()
{
	using namespace MKHGameplayTags::Equip;

	if (EnsureOwningQuickSlotManagerComp())
	{
		for (const FGameplayTag& SlotTag : { ConsumableQuickSlot1.GetTag(), ConsumableQuickSlot2.GetTag(), ConsumableQuickSlot3.GetTag() })
		{
			if (OwningQuickSlotManagerComp->GetQuickSlotItemID(SlotTag) == 0)
			{
				return SlotTag;
			}
		}
	}
	return ConsumableQuickSlot1;
}

TArray<FGameplayTag> UInventoryDashboardController::FindQuickSlotChoices(const UInventoryItem& Item)
{
	using namespace MKHGameplayTags::Equip;

	TArray<FGameplayTag> Choices;
	const FGameplayTag ItemTag = Item.GetItemTag();
	if (ItemTag.MatchesTag(Category_Consumable))
	{
		Choices = { ConsumableQuickSlot1.GetTag(), ConsumableQuickSlot2.GetTag(), ConsumableQuickSlot3.GetTag() };
	}
	else if (ItemTag.MatchesTag(Category_Weapon))
	{
		// Same rule as the mouse (FindQuickSlotForItem): one weapon per type in the quick slots.
		const FGameplayTag SameTypeSlot = FindWeaponQuickSlotOfSameType(Item);
		Choices = SameTypeSlot.IsValid() ? TArray<FGameplayTag>{ SameTypeSlot } : TArray<FGameplayTag>{ WeaponQuickSlot1.GetTag(), WeaponQuickSlot2.GetTag() };
	}

	Choices.Remove(Item.GetQuickSlotTag());
	return Choices;
}

FGameplayTag UInventoryDashboardController::FindWeaponQuickSlotOfSameType(const UInventoryItem& Item)
{
	using namespace MKHGameplayTags::Equip;

	if (!EnsureOwningQuickSlotManagerComp() || !EnsureOwningInventory() || !Item.GetSlotTag().IsValid())
	{
		return FGameplayTag();
	}

	for (const FGameplayTag& QuickSlotTag : { WeaponQuickSlot1.GetTag(), WeaponQuickSlot2.GetTag() })
	{
		const int64 OccupantID = OwningQuickSlotManagerComp->GetQuickSlotItemID(QuickSlotTag);
		if (OccupantID == 0 || OccupantID == Item.GetItemID())
		{
			continue;
		}

		const FRPGInventoryEntry Occupant = OwningInventoryComp->FindInventoryEntryByID(OccupantID);
		const FMasterItemDefinition* Definition = Occupant.IsValid() ? UMKHDataSubsystem::FindItemDefinitionForContext(this, Occupant.Item.ItemTag) : nullptr;
		const UEquipmentDefinition* Equipment = (Definition && IsValid(Definition->EquipmentItemProps.EquipmentClass))
			? GetDefault<UEquipmentDefinition>(Definition->EquipmentItemProps.EquipmentClass)
			: nullptr;
		if (IsValid(Equipment) && Equipment->SlotTag.MatchesTagExact(Item.GetSlotTag()))
		{
			return QuickSlotTag;
		}
	}
	return FGameplayTag();
}

FGameplayTag UInventoryDashboardController::FindQuickSlotForItem(const UInventoryItem& Item)
{
	using namespace MKHGameplayTags::Equip;

	const FGameplayTag ItemTag = Item.GetItemTag();
	if (ItemTag.MatchesTag(Category_Consumable))
	{
		return FindConsumableQuickSlotForSplit();
	}
	if (!ItemTag.MatchesTag(Category_Weapon))
	{
		return FGameplayTag();
	}

	const FGameplayTag SameTypeSlot = FindWeaponQuickSlotOfSameType(Item);
	if (SameTypeSlot.IsValid())
	{
		return SameTypeSlot;
	}
	const bool bFirstSlotEmpty = !EnsureOwningQuickSlotManagerComp() || OwningQuickSlotManagerComp->GetQuickSlotItemID(WeaponQuickSlot1) == 0;
	return bFirstSlotEmpty ? WeaponQuickSlot1.GetTag() : WeaponQuickSlot2.GetTag();
}

void UInventoryDashboardController::UseItem(int64 ItemID, int32 Quantity)
{
	if (EnsureOwningInventory())
	{
		OwningInventoryComp->UseItem(ItemID, Quantity);
	}
}

FGameplayTag UInventoryDashboardController::GetItemTag(int64 ItemID)
{
	return EnsureOwningInventory() ? OwningInventoryComp->FindInventoryEntryByID(ItemID).Item.ItemTag : FGameplayTag();
}

void UInventoryDashboardController::UnequipItem(const FGameplayTag& EquipmentSlotTag)
{
	if (!EnsureOwningEquipmentManagerComp())
	{
		return;
	}

	const FRPGEquipmentEntry* Entry = OwningEquipmentManagerComp->GetEquipmentEntryBySlot(EquipmentSlotTag);
	UMKHQuickSlotPreferenceComponent* Preferences = OwningEquipmentManagerComp->GetOwner()->FindComponentByClass<UMKHQuickSlotPreferenceComponent>();
	if (Entry != nullptr && IsValid(Preferences))
	{
		Preferences->NotifyArmorUnequippedByPlayer(Entry->Item.ItemTag);
	}
	OwningEquipmentManagerComp->UnEquipItem(EquipmentSlotTag);
}

void UInventoryDashboardController::SwapWeaponSkills(int64 ItemID, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot)
{
	if (!EnsureOwningInventory() || !OwningInventoryComp->CanSwapWeaponSkills(ItemID, FromSlot, ToSlot))
	{
		return;
	}

	const FRPGInventoryEntry Entry = OwningInventoryComp->FindInventoryEntryByID(ItemID);
	UMKHQuickSlotPreferenceComponent* Preferences = OwningInventoryComp->GetOwner()->FindComponentByClass<UMKHQuickSlotPreferenceComponent>();
	if (IsValid(Preferences))
	{
		if (const FEquipmentAbilityDefinition* Moved = FMKHSkillLayout::FindSkillInSlot(Entry.Item.EffectPackage, FromSlot))
		{
			Preferences->NotifySkillSlotChosenByPlayer(Moved->AbilityTag, ToSlot);
		}
		if (const FEquipmentAbilityDefinition* Displaced = FMKHSkillLayout::FindSkillInSlot(Entry.Item.EffectPackage, ToSlot))
		{
			Preferences->NotifySkillSlotChosenByPlayer(Displaced->AbilityTag, FromSlot);
		}
	}
	OwningInventoryComp->SwapWeaponSkills(ItemID, FromSlot, ToSlot);
}

void UInventoryDashboardController::AddToQuickSlot(int64 ItemID, const FGameplayTag& QuickSlotTag)
{
	if (EnsureOwningInventory())
	{
		OwningInventoryComp->AddEntryToQuickSlot(ItemID, QuickSlotTag);
	}
}

void UInventoryDashboardController::RemoveFromQuickSlot(int64 ItemID)
{
	if (!EnsureOwningInventory())
	{
		return;
	}

	UMKHQuickSlotPreferenceComponent* Preferences = OwningInventoryComp->GetOwner()->FindComponentByClass<UMKHQuickSlotPreferenceComponent>();
	if (IsValid(Preferences) && EnsureOwningQuickSlotManagerComp())
	{
		Preferences->NotifyQuickSlotEmptiedByPlayer(OwningQuickSlotManagerComp->FindQuickSlotTagByItemID(ItemID));
	}
	OwningInventoryComp->RemoveEntryFromQuickSlot(ItemID);
}

void UInventoryDashboardController::DropItem(int64 ItemID, int32 Quantity)
{
	if (EnsureOwningInventory())
	{
		OwningInventoryComp->DropItem(ItemID, Quantity);
	}
}

void UInventoryDashboardController::UnbindAllEventsFromDelegates()
{
	DashboardBagItemChangedDelegate.Clear();
	DashboardBagItemRemovedDelegate.Clear();
	DashboardEquipmentChangeDelegate.Clear();
	DashboardEquipmentRemovedDelegate.Clear();
	QuickSlotItemRelocatedDelegate.Clear();
	QuickSlotItemChangedDelegate.Clear();
	QuickSlotItemRemovedDelegate.Clear();
}
