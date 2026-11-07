// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/InventoryComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Data/GenericClassReference.h"
#include "Data/MKHDataSubsystem.h"
#include "GameInstance/MKHGameInstance.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/EquipmentInterface.h"
#include "Interfaces/QuickSlotInterface.h"
#include "Inventory/MKHItemSubsystem.h"
#include "Inventory/MKHSkillLayout.h"
#include "Inventory/Pickup/MKHPickupItem.h"
#include "Net/UnrealNetwork.h"
#include "QuickSlot/QuickSlotManagerComponent.h"
#include "MKHLogChannels.h"

int64 FRPGInventoryList::AddItemEntry(const FMKHItemEntry& Item)
{
	if (!Item.IsValid())
	{
		return 0;
	}

	// Weapons are never removed from the inventory when equipped, so the entry already exists.
	if (FRPGInventoryEntry* ExistingEntry = FindEntryByID(Item.ItemID))
	{
		ExistingEntry->bIsUsed = false;
		MarkEntryChanged(*ExistingEntry);
		return Item.ItemID;
	}

	// Non-equipment items merge into an existing stack of the same tag.
	if (!Item.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Equipment))
	{
		if (const int64 StackID = TryStackItem(Item.ItemTag, FMath::Max(Item.Quantity, 1)); StackID != 0)
		{
			return StackID;
		}
	}

	AddSeparateEntry(Item);
	return FindEntryByID(Item.ItemID) ? Item.ItemID : 0;
}

void FRPGInventoryList::AddSeparateEntry(const FMKHItemEntry& Item)
{
	if (!Item.IsValid() || FindEntryByID(Item.ItemID))
	{
		return;
	}

	FRPGInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = Item;
	NewEntry.Item.Quantity = FMath::Max(Item.Quantity, 1);
	if (IsValid(OwnerComponent))
	{
		FMKHSkillLayout::ApplyPreferences(NewEntry.Item.EffectPackage, OwnerComponent->GetSkillLayout());
	}
	BroadcastNewEntry(NewEntry);
}

int64 FRPGInventoryList::TryStackItem(const FGameplayTag& ItemTag, int32 NumItems)
{
	FRPGInventoryEntry* Target = FindStackTarget(ItemTag, 0, true);
	if (!Target)
	{
		return 0;
	}

	Target->Item.Quantity += NumItems;
	MarkEntryChanged(*Target);
	return Target->Item.ItemID;
}

const FRPGInventoryEntry* FRPGInventoryList::FindQuickSlottedStack(const FGameplayTag& ItemTag) const
{
	return Entries.FindByPredicate([this, &ItemTag](const FRPGInventoryEntry& Entry)
	{
		return Entry.Item.ItemTag.MatchesTagExact(ItemTag) && IsValid(OwnerComponent) && OwnerComponent->IsItemQuickSlotted(Entry.Item.ItemID);
	});
}

FRPGInventoryEntry* FRPGInventoryList::FindStackTarget(const FGameplayTag& ItemTag, int64 ExcludedItemID, bool bAllowQuickSlotted)
{
	FRPGInventoryEntry* QuickSlottedMatch = nullptr;
	for (FRPGInventoryEntry& Entry : Entries)
	{
		if (Entry.Item.ItemID == ExcludedItemID || !Entry.Item.ItemTag.MatchesTagExact(ItemTag))
		{
			continue;
		}

		const bool bQuickSlotted = IsValid(OwnerComponent) && OwnerComponent->IsItemQuickSlotted(Entry.Item.ItemID);
		if (!bQuickSlotted)
		{
			return &Entry;
		}
		if (!QuickSlottedMatch)
		{
			QuickSlottedMatch = &Entry;
		}
	}

	return bAllowQuickSlotted ? QuickSlottedMatch : nullptr;
}

void FRPGInventoryList::MarkEntryChanged(FRPGInventoryEntry& Entry)
{
	MarkItemDirty(Entry);

	if (OwnerComponent->GetOwner()->HasAuthority())
	{
		BroadcastEntryUpdate(Entry, true);
	}
}

void FRPGInventoryList::RelocateQuickSlotOccupant(FRPGInventoryEntry& EntryToAssign, const FGameplayTag& QuickSlotTag)
{
	FRPGInventoryEntry* ExistingOccupant = GetAlreadyQuickSlottedEntry(QuickSlotTag);
	if (!ExistingOccupant || ExistingOccupant == &EntryToAssign)
	{
		return;
	}

	ExistingOccupant->bIsQuickSlotted = EntryToAssign.bIsQuickSlotted;
	ExistingOccupant->QuickSlotTag = EntryToAssign.QuickSlotTag;

	if (!ExistingOccupant->bIsQuickSlotted)
	{
		if (ExistingOccupant->bIsUsed)
		{
			OwnerComponent->EquipmentItemUnequippedDelegate.Broadcast(ExistingOccupant->Item.ItemID);
		}

		OwnerComponent->RemovePreloadedItemRef(ExistingOccupant->Item.ItemID);
	}

	QuickSlotItemRelocatedDelegate.Broadcast(*ExistingOccupant);
}

void FRPGInventoryList::ReleaseQuickSlotOnRemoval(FRPGInventoryEntry& Entry)
{
	if (!Entry.bIsQuickSlotted)
	{
		return;
	}

	Entry.bIsQuickSlotted = false;
	Entry.QuickSlotTag = FGameplayTag();
	QuickSlotItemRelocatedDelegate.Broadcast(Entry);
}

void FRPGInventoryList::BroadcastNewEntry(FRPGInventoryEntry& NewEntry)
{
	MarkItemDirty(NewEntry);

	if (OwnerComponent->GetOwner()->HasAuthority())
	{
		BroadcastEntryUpdate(NewEntry, true);
	}
}

void FRPGInventoryList::BroadcastEntryUpdate(const FRPGInventoryEntry& Entry, bool bChanged)
{
	if (Entry.bIsQuickSlotted)
	{
		if (bChanged)
		{
			QuickSlotItemChangeDelegate.Broadcast(Entry);
		}
		else
		{
			QuickSlotItemRemovedDelegate.Broadcast(Entry.Item.ItemID);
		}
	}
	else
	{
		if (bChanged)
		{
			InventoryItemChangedDelegate.Broadcast(Entry);
		}
		else
		{
			InventoryItemRemovedDelegate.Broadcast(Entry.Item.ItemID);
		}
	}
}


void FRPGInventoryList::RemoveItem(const FRPGInventoryEntry& InventoryEntry, int32 NumItems)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FRPGInventoryEntry& Entry = *EntryIt;

		if (Entry.Item.ItemID == InventoryEntry.Item.ItemID)
		{
			Entry.Item.Quantity = Entry.Item.Quantity - NumItems;

			if (Entry.Item.Quantity <= 0)
			{
				ReleaseQuickSlotOnRemoval(Entry);
				BroadcastEntryUpdate(Entry, false);
				if (IsValid(OwnerComponent))
				{
					OwnerComponent->RemovePreloadedItemRef(Entry.Item.ItemID);
				}
				EntryIt.RemoveCurrent();
				MarkArrayDirty();
			}
			else
			{
				MarkEntryChanged(Entry);
			}
			break;
		}
	}
}

FRPGInventoryEntry* FRPGInventoryList::FindEntryByID(int64 ItemID)
{
	return Entries.FindByPredicate([ItemID](const FRPGInventoryEntry& Entry)
	{
		return Entry.Item.ItemID == ItemID;
	});
}

const FRPGInventoryEntry* FRPGInventoryList::FindEntryByID(int64 ItemID) const
{
	return Entries.FindByPredicate([ItemID](const FRPGInventoryEntry& Entry)
	{
		return Entry.Item.ItemID == ItemID;
	});
}

bool FRPGInventoryList::HasEnough(int64 ItemID, int32 NumItems) const
{
	for (auto EntryIt = Entries.CreateConstIterator(); EntryIt; ++EntryIt)
	{
		const FRPGInventoryEntry& Entry = *EntryIt;

		if (Entry.Item.ItemID == ItemID)
		{
			if (Entry.Item.Quantity >= NumItems)
			{
				return true;
			}
			return false;
		}
	}

	return false;
}

FRPGInventoryEntry* FRPGInventoryList::GetAlreadyQuickSlottedEntry(const FGameplayTag& QuickSlotTag)
{
	AActor* Owner = OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
	if (!IsValid(Owner) || !Owner->Implements<UQuickSlotInterface>())
	{
		return nullptr;
	}

	const UQuickSlotManagerComponent* QuickSlotManagerComponent = IQuickSlotInterface::Execute_GetQuickSlotManagerComponent(Owner);
	if (!QuickSlotManagerComponent)
	{
		return nullptr;
	}

	const int64 AlreadySlottedItemID = QuickSlotManagerComponent->GetQuickSlotItemID(QuickSlotTag);
	
	if (AlreadySlottedItemID == 0)
	{
		return nullptr;
	}
	return FindEntryByID(AlreadySlottedItemID);
}

void FRPGInventoryList::AddEntryToQuickSlot(int64 ItemID, const FGameplayTag& QuickSlotTag)
{
	if (ItemID == 0 || !QuickSlotTag.IsValid())
	{
		return;
	}

	FRPGInventoryEntry* Entry = FindEntryByID(ItemID);
	if (Entry)
	{
		RelocateQuickSlotOccupant(*Entry, QuickSlotTag);

		Entry->bIsQuickSlotted = true;
		Entry->QuickSlotTag = QuickSlotTag;
		QuickSlotItemRelocatedDelegate.Broadcast(*Entry);

		if (QuickSlotTag.MatchesTag(MKHGameplayTags::Input::WeaponQuickSlotCategory))
		{
			OwnerComponent->PreloadItem(Entry);
		}
	}
}

void FRPGInventoryList::RemoveEntryFromQuickSlot(const int64 ItemID)
{
	if (FRPGInventoryEntry* Entry = FindEntryByID(ItemID))
	{
		if (Entry->bIsQuickSlotted)
		{
			if (Entry->bIsUsed)
			{
				OwnerComponent->EquipmentItemUnequippedDelegate.Broadcast(ItemID);
			}
			Entry->bIsQuickSlotted = false;
			Entry->QuickSlotTag = FGameplayTag();
			QuickSlotItemRelocatedDelegate.Broadcast(*Entry);

			OwnerComponent->RemovePreloadedItemRef(ItemID);
		}
	}
}

void FRPGInventoryList::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	for (const int32 Index : RemovedIndices)
	{
		ReleaseQuickSlotOnRemoval(Entries[Index]);
		BroadcastEntryUpdate(Entries[Index], false);

		// A removed entry (dropped, used up) no longer needs its quick slot preload on the owning client.
		if (IsValid(OwnerComponent))
		{
			OwnerComponent->RemovePreloadedItemRef(Entries[Index].Item.ItemID);
		}
	}
}

void FRPGInventoryList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	for (const int32 Index : AddedIndices)
	{
		BroadcastEntryUpdate(Entries[Index], true);

		// A split stack whose quick slot assignment arrived before the entry itself.
		if (IsValid(OwnerComponent))
		{
			OwnerComponent->ApplyPendingQuickSlotAssignment(Entries[Index].Item.ItemID);
		}
	}
}

void FRPGInventoryList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	for (const int32 Index : ChangedIndices)
	{
		BroadcastEntryUpdate(Entries[Index], true);
	}
}

UInventoryComponent::UInventoryComponent() : InventoryList(this)
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInventoryComponent, InventoryList);
}

void UInventoryComponent::AddItem(const FGameplayTag& ItemTag, int32 NumItems)
{
	if (!HasInventoryAuthority())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::AddItem: ignored '%s', adding items is server only."), *ItemTag.ToString());
		return;
	}

	UMKHItemSubsystem* ItemSubsystem = UMKHItemSubsystem::Get(this);
	if (!IsValid(ItemSubsystem))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::AddItem: UMKHItemSubsystem unavailable, '%s' not added."), *ItemTag.ToString());
		return;
	}

	InventoryList.AddItemEntry(ItemSubsystem->GenerateItem(ItemTag, NumItems));
}

void UInventoryComponent::GrantStartingLoadout(const TArray<FMKHStartingItem>& Loadout)
{
	UMKHItemSubsystem* ItemSubsystem = UMKHItemSubsystem::Get(this);
	if (!HasInventoryAuthority() || !IsValid(ItemSubsystem))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::GrantStartingLoadout: ignored, server only and needs UMKHItemSubsystem."));
		return;
	}

	TArray<int64> GrantedItemIDs;
	for (const FMKHStartingItem& StartingItem : Loadout)
	{
		// Equipment is one item per entry; consumables arrive as a single stack.
		const bool bIsEquipment = StartingItem.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Equipment);
		const int32 NumEntries = bIsEquipment ? StartingItem.Quantity : 1;
		const int32 EntryQuantity = bIsEquipment ? 1 : StartingItem.Quantity;

		for (int32 Index = 0; Index < NumEntries; ++Index)
		{
			const FMKHItemEntry Item = ItemSubsystem->GenerateItem(StartingItem.ItemTag, EntryQuantity, StartingItem.RarityRule);
			if (!Item.IsValid())
			{
				continue;
			}

			InventoryList.AddItemEntry(Item);
			GrantedItemIDs.Add(Item.ItemID);
		}
	}

	OnStartingLoadoutGranted.Broadcast(GrantedItemIDs);
}

void UInventoryComponent::UseItem(int64 ItemID, int32 NumItems)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}
		
	if (!InventoryList.HasEnough(ItemID, NumItems))
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		ServerUseItem(ItemID, NumItems);
		return;
	}

	FRPGInventoryEntry* Entry = InventoryList.FindEntryByID(ItemID);
	if (!Entry || Entry->bIsUsed)
	{
		return;
	}

	const FMasterItemDefinition* ItemDef = FindItemDefinition(Entry->Item.ItemTag);
	if (!ItemDef)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::UseItem: no definition for '%s'."), *Entry->Item.ItemTag.ToString());
		return;
	}

	if (UAbilitySystemComponent* OwnerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner))
	{
		TryUseConsumable(Entry, *ItemDef, OwnerASC, NumItems);
	}

	TryUseEquipment(Entry, *ItemDef);
}

void UInventoryComponent::RemoveItem(int64 ItemID, int32 NumItems)
{
	if (!HasInventoryAuthority())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::RemoveItem: ignored item %lld, removing items is server only."), ItemID);
		return;
	}

	if (NumItems < 1)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::RemoveItem: invalid quantity %d for item %lld."), NumItems, ItemID);
		return;
	}

	// Equipped armor is not in the inventory: releasing it brings it back so it can be removed.
	const FRPGInventoryEntry* Entry = InventoryList.FindEntryByID(ItemID);
	if (!Entry || NumItems >= Entry->Item.Quantity)
	{
		ReleaseItemBindings(ItemID);
		Entry = InventoryList.FindEntryByID(ItemID);
	}

	if (!Entry)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::RemoveItem: item %lld not found."), ItemID);
		return;
	}

	InventoryList.RemoveItem(*Entry, NumItems);
}

void UInventoryComponent::DropItem(int64 ItemID, int32 Quantity)
{
	if (ItemID <= 0)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::DropItem: invalid item ID %lld."), ItemID);
		return;
	}

	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		ServerDropItem(ItemID, Quantity);
		return;
	}

	DropItemOnServer(ItemID, Quantity);
}

void UInventoryComponent::ResetInventory()
{
	if (!HasInventoryAuthority())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::ResetInventory: ignored, resetting the inventory is server only."));
		return;
	}

	// Equipped armor is not in the bag: RemoveItem unequips it first, like every quick slot binding.
	for (const int64 ItemID : CollectOwnedItemIDs())
	{
		const FRPGInventoryEntry* Entry = InventoryList.FindEntryByID(ItemID);
		RemoveItem(ItemID, Entry ? Entry->Item.Quantity : 1);
	}

	PendingQuickSlotAssignments.Reset();
}

void UInventoryComponent::TryUseConsumable(FRPGInventoryEntry* Entry, const FMasterItemDefinition& ItemDef, UAbilitySystemComponent* OwnerASC, int32 NumItems)
{
	if (!IsValid(ItemDef.ConsumableProps.ItemEffectClass))
	{
		return;
	}

	if (IsConsumableOnCooldown(ItemDef.ConsumableProps, OwnerASC))
	{
		return;
	}

	const FGameplayEffectContextHandle EffectContext = OwnerASC->MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle = OwnerASC->MakeOutgoingSpec(
		ItemDef.ConsumableProps.ItemEffectClass,
		ItemDef.ConsumableProps.ItemEffectLevel,
		EffectContext);

	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	OwnerASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	ApplyConsumableCooldown(ItemDef.ConsumableProps, OwnerASC);

	// Through RemoveItem so the last one also frees its quick slot on the server (remote owners included).
	RemoveItem(Entry->Item.ItemID, 1);
}

bool UInventoryComponent::IsConsumableOnCooldown(const FConsumableProps& ConsumableProps, const UAbilitySystemComponent* OwnerASC) const
{
	if (!ConsumableProps.CooldownTag.IsValid() || !IsValid(OwnerASC))
	{
		return false;
	}

	return OwnerASC->HasMatchingGameplayTag(ConsumableProps.CooldownTag);
}

void UInventoryComponent::ApplyConsumableCooldown(const FConsumableProps& ConsumableProps, UAbilitySystemComponent* OwnerASC) const
{
	if (!ConsumableProps.CooldownTag.IsValid() || ConsumableProps.CooldownTime <= 0.f || !IsValid(OwnerASC))
	{
		return;
	}

	UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(GetWorld());
	if (!IsValid(GameInstance))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("ApplyConsumableCooldown: missing MKHGameInstance, cooldown not applied."));
		return;
	}

	const TSubclassOf<UGameplayEffect> CooldownEffect = UGenericClassReference::GetGameplayEffectByName(
		GameInstance->GetGenericClassReference(),
		UGenericClassReference::SkillCooldownEffectKey);
	if (!IsValid(CooldownEffect))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("ApplyConsumableCooldown: generic cooldown effect not found, cooldown not applied."));
		return;
	}

	const FGameplayEffectContextHandle EffectContext = OwnerASC->MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle = OwnerASC->MakeOutgoingSpec(CooldownEffect, 1.f, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	// Same pattern as skill cooldowns: duration via set-by-caller, cooldown tag granted dynamically
	// so the HUD can track it through GetCooldownRemainingForTag.
	SpecHandle.Data->SetSetByCallerMagnitude(MKHGameplayTags::Combat::Data_AbilityCooldownTime, ConsumableProps.CooldownTime);
	SpecHandle.Data->DynamicGrantedTags.AddTag(ConsumableProps.CooldownTag);

	OwnerASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UInventoryComponent::TryUseEquipment(FRPGInventoryEntry* Entry, const FMasterItemDefinition& ItemDef)
{
	if (!Entry || !IsValid(ItemDef.EquipmentItemProps.EquipmentClass))
	{
		return;
	}

	// Copy before broadcasting: equipping may return the previously equipped item to the
	// inventory, which can reallocate the entries array and leave Entry dangling.
	const FMKHItemEntry UsedItem = Entry->Item;
	EquipmentItemUsedDelegate.Broadcast(UsedItem);

	FRPGInventoryEntry* UsedEntry = InventoryList.FindEntryByID(UsedItem.ItemID);
	if (!UsedEntry)
	{
		return;
	}

	// Keep weapon entries in inventory and mark them as used to support proper unequip flow.
	if (UsedItem.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Weapon))
	{
		MarkWeaponEntryUsed(*UsedEntry);
	}
	else
	{
		InventoryList.RemoveItem(*UsedEntry, 1);
	}
}

void UInventoryComponent::MarkWeaponEntryUsed(FRPGInventoryEntry& Entry)
{
	Entry.bIsUsed = true;
	InventoryList.MarkEntryChanged(Entry);
}

void UInventoryComponent::ServerUseItem_Implementation(int64 ItemID, int32 NumItems)
{
	UseItem(ItemID, NumItems);
}

bool UInventoryComponent::ServerUseItem_Validate(int64 ItemID, int32 NumItems)
{
	return InventoryList.HasEnough(ItemID, NumItems);
}

void UInventoryComponent::ServerDropItem_Implementation(int64 ItemID, int32 Quantity)
{
	DropItem(ItemID, Quantity);
}

bool UInventoryComponent::ServerDropItem_Validate(int64 ItemID, int32 Quantity)
{
	// Only malformed requests kick the client; a missing item (race) is rejected softly by DropItemOnServer.
	return ItemID > 0;
}

bool UInventoryComponent::HasInventoryAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}

void UInventoryComponent::DropItemOnServer(int64 ItemID, int32 Quantity)
{
	APawn* Pawn = GetOwnerPawn();
	FTransform DropTransform;
	if (!IsValid(Pawn) || !ComputeDropTransform(*Pawn, DropTransform))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::DropItem: item %lld not dropped, the owner has no pawn."), ItemID);
		return;
	}

	// Equipped armor lives in the equipment: unequipping returns it to the inventory first.
	UnequipIfEquipped(ItemID);

	const FRPGInventoryEntry* Entry = InventoryList.FindEntryByID(ItemID);
	if (!Entry)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::DropItem: item %lld not found."), ItemID);
		return;
	}

	const FMKHItemEntry DroppedItem = MakeDroppedItem(Entry->Item, Quantity);
	if (!DroppedItem.IsValid())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::DropItem: could not split item %lld."), ItemID);
		return;
	}

	if (!IsValid(AMKHPickupItem::SpawnPickupFromItem(this, DroppedItem, DropTransform, nullptr, EMKHPickupAppearance::Drop, Pawn)))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::DropItem: pickup spawn failed, item %lld kept."), ItemID);
		return;
	}

	RemoveItem(ItemID, DroppedItem.Quantity);
}

FMKHItemEntry UInventoryComponent::MakeDroppedItem(const FMKHItemEntry& StoredItem, int32 RequestedQuantity) const
{
	FMKHItemEntry DroppedItem = StoredItem;

	const bool bWholeStack = RequestedQuantity <= 0
		|| RequestedQuantity >= StoredItem.Quantity
		|| StoredItem.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Equipment);
	if (bWholeStack)
	{
		return DroppedItem;
	}

	// The dropped part becomes a new item instance; the stack left in the inventory keeps its ID.
	UMKHItemSubsystem* ItemSubsystem = UMKHItemSubsystem::Get(this);
	DroppedItem.ItemID = IsValid(ItemSubsystem) ? ItemSubsystem->GenerateItemID() : 0;
	DroppedItem.Quantity = RequestedQuantity;
	return DroppedItem;
}

bool UInventoryComponent::ComputeDropTransform(const APawn& Pawn, FTransform& OutTransform) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const FRotator Facing(0.f, Pawn.GetActorRotation().Yaw, 0.f);
	const FVector PawnLocation = Pawn.GetActorLocation();
	// The pawn's attached equipment (weapon meshes) must not stop the traces.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InventoryDropItem), false, &Pawn);
	TArray<AActor*> AttachedActors;
	Pawn.GetAttachedActors(AttachedActors, true, true);
	QueryParams.AddIgnoredActors(AttachedActors);

	const FVector2D Scatter = FMath::RandPointInCircle(DropScatterRadius);
	const FVector Target = PawnLocation + Facing.Vector() * DropDistance + FVector(Scatter.X, Scatter.Y, 0.f);
	const FVector DropLocation = AMKHPickupItem::FindRestLocation(*World, PawnLocation, Target, DropWallClearance,
		DropGroundTraceDistance, PawnLocation.Z - Pawn.GetSimpleCollisionHalfHeight(), QueryParams);

	OutTransform = FTransform(Facing, DropLocation);
	return true;
}

TArray<int64> UInventoryComponent::CollectOwnedItemIDs() const
{
	TArray<int64> ItemIDs;
	for (const FRPGInventoryEntry& Entry : InventoryList.Entries)
	{
		ItemIDs.AddUnique(Entry.Item.ItemID);
	}

	AActor* Owner = GetOwner();
	UEquipmentManagerComponent* Equipment = (IsValid(Owner) && Owner->Implements<UEquipmentInterface>())
		? IEquipmentInterface::Execute_GetEquipmentManagerComponent(Owner)
		: nullptr;
	if (IsValid(Equipment))
	{
		TArray<FRPGEquipmentEntry> EquippedEntries;
		Equipment->EquipmentList.GetEntries(EquippedEntries);
		for (const FRPGEquipmentEntry& Entry : EquippedEntries)
		{
			ItemIDs.AddUnique(Entry.Item.ItemID);
		}
	}
	return ItemIDs;
}

APawn* UInventoryComponent::GetOwnerPawn() const
{
	const AController* Controller = Cast<AController>(GetOwner());
	return IsValid(Controller) ? Controller->GetPawn() : nullptr;
}

void UInventoryComponent::ReleaseItemBindings(int64 ItemID)
{
	UnequipIfEquipped(ItemID);
	ClearQuickSlotBinding(ItemID);
}

void UInventoryComponent::UnequipIfEquipped(int64 ItemID)
{
	// Handled by the owner's equipment (no-op when it does not hold the item); it returns the item here.
	EquipmentItemUnequippedDelegate.Broadcast(ItemID);
}

void UInventoryComponent::ClearQuickSlotBinding(int64 ItemID)
{
	// Entry flags are only set where the quick slot was assigned (the listen-server host here).
	InventoryList.RemoveEntryFromQuickSlot(ItemID);

	// Remote owners mirror their quick slots on the server manager, which replicates the removal back.
	if (UQuickSlotManagerComponent* QuickSlotManager = FindQuickSlotManager(); IsValid(QuickSlotManager))
	{
		QuickSlotManager->RemoveQuickSlot(ItemID);
	}
}

bool UInventoryComponent::CanSwapWeaponSkills(int64 ItemID, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot) const
{
	const FRPGInventoryEntry* Entry = InventoryList.FindEntryByID(ItemID);
	if (Entry == nullptr || FromSlot == ToSlot || !FMKHSkillLayout::IsSkillSlot(ToSlot)
		|| FMKHSkillLayout::FindSkillInSlot(Entry->Item.EffectPackage, FromSlot) == nullptr)
	{
		return false;
	}

	// A running skill keeps its input until it ends: its release must reach the same ability.
	const UEquipmentManagerComponent* Equipment = FindEquipmentManager();
	return !IsValid(Equipment) || !Equipment->IsItemSkillActive(ItemID, Entry->Item.EffectPackage, FromSlot, ToSlot);
}

void UInventoryComponent::SwapWeaponSkills(int64 ItemID, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot)
{
	if (!CanSwapWeaponSkills(ItemID, FromSlot, ToSlot))
	{
		return;
	}

	if (!HasInventoryAuthority())
	{
		ServerSwapWeaponSkills(ItemID, FromSlot, ToSlot);
		return;
	}

	FRPGInventoryEntry* Entry = InventoryList.FindEntryByID(ItemID);
	if (Entry != nullptr && FMKHSkillLayout::SwapSlots(Entry->Item.EffectPackage, FromSlot, ToSlot))
	{
		LastArrangedItemID = ItemID;
		PublishWeaponSkillChange(*Entry);
	}
}

void UInventoryComponent::SetSkillLayout(const TArray<FMKHSkillSlotPreference>& InSkillLayout)
{
	SkillLayout = InSkillLayout;
	if (!HasInventoryAuthority())
	{
		return;
	}

	for (const FRPGInventoryEntry& Entry : GetInventoryEntries())
	{
		FRPGInventoryEntry* Stored = InventoryList.FindEntryByID(Entry.Item.ItemID);
		if (Stored != nullptr && Stored->Item.ItemID != LastArrangedItemID && !IsAnySkillOfItemActive(Stored->Item)
			&& FMKHSkillLayout::ApplyPreferences(Stored->Item.EffectPackage, SkillLayout))
		{
			PublishWeaponSkillChange(*Stored);
		}
	}
}

bool UInventoryComponent::IsAnySkillOfItemActive(const FMKHItemEntry& Item) const
{
	const UEquipmentManagerComponent* Equipment = FindEquipmentManager();
	if (!IsValid(Equipment))
	{
		return false;
	}

	const TConstArrayView<FGameplayTag> Slots = FMKHSkillLayout::GetSkillSlots();
	return Equipment->IsItemSkillActive(Item.ItemID, Item.EffectPackage, Slots[0], Slots[1])
		|| Equipment->IsItemSkillActive(Item.ItemID, Item.EffectPackage, Slots[2], Slots[2]);
}

void UInventoryComponent::PublishWeaponSkillChange(FRPGInventoryEntry& Entry)
{
	if (UEquipmentManagerComponent* Equipment = FindEquipmentManager(); IsValid(Equipment))
	{
		Equipment->ApplyItemSkillLayout(Entry.Item);
	}
	InventoryList.MarkEntryChanged(Entry);
}

void UInventoryComponent::ServerSwapWeaponSkills_Implementation(int64 ItemID, FGameplayTag FromSlot, FGameplayTag ToSlot)
{
	SwapWeaponSkills(ItemID, FromSlot, ToSlot);
}

bool UInventoryComponent::ServerSwapWeaponSkills_Validate(int64 ItemID, FGameplayTag FromSlot, FGameplayTag ToSlot)
{
	return ItemID != 0 && FMKHSkillLayout::IsSkillSlot(FromSlot) && FMKHSkillLayout::IsSkillSlot(ToSlot);
}

UEquipmentManagerComponent* UInventoryComponent::FindEquipmentManager() const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UEquipmentInterface>())
	{
		return nullptr;
	}
	return IEquipmentInterface::Execute_GetEquipmentManagerComponent(Owner);
}

UQuickSlotManagerComponent* UInventoryComponent::FindQuickSlotManager() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UQuickSlotInterface>())
	{
		return nullptr;
	}
	return IQuickSlotInterface::Execute_GetQuickSlotManagerComponent(Owner);
}

bool UInventoryComponent::IsLocallyOwned() const
{
	const APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	return IsValid(OwnerController) && OwnerController->IsLocalController();
}

void UInventoryComponent::SplitStackOnServer(int64 ItemID, int32 Quantity, const FGameplayTag& QuickSlotTag)
{
	FRPGInventoryEntry* Source = InventoryList.FindEntryByID(ItemID);
	if (!Source || !Source->Item.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Consumable))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::SplitStack: item %lld is not a consumable stack in the inventory."), ItemID);
		return;
	}

	if (Quantity >= Source->Item.Quantity)
	{
		AssignQuickSlotOnOwner(ItemID, QuickSlotTag);
		return;
	}

	UMKHItemSubsystem* ItemSubsystem = UMKHItemSubsystem::Get(this);
	FMKHItemEntry SplitItem = Source->Item;
	SplitItem.ItemID = IsValid(ItemSubsystem) ? ItemSubsystem->GenerateItemID() : 0;
	SplitItem.Quantity = Quantity;
	if (!SplitItem.IsValid())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::SplitStack: could not generate an ItemID for the split of %lld."), ItemID);
		return;
	}

	Source->Item.Quantity -= Quantity;
	InventoryList.MarkEntryChanged(*Source);
	InventoryList.AddSeparateEntry(SplitItem);
	AssignQuickSlotOnOwner(SplitItem.ItemID, QuickSlotTag);
}

void UInventoryComponent::AssignQuickSlotOnOwner(int64 ItemID, const FGameplayTag& QuickSlotTag)
{
	if (IsLocallyOwned())
	{
		AddEntryToQuickSlot(ItemID, QuickSlotTag);
		return;
	}
	ClientAssignQuickSlot(ItemID, QuickSlotTag);
}

void UInventoryComponent::ClientAssignQuickSlot_Implementation(int64 ItemID, FGameplayTag QuickSlotTag)
{
	if (InventoryList.FindEntryByID(ItemID))
	{
		AddEntryToQuickSlot(ItemID, QuickSlotTag);
		return;
	}
	PendingQuickSlotAssignments.Add(ItemID, QuickSlotTag);
}

void UInventoryComponent::ApplyPendingQuickSlotAssignment(int64 ItemID)
{
	FGameplayTag QuickSlotTag;
	if (PendingQuickSlotAssignments.RemoveAndCopyValue(ItemID, QuickSlotTag))
	{
		AddEntryToQuickSlot(ItemID, QuickSlotTag);
	}
}

void UInventoryComponent::RequestRestack(int64 ItemID)
{
	const FRPGInventoryEntry* Entry = InventoryList.FindEntryByID(ItemID);
	if (!Entry || Entry->bIsQuickSlotted || Entry->Item.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Equipment))
	{
		return;
	}

	if (!HasInventoryAuthority())
	{
		ServerRestackItem(ItemID);
		return;
	}
	RestackOnServer(ItemID);
}

void UInventoryComponent::RestackOnServer(int64 ItemID)
{
	const FRPGInventoryEntry* Source = InventoryList.FindEntryByID(ItemID);
	if (!Source || IsItemQuickSlotted(ItemID) || Source->Item.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Equipment))
	{
		return;
	}

	FRPGInventoryEntry* Target = InventoryList.FindStackTarget(Source->Item.ItemTag, ItemID, false);
	if (!Target)
	{
		return;
	}

	const int32 Quantity = Source->Item.Quantity;
	Target->Item.Quantity += Quantity;
	InventoryList.MarkEntryChanged(*Target);
	InventoryList.RemoveItem(*Source, Quantity);
}

void UInventoryComponent::ServerSplitStackToQuickSlot_Implementation(int64 ItemID, int32 Quantity, FGameplayTag QuickSlotTag)
{
	SplitStackOnServer(ItemID, Quantity, QuickSlotTag);
}

bool UInventoryComponent::ServerSplitStackToQuickSlot_Validate(int64 ItemID, int32 Quantity, FGameplayTag QuickSlotTag)
{
	return ItemID > 0 && Quantity >= 1 && QuickSlotTag.MatchesTag(MKHGameplayTags::Input::ConsumableQuickSlotCategory);
}

void UInventoryComponent::ServerRestackItem_Implementation(int64 ItemID)
{
	RestackOnServer(ItemID);
}

bool UInventoryComponent::ServerRestackItem_Validate(int64 ItemID)
{
	return ItemID > 0;
}

const FMasterItemDefinition* UInventoryComponent::FindItemDefinition(const FGameplayTag& ItemTag) const
{
	const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(this);
	return IsValid(DataSubsystem) ? DataSubsystem->FindItemDefinition(ItemTag) : nullptr;
}

FRPGInventoryEntry UInventoryComponent::FindInventoryEntryByID(int64 ItemID) const
{
	if (const FRPGInventoryEntry* Found = InventoryList.FindEntryByID(ItemID))
	{
		return *Found;
	}
	return FRPGInventoryEntry();
}

bool UInventoryComponent::HasEnough(int64 ItemID, int32 NumItems) const
{
	return InventoryList.HasEnough(ItemID, NumItems);
}

FGameplayTag UInventoryComponent::GetConsumableCooldownTag(int64 ItemID) const
{
	const FRPGInventoryEntry* Entry = InventoryList.FindEntryByID(ItemID);
	if (!Entry)
	{
		return FGameplayTag();
	}

	const FMasterItemDefinition* ItemDef = FindItemDefinition(Entry->Item.ItemTag);
	return ItemDef ? ItemDef->ConsumableProps.CooldownTag : FGameplayTag();
}

TArray<FRPGInventoryEntry> UInventoryComponent::GetInventoryEntries() const
{
	return InventoryList.Entries;
}

int64 UInventoryComponent::AddItemEntry(const FMKHItemEntry& Item)
{
	// Server only: clients receive the updated inventory via replication, no RPC needed.
	if (!HasInventoryAuthority())
	{
		return 0;
	}

	return InventoryList.AddItemEntry(Item);
}

void UInventoryComponent::MoveQuantityToStack(int64 SourceItemID, int64 TargetItemID, int32 Quantity)
{
	FRPGInventoryEntry* Source = InventoryList.FindEntryByID(SourceItemID);
	FRPGInventoryEntry* Target = InventoryList.FindEntryByID(TargetItemID);
	if (!HasInventoryAuthority() || !Source || !Target || Source == Target || Quantity < 1
		|| !Source->Item.ItemTag.MatchesTagExact(Target->Item.ItemTag)
		|| Source->Item.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Equipment))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::MoveQuantityToStack: invalid move of %d from %lld to %lld."), Quantity, SourceItemID, TargetItemID);
		return;
	}

	const int32 MovedQuantity = FMath::Min(Quantity, Source->Item.Quantity);
	Target->Item.Quantity += MovedQuantity;
	InventoryList.MarkEntryChanged(*Target);
	InventoryList.RemoveItem(*Source, MovedQuantity);
}

int64 UInventoryComponent::FindQuickSlottedStackID(const FGameplayTag& ItemTag) const
{
	const FRPGInventoryEntry* Stack = InventoryList.FindQuickSlottedStack(ItemTag);
	return Stack ? Stack->Item.ItemID : 0;
}

void UInventoryComponent::AddEntryToQuickSlot(int64 ItemID, const FGameplayTag& QuickSlotTag)
{
	const FRPGInventoryEntry* Occupant = InventoryList.GetAlreadyQuickSlottedEntry(QuickSlotTag);
	const int64 OccupantID = (Occupant && Occupant->Item.ItemID != ItemID) ? Occupant->Item.ItemID : 0;

	InventoryList.AddEntryToQuickSlot(ItemID, QuickSlotTag);

	// The displaced stack goes back to its bag stack when it did not take another quick slot.
	const FRPGInventoryEntry* Displaced = OccupantID != 0 ? InventoryList.FindEntryByID(OccupantID) : nullptr;
	if (Displaced && !Displaced->bIsQuickSlotted)
	{
		RequestRestack(OccupantID);
	}
}

void UInventoryComponent::RemoveEntryFromQuickSlot(int64 ItemID)
{
	InventoryList.RemoveEntryFromQuickSlot(ItemID);
	RequestRestack(ItemID);
}

void UInventoryComponent::SplitStackToQuickSlot(int64 ItemID, int32 Quantity, FGameplayTag QuickSlotTag)
{
	const FRPGInventoryEntry* Entry = InventoryList.FindEntryByID(ItemID);
	if (!Entry || Quantity < 1 || !Entry->Item.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Consumable)
		|| !QuickSlotTag.MatchesTag(MKHGameplayTags::Input::ConsumableQuickSlotCategory))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryComponent::SplitStackToQuickSlot: invalid split of item %lld (x%d) to '%s'."),
			ItemID, Quantity, *QuickSlotTag.ToString());
		return;
	}

	// The whole stack needs no new entry: quick slots are assigned on the owning machine.
	if (Quantity >= Entry->Item.Quantity)
	{
		AddEntryToQuickSlot(ItemID, QuickSlotTag);
		return;
	}

	if (!HasInventoryAuthority())
	{
		ServerSplitStackToQuickSlot(ItemID, Quantity, QuickSlotTag);
		return;
	}

	SplitStackOnServer(ItemID, Quantity, QuickSlotTag);
}

bool UInventoryComponent::IsItemQuickSlotted(int64 ItemID) const
{
	const UQuickSlotManagerComponent* QuickSlotManager = FindQuickSlotManager();
	return IsValid(QuickSlotManager) && QuickSlotManager->IsItemQuickSlotted(ItemID);
}

void UInventoryComponent::PreloadItem(FRPGInventoryEntry* Entry)
{
	if (!Entry || PreloadedItemHandles.Contains(Entry->Item.ItemID))
	{
		return;
	}

	FStreamableManager& Manager = UAssetManager::GetStreamableManager();
	TArray<TSharedPtr<FStreamableHandle>>& Handles = PreloadedItemHandles.Add(Entry->Item.ItemID);

	const FMasterItemDefinition* ItemDef = FindItemDefinition(Entry->Item.ItemTag);
	
	if (ItemDef && ItemDef->EquipmentItemProps.EquipmentClass)
	{
		if (const UEquipmentDefinition* EquipmentCDO = GetDefault<UEquipmentDefinition>(ItemDef->EquipmentItemProps.EquipmentClass); IsValid(EquipmentCDO))
		{
			PreloadEquipmentActors(EquipmentCDO, Handles, Manager);
		}
	}
	
	PreloadAbilities(Entry->Item.EffectPackage, Handles, Manager);
	PreloadPassiveEffects(Entry->Item.EffectPackage, Handles, Manager);
}

void UInventoryComponent::PreloadEquipmentActors(const UEquipmentDefinition* EquipmentCDO, TArray<TSharedPtr<FStreamableHandle>>& Handles, FStreamableManager& Manager)
{
	if (!EquipmentCDO) return;
	
	for (const FEquipmentActorToSpawn& ActorToSpawn : EquipmentCDO->ActorsToSpawn)
	{
		if (!ActorToSpawn.EquipmentClass.IsNull())
		{
			Handles.Add(Manager.RequestAsyncLoad(ActorToSpawn.EquipmentClass.ToSoftObjectPath()));
		}
	}
}

void UInventoryComponent::PreloadAbilities(const FEquipmentEffectPackage& EffectPackage, TArray<TSharedPtr<FStreamableHandle>>& Handles, FStreamableManager& Manager)
{
	for (const FEquipmentAbilityDefinition& AbilityDef : EffectPackage.Abilities)
	{
		if (!AbilityDef.AbilityClass.IsNull())
		{
			Handles.Add(Manager.RequestAsyncLoad(AbilityDef.AbilityClass.ToSoftObjectPath()));
		}
	}
}

void UInventoryComponent::PreloadPassiveEffects(const FEquipmentEffectPackage& EffectPackage, TArray<TSharedPtr<FStreamableHandle>>& Handles, FStreamableManager& Manager)
{
	for (const FEquipmentStatEffectDefinition& StatEffectDef : EffectPackage.StatEffects)
	{
		if (!StatEffectDef.EffectClass.IsNull())
		{
			Handles.Add(Manager.RequestAsyncLoad(StatEffectDef.EffectClass.ToSoftObjectPath()));
		}
	}
}

void UInventoryComponent::RemovePreloadedItemRef(int64 ItemID)
{
	PreloadedItemHandles.Remove(ItemID);
}
