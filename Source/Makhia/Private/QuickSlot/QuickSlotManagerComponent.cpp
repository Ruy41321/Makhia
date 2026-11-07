// Fill out your copyright notice in the Description page of Project Settings.

#include "QuickSlot/QuickSlotManagerComponent.h"

#include "MKHLogChannels.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Interfaces/InventoryInterface.h"
#include "Inventory/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/MKHPlayerCharacter.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"
#include "Player/PlayerController/MKHPlayerController.h"

UQuickSlotManagerComponent::UQuickSlotManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQuickSlotManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UQuickSlotManagerComponent, QuickEquipWeaponSlotTag);
	DOREPLIFETIME(UQuickSlotManagerComponent, QuickSlotTagEntries);
	DOREPLIFETIME(UQuickSlotManagerComponent, bCanTriggerActivation);
}

void UQuickSlotManagerComponent::AddQuickSlot(const FGameplayTag& QuickSlotTag, int64 ItemID)
{
	RemoveQuickSlot(ItemID);
	QuickSlotTagMap.Add(QuickSlotTag, ItemID);
	SyncQuickSlotEntriesFromMap();
	OnQuickSlotsChanged.Broadcast();

	if (QuickSlotTag.MatchesTag(MKHGameplayTags::Input::WeaponQuickSlotCategory) && !QuickEquipWeaponSlotTag.IsValid())
	{
		SetQuickEquipWeaponSlotTag(QuickSlotTag);
	}

	if (AActor* Owner = GetOwner(); IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerAddQuickSlot(QuickSlotTag, ItemID);
	}
}

void UQuickSlotManagerComponent::RemoveQuickSlot(int64 ItemID)
{
	const FGameplayTag* ExistingKey = QuickSlotTagMap.FindKey(ItemID);
	if (!ExistingKey)
	{
		return;
	}

	const bool bRemovedQuickEquipWeapon = ExistingKey->MatchesTagExact(QuickEquipWeaponSlotTag);
	QuickSlotTagMap.Remove(*ExistingKey);
	SyncQuickSlotEntriesFromMap();
	OnQuickSlotsChanged.Broadcast();

	if (bRemovedQuickEquipWeapon)
	{
		RefreshQuickEquipWeaponSlotTag();
	}

	if (AActor* Owner = GetOwner(); IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerRemoveQuickSlot(ItemID);
	}
}

void UQuickSlotManagerComponent::UseQuickSlot(const FGameplayTag& QuickSlotTag)
{
	// HandleQuickSlotActivation(QuickSlotTag);

	const int64 ItemID = GetQuickSlotItemID(QuickSlotTag);
	if (!ItemID)
	{
		return;
	}

	UInventoryComponent* InventoryComponent = ResolveInventoryComponent();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("UQuickSlotManagerComponent::UseQuickSlot - InventoryComponent is invalid on %s"), *GetNameSafe(GetOwner()));
		return;
	}

	UpdateQuickEquipWeaponSlotOnUse(QuickSlotTag);
	NotifyWeaponSwapRequestedIfClient(QuickSlotTag);
	InventoryComponent->UseItem(ItemID, 1);
}

void UQuickSlotManagerComponent::NotifyWeaponSwapRequestedIfClient(const FGameplayTag& QuickSlotTag) const
{
	if (!QuickSlotTag.MatchesTag(MKHGameplayTags::Input::WeaponQuickSlotCategory))
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->HasAuthority())
	{
		return;
	}

	AMKHPlayerController* PC = Cast<AMKHPlayerController>(Owner);
	if (!IsValid(PC))
	{
		return;
	}

	if (UMKHAbilitySystemComponent* ASC = PC->GetMKHAbilitySystemComponent(); IsValid(ASC))
	{
		ASC->NotifyWeaponSwapRequested();
	}
}

int64 UQuickSlotManagerComponent::GetQuickSlotItemID(const FGameplayTag& QuickSlotTag) const
{
	if (const int64* ItemID = QuickSlotTagMap.Find(QuickSlotTag))
	{
		return *ItemID;
	}

	return 0;
}

bool UQuickSlotManagerComponent::IsItemQuickSlotted(int64 ItemID) const
{
	return ItemID != 0 && QuickSlotTagMap.FindKey(ItemID) != nullptr;
}

FGameplayTag UQuickSlotManagerComponent::FindQuickSlotTagByItemID(int64 ItemID) const
{
	const FGameplayTag* QuickSlotTag = ItemID != 0 ? QuickSlotTagMap.FindKey(ItemID) : nullptr;
	return QuickSlotTag ? *QuickSlotTag : FGameplayTag();
}

bool UQuickSlotManagerComponent::TryEquipWeapon()
{
	if (!QuickEquipWeaponSlotTag.IsValid())
	{
		return false;
	}

	const AMKHPlayerController* PC = Cast<AMKHPlayerController>(GetOwner());
	if (!PC)
	{
		return false;
	}
    
	// Ability System validation
	if (const UAbilitySystemComponent* ASC = PC->GetAbilitySystemComponent())
	{
		// Static local variable: initialized only the first time this code runs
		static FGameplayTagContainer BlockingTags;
		if (BlockingTags.IsEmpty())
		{
			BlockingTags.AddTag(MKHGameplayTags::State::Stunned);
			BlockingTags.AddTag(MKHGameplayTags::State::Staggered);
			BlockingTags.AddTag(MKHGameplayTags::State::Movement::Dead);
			BlockingTags.AddTag(MKHGameplayTags::State::Movement::Jumping);
			BlockingTags.AddTag(MKHGameplayTags::State::Movement::Falling);
		}

		if (ASC->HasAnyMatchingGameplayTags(BlockingTags))
		{
			return false;
		}
	}

	// SetCanTriggerActivation(false); // Dont trigger activation for sheathed use
	UseQuickSlot(QuickEquipWeaponSlotTag);

	return true;
}

void UQuickSlotManagerComponent::SetQuickEquipWeaponSlotTag(const FGameplayTag& NewQuickEquipWeaponSlotTag)
{
	if (QuickEquipWeaponSlotTag == NewQuickEquipWeaponSlotTag)
	{
		return;
	}

	QuickEquipWeaponSlotTag = NewQuickEquipWeaponSlotTag;
	OnQuickEquipWeaponSlotChanged.Broadcast(QuickEquipWeaponSlotTag);

	if (AActor* Owner = GetOwner(); IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerSetQuickEquipWeaponSlotTag(NewQuickEquipWeaponSlotTag);
	}
}

void UQuickSlotManagerComponent::SyncQuickSlotMapFromEntries()
{
	QuickSlotTagMap.Empty(QuickSlotTagEntries.Num());

	for (const FRPGQuickSlotEntry& Entry : QuickSlotTagEntries)
	{
		if (Entry.QuickSlotTag.IsValid() && Entry.ItemID != 0)
		{
			QuickSlotTagMap.Add(Entry.QuickSlotTag, Entry.ItemID);
		}
	}
}

void UQuickSlotManagerComponent::SyncQuickSlotEntriesFromMap()
{
	QuickSlotTagEntries.Empty(QuickSlotTagMap.Num());

	for (const TPair<FGameplayTag, int64>& Pair : QuickSlotTagMap)
	{
		FRPGQuickSlotEntry& Entry = QuickSlotTagEntries.AddDefaulted_GetRef();
		Entry.QuickSlotTag = Pair.Key;
		Entry.ItemID = Pair.Value;
	}
}
void UQuickSlotManagerComponent::SetCanTriggerActivation(const bool bNewCanTriggerActivation)
{
	if (bCanTriggerActivation == bNewCanTriggerActivation)
	{
		return;
	}

	bCanTriggerActivation = bNewCanTriggerActivation;

	if (AActor* Owner = GetOwner(); IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerSetCanTriggerActivation(bNewCanTriggerActivation);
	}
}

void UQuickSlotManagerComponent::OnRep_QuickSlotTagMap()
{
	SyncQuickSlotMapFromEntries();
	OnQuickSlotsChanged.Broadcast();
}

void UQuickSlotManagerComponent::OnRep_QuickEquipWeaponSlotTag()
{
	OnQuickEquipWeaponSlotChanged.Broadcast(QuickEquipWeaponSlotTag);
}

void UQuickSlotManagerComponent::HandleQuickSlotActivation(const FGameplayTag& QuickSlotTag)
{
	if (bCanTriggerActivation)
	{
		QuickSlotActivatedDelegate.ExecuteIfBound(QuickSlotTag);
		return;
	}

	SetCanTriggerActivation(true);
}

UInventoryComponent* UQuickSlotManagerComponent::ResolveInventoryComponent() const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UInventoryInterface>())
	{
		return nullptr;
	}

	return IInventoryInterface::Execute_GetInventoryComponent(Owner);
}

void UQuickSlotManagerComponent::UpdateQuickEquipWeaponSlotOnUse(const FGameplayTag& QuickSlotTag)
{
	// Cache the latest used weapon slot for sheathed-attack quick equip flows.
	if (QuickSlotTag.MatchesTag(MKHGameplayTags::Input::WeaponQuickSlotCategory))
	{
		SetQuickEquipWeaponSlotTag(QuickSlotTag);
	}
}

void UQuickSlotManagerComponent::RefreshQuickEquipWeaponSlotTag()
{
	if (QuickSlotTagMap.Contains(MKHGameplayTags::Equip::WeaponQuickSlot1))
	{
		SetQuickEquipWeaponSlotTag(MKHGameplayTags::Equip::WeaponQuickSlot1);
		return;
	}

	if (QuickSlotTagMap.Contains(MKHGameplayTags::Equip::WeaponQuickSlot2))
	{
		SetQuickEquipWeaponSlotTag(MKHGameplayTags::Equip::WeaponQuickSlot2);
		return;
	}

	SetQuickEquipWeaponSlotTag(FGameplayTag());
}

void UQuickSlotManagerComponent::ServerSetQuickEquipWeaponSlotTag_Implementation(FGameplayTag NewQuickEquipWeaponSlotTag)
{
	SetQuickEquipWeaponSlotTag(NewQuickEquipWeaponSlotTag);
}

void UQuickSlotManagerComponent::ServerAddQuickSlot_Implementation(const FGameplayTag& QuickSlotTag, int64 ItemID)
{
	AddQuickSlot(QuickSlotTag, ItemID);
}

void UQuickSlotManagerComponent::ServerRemoveQuickSlot_Implementation(int64 ItemID)
{
	RemoveQuickSlot(ItemID);
}

void UQuickSlotManagerComponent::ServerSetCanTriggerActivation_Implementation(bool bNewCanTriggerActivation)
{
	SetCanTriggerActivation(bNewCanTriggerActivation);
}

