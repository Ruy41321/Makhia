// Fill out your copyright notice in the Description page of Project Settings.


#include "Equipment/EquipmentManagerComponent.h"

#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentInstance.h"
#include "Data/MKHDataSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Inventory/MKHSkillLayout.h"
#include "MKHLogChannels.h"

UMKHAbilitySystemComponent* FRPGEquipmentList::GetAbilitySystemComponent()
{
	check(OwnerComponent);
	check(OwnerComponent->GetOwner());

	return Cast<UMKHAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerComponent->GetOwner()));
}

float FRPGEquipmentEntry::GetBaseArmor() const
{
	return IsValid(EquipmentDefinition) ? GetDefault<UEquipmentDefinition>(EquipmentDefinition)->BaseArmorGiven : 0.f;
}

void FRPGEquipmentList::ApplyEntryEffects(FRPGEquipmentEntry& Entry)
{
	// Base armor is applied alongside the rolled stats, so armor pieces without rolls still need this call.
	if (Entry.HasStats() || Entry.GetBaseArmor() > 0.f)
	{
		AddEquipmentStats(&Entry);
	}

	if (Entry.HasAbility())
	{
		AddEquipmentAbility(&Entry);
	}
}

void FRPGEquipmentList::RemoveEntryEffects(FRPGEquipmentEntry& Entry)
{
	RemoveEquipmentStats(&Entry);
	RemoveEquipmentAbility(&Entry);
}

void FRPGEquipmentList::AddEquipmentStats(FRPGEquipmentEntry* Entry)
{
	if (UMKHAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->AddEquipmentEffects(Entry);
	}
}

void FRPGEquipmentList::RemoveEquipmentStats(FRPGEquipmentEntry* Entry)
{
	if (UMKHAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->RemoveEquipmentEffects(Entry);
	}
}

void FRPGEquipmentList::AddEquipmentAbility(FRPGEquipmentEntry* Entry)
{
	if (UMKHAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->AddEquipmentAbility(Entry);
	}
}

void FRPGEquipmentList::RemoveEquipmentAbility(FRPGEquipmentEntry* Entry)
{
	if (UMKHAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->RemoveEquipmentAbility(Entry);
	}
}

UEquipmentInstance* FRPGEquipmentList::AddEntry(const FRPGEquipmentEntry& InEntry)
{
	check(InEntry.EquipmentDefinition);
	check(OwnerComponent);
	check(OwnerComponent->GetOwner()->HasAuthority());

	const UEquipmentDefinition* EquipmentCDO = GetDefault<UEquipmentDefinition>(InEntry.EquipmentDefinition);
	TSubclassOf<UEquipmentInstance> InstanceType = EquipmentCDO->InstanceType;

	if (!IsValid(InstanceType))
	{
		InstanceType = UEquipmentInstance::StaticClass();
	}

	FGameplayTag SlotTag = EquipmentCDO->SlotTag.MatchesTag(MKHGameplayTags::Equip::WeaponSlot) ? MKHGameplayTags::Equip::WeaponSlot : EquipmentCDO->SlotTag;
	if (FindEntryBySlot(SlotTag) != nullptr)
	{
		// If the slot is already occupied, remove the existing entry before adding the new one.
		RemoveEntryBySlot(SlotTag);
	}

	FRPGEquipmentEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = InEntry.Item;
	NewEntry.SlotTag = SlotTag;
	NewEntry.EquipmentDefinition = InEntry.EquipmentDefinition;
	NewEntry.Instance = NewObject<UEquipmentInstance>(OwnerComponent->GetOwner(), InstanceType);

	ApplyEntryEffects(NewEntry);

	if (IsValid(NewEntry.Instance))
	{
		NewEntry.Instance->SpawnEquipmentActors(EquipmentCDO->ActorsToSpawn, EquipmentCDO->BaseDamage, EquipmentCDO->BlockStabilityPercent);
	}

	MarkItemDirty(NewEntry);
	EquipmentEntryDelegate.Broadcast(NewEntry);

	return NewEntry.Instance;
}

void FRPGEquipmentList::RemoveEntryBySlot(const FGameplayTag& SlotTag)
{
	check(OwnerComponent);

	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FRPGEquipmentEntry& Entry = *EntryIt;
		if (Entry.SlotTag.MatchesTagExact(SlotTag))
		{
			RemoveEntryEffects(Entry);

			if (IsValid(Entry.Instance))
			{
				Entry.Instance->DestroySpawnedActors();
			}

			// Broadcast the unequip event before removal so listeners can return the item to inventory.
			UnEquippedEntryDelegate.Broadcast(Entry);
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
			break;
		}
	}
}

FRPGEquipmentEntry* FRPGEquipmentList::FindEntryMutable(int64 ItemID, const FGameplayTag& SlotTag)
{
	return Entries.FindByPredicate([ItemID, &SlotTag](FRPGEquipmentEntry& Entry)
	{
		return Entry.Item.ItemID == ItemID && Entry.SlotTag.MatchesTagExact(SlotTag);
	});
}

const FRPGEquipmentEntry* FRPGEquipmentList::FindEntry(int64 ItemID, const FGameplayTag& SlotTag) const
{
	return Entries.FindByPredicate([ItemID, &SlotTag](const FRPGEquipmentEntry& Entry)
	{
		return Entry.Item.ItemID == ItemID && Entry.SlotTag.MatchesTagExact(SlotTag);
	});
}

const FRPGEquipmentEntry* FRPGEquipmentList::FindEntryByItemID(int64 ItemID) const
{
	return Entries.FindByPredicate([ItemID](const FRPGEquipmentEntry& Entry)
	{
		return Entry.Item.ItemID == ItemID;
	});
}

const FRPGEquipmentEntry* FRPGEquipmentList::FindEntryBySlot(const FGameplayTag& SlotTag) const
{
	return Entries.FindByPredicate([&SlotTag](const FRPGEquipmentEntry& Entry)
	{
		return Entry.SlotTag.MatchesTagExact(SlotTag);
	});
}

FRPGEquipmentEntry* FRPGEquipmentList::FindEntryBySlotMutable(const FGameplayTag& SlotTag)
{
	return Entries.FindByPredicate([&SlotTag](FRPGEquipmentEntry& Entry)
	{
		return Entry.SlotTag.MatchesTagExact(SlotTag);
	});
}

void FRPGEquipmentList::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	for (const int32 Index : RemovedIndices)
	{
		UnEquippedEntryDelegate.Broadcast(Entries[Index]);
	}
}

void FRPGEquipmentList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	for (const int32 Index : AddedIndices)
	{
		EquipmentEntryDelegate.Broadcast(Entries[Index]);
	}
}

void FRPGEquipmentList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	for (const int32 Index : ChangedIndices)
	{
		EquipmentEntryDelegate.Broadcast(Entries[Index]);
	}
}

UEquipmentManagerComponent::UEquipmentManagerComponent() : EquipmentList(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEquipmentManagerComponent::PrintEquipmentList() const
{
	if (!IsValid(GEngine))
	{
		return;
	}

	TArray<FRPGEquipmentEntry> Entries;
	EquipmentList.GetEntries(Entries);
	for (const FRPGEquipmentEntry& Entry : Entries)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
			FString::Printf(TEXT("Slot: %s, Item: %s"), *Entry.SlotTag.ToString(), *Entry.Item.ItemTag.ToString()));
	}
}

void UEquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEquipmentManagerComponent, EquipmentList);
}

bool UEquipmentManagerComponent::BuildEquipmentEntry(const FMKHItemEntry& Item, FRPGEquipmentEntry& OutEntry) const
{
	const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(this);
	const FMasterItemDefinition* ItemDef = IsValid(DataSubsystem) ? DataSubsystem->FindItemDefinition(Item.ItemTag) : nullptr;
	if (!ItemDef || !IsValid(ItemDef->EquipmentItemProps.EquipmentClass))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UEquipmentManagerComponent::BuildEquipmentEntry: '%s' has no equipment definition, cannot equip."),
			*Item.ItemTag.ToString());
		return false;
	}

	OutEntry.Item = Item;
	OutEntry.EquipmentDefinition = ItemDef->EquipmentItemProps.EquipmentClass;
	return true;
}

void UEquipmentManagerComponent::EquipItem(const FMKHItemEntry& Item)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UEquipmentManagerComponent::EquipItem: ignored, equipping is server only."));
		return;
	}

	FRPGEquipmentEntry NewEntry;
	if (!BuildEquipmentEntry(Item, NewEntry))
	{
		return;
	}

	if (UEquipmentInstance* Result = EquipmentList.AddEntry(NewEntry))
	{
		Result->OnEquipped();
	}

	// A weapon replacing another in hand stays one unsheathe (AddEntry swaps without UnEquipItem).
	if (WeaponEventsMuteCount == 0 && Item.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Weapon))
	{
		SendOwnerEvent(MKHGameplayTags::Event::WeaponUnsheathed);
	}
}

void UEquipmentManagerComponent::UnEquipItemByItemID(int64 ItemID)
{
	if (const FGameplayTag& Tag = GetSlotTagByItemID(ItemID); Tag.IsValid())
		UnEquipItem(Tag);
}

void UEquipmentManagerComponent::UnEquipItem(const FGameplayTag& SlotTag)
{
	if (!GetOwner()->HasAuthority())
	{
		ServerUnEquipItem(SlotTag);
		return;
	}

	// Call OnUnEquipped before removing the entry to ensure the event is broadcast with valid runtime data.
	const FRPGEquipmentEntry* FoundEntry = EquipmentList.FindEntryBySlot(SlotTag);

	if (!FoundEntry)
	{
		// Weapon sheathe with no equipped weapon is a valid no-op.
		// UE_LOG(LogTemp, Warning, TEXT("UEquipmentManagerComponent::UnEquipItem - No entry found for SlotTag: %s"), *SlotTag.ToString());
		return;
	}

	if (IsValid(FoundEntry->Instance))
	{
		FoundEntry->Instance->OnUnEquipped();
	}

	EquipmentList.RemoveEntryBySlot(SlotTag);

	if (WeaponEventsMuteCount == 0 && SlotTag.MatchesTag(MKHGameplayTags::Equip::WeaponSlot))
	{
		SendOwnerEvent(MKHGameplayTags::Event::WeaponSheathed);
	}
}

void UEquipmentManagerComponent::SendOwnerEvent(const FGameplayTag& EventTag) const
{
	UMKHAbilitySystemComponent* AbilitySystem = FindOwnerAbilitySystem();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = AbilitySystem->GetAvatarActor();
	Payload.Target = Payload.Instigator;
	AbilitySystem->HandleGameplayEvent(EventTag, &Payload);
}

const FGameplayTag& UEquipmentManagerComponent::GetSlotTagByItemID(int64 ItemID) const
{
	if (const FRPGEquipmentEntry* FoundEntry = EquipmentList.FindEntryByItemID(ItemID))
	{
		return FoundEntry->SlotTag;
	}

	return FGameplayTag::EmptyTag;
}

UEquipmentInstance* UEquipmentManagerComponent::GetEquipmentInstanceBySlot(const FGameplayTag& SlotTag) const
{	
	const FRPGEquipmentEntry* FoundEntry = EquipmentList.FindEntryBySlot(SlotTag);
	return (FoundEntry) ? FoundEntry->Instance : nullptr;
}

FRPGEquipmentEntry* UEquipmentManagerComponent::GetEquipmentEntryBySlot(const FGameplayTag& SlotTag) const
{
	return const_cast<FRPGEquipmentEntry*>(EquipmentList.FindEntryBySlot(SlotTag));
}

void UEquipmentManagerComponent::HideEquippedWeaponMesh(const bool bHide) const
{
	if (const UEquipmentInstance* WeaponInstance = GetEquipmentInstanceBySlot(MKHGameplayTags::Equip::WeaponSlot))
	{
		for (const TObjectPtr<AActor>& SpawnedActor : WeaponInstance->GetSpawnedActors())
		{
			if (IsValid(SpawnedActor))
			{
				SpawnedActor->SetActorHiddenInGame(bHide);
			}
		}
	}
}

bool UEquipmentManagerComponent::IsItemSkillActive(int64 ItemID, const FEquipmentEffectPackage& Package, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot) const
{
	const UMKHAbilitySystemComponent* ASC = FindOwnerAbilitySystem();
	if (!IsValid(ASC) || EquipmentList.FindEntryByItemID(ItemID) == nullptr)
	{
		return false;
	}

	for (const FGameplayTag& SlotTag : { FromSlot, ToSlot })
	{
		const FEquipmentAbilityDefinition* Skill = FMKHSkillLayout::FindSkillInSlot(Package, SlotTag);
		const FGameplayAbilitySpec* Spec = Skill ? ASC->FindAbilitySpecForAbilityTag(Skill->AbilityTag) : nullptr;
		if (Spec != nullptr && Spec->IsActive())
		{
			return true;
		}
	}
	return false;
}

void UEquipmentManagerComponent::ApplyItemSkillLayout(const FMKHItemEntry& Item)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	const FRPGEquipmentEntry* Found = EquipmentList.FindEntryByItemID(Item.ItemID);
	FRPGEquipmentEntry* Entry = Found ? EquipmentList.FindEntryMutable(Item.ItemID, Found->SlotTag) : nullptr;
	if (Entry == nullptr)
	{
		return;
	}

	Entry->Item.EffectPackage = Item.EffectPackage;
	if (UMKHAbilitySystemComponent* ASC = FindOwnerAbilitySystem())
	{
		ASC->RefreshEquippedSkillInputs(Entry->Item.EffectPackage);
	}
	EquipmentList.MarkItemDirty(*Entry);
}

UMKHAbilitySystemComponent* UEquipmentManagerComponent::FindOwnerAbilitySystem() const
{
	return Cast<UMKHAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
}

void UEquipmentManagerComponent::ServerUnEquipItem_Implementation(FGameplayTag SlotTag)
{
	UnEquipItem(SlotTag);
}