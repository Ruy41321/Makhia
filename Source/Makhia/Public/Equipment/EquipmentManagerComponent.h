// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Equipment/EquipmentTypes.h"
#include "Inventory/MKHItemEntry.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "EquipmentManagerComponent.generated.h"

class UEquipmentManagerComponent;
class UEquipmentDefinition;
class UEquipmentInstance;
class UMKHAbilitySystemComponent;

/** One equipped item: the item identity plus the state that only exists while it is equipped. */
USTRUCT(BlueprintType)
struct FRPGEquipmentEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Identity of the equipped item; its ItemID is preserved across equip/unequip cycles. */
	UPROPERTY(BlueprintReadOnly)
	FMKHItemEntry Item = FMKHItemEntry();

	/** Equipment slot occupied by this entry (for example weapon or armor). */
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag SlotTag = FGameplayTag();

	/** Runtime handles granted by GAS for clean removal during unequip. */
	UPROPERTY(NotReplicated)
	FEquipmentGrantedHandles GrantedHandles = FEquipmentGrantedHandles();

	/** Returns whether this entry contains stat effects to apply. */
	bool HasStats() const { return !Item.EffectPackage.StatEffects.IsEmpty(); }

	/** Returns whether this entry contains abilities to grant. */
	bool HasAbility() const { return !Item.EffectPackage.Abilities.IsEmpty(); }

	/**
	 * Base Armor granted by this entry's definition (UEquipmentDefinition::BaseArmorGiven), independent of rolls.
	 *
	 * @return The base armor, 0 for non-armor items or when the definition is missing.
	 */
	float GetBaseArmor() const;

private:

	friend UEquipmentManagerComponent;
	friend struct FRPGEquipmentList;

	/** Equipment definition class used to resolve CDO data and instance type. */
	UPROPERTY()
	TSubclassOf<UEquipmentDefinition> EquipmentDefinition = nullptr;

	/** Non-replicated runtime instance managing spawned actors and local callbacks. */
	UPROPERTY(NotReplicated)
	TObjectPtr<UEquipmentInstance> Instance = nullptr;

};

DECLARE_MULTICAST_DELEGATE_OneParam(FEquipmentEntrySignature, const FRPGEquipmentEntry& /*Equipment Entry*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnEquippedEntrySignature, const FRPGEquipmentEntry& /*UnEquipment Entry*/);

USTRUCT()
struct FRPGEquipmentList : public FFastArraySerializer
{
	GENERATED_BODY()

	/** Default constructor required by Unreal serialization. */
	FRPGEquipmentList() : OwnerComponent() {}
	/** Constructor with owner for authority checks and ASC lookup. */
	FRPGEquipmentList(UActorComponent* InComponent) : OwnerComponent(InComponent) {}

	/** Gets the owner's Ability System Component. */
	UMKHAbilitySystemComponent* GetAbilitySystemComponent();
	/** Applies this entry's stat effects to the ASC. */
	void AddEquipmentStats(FRPGEquipmentEntry* Entry);
	/** Removes this entry's stat effects from the ASC. */
	void RemoveEquipmentStats(FRPGEquipmentEntry* Entry);
	/** Grants to the ASC the abilities defined in this entry. */
	void AddEquipmentAbility(FRPGEquipmentEntry* Entry);
	/** Revokes from the ASC the abilities granted by this entry. */
	void RemoveEquipmentAbility(FRPGEquipmentEntry* Entry);

	/** Adds a new equipped entry, handling slot replacement and side effects. */
	UEquipmentInstance* AddEntry(const FRPGEquipmentEntry& InEntry);
	/** Removes the entry currently occupying the provided slot. */
	void RemoveEntryBySlot(const FGameplayTag& SlotTag);

	/** Finds a mutable entry by ItemID and slot. */
	FRPGEquipmentEntry* FindEntryMutable(int64 ItemID, const FGameplayTag& SlotTag);
	/** Finds a const entry by ItemID and slot. */
	const FRPGEquipmentEntry* FindEntry(int64 ItemID, const FGameplayTag& SlotTag) const;
	/** Finds a const entry by ItemID. */
	const FRPGEquipmentEntry* FindEntryByItemID(int64 ItemID) const;
	/** Finds a const entry by slot. */
	const FRPGEquipmentEntry* FindEntryBySlot(const FGameplayTag& SlotTag) const;
	/** Finds a mutable entry by slot. */
	FRPGEquipmentEntry* FindEntryBySlotMutable(const FGameplayTag& SlotTag);

	/** Returns a copy of the current replicated entries. */
	void GetEntries(TArray<FRPGEquipmentEntry>& OutEntries) const { OutEntries = Entries; }

	// FFastArraySerializer Contract
	/** Handles replicated removals on clients. */
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	/** Handles replicated additions on clients. */
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	/** Handles replicated changes on clients. */
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);

	/** FastArray delta serialization entry point. */
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& Parms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRPGEquipmentEntry, FRPGEquipmentList>(Entries, Parms, *this);
	}

	/** Delegate broadcast when an entry is equipped or updated. */
	FEquipmentEntrySignature EquipmentEntryDelegate;
	/** Delegate broadcast when an entry is unequipped. */
	FOnUnEquippedEntrySignature UnEquippedEntryDelegate;

private:
	/** Applies gameplay side effects (stats and abilities) for a valid entry. */
	void ApplyEntryEffects(FRPGEquipmentEntry& Entry);
	/** Removes gameplay side effects (stats and abilities) for a valid entry. */
	void RemoveEntryEffects(FRPGEquipmentEntry& Entry);

	/** Replicated container of equipped entries. */
	UPROPERTY()
	TArray<FRPGEquipmentEntry> Entries;

	/** Serializer owner used for authority checks and gameplay services. */
	UPROPERTY()
	TObjectPtr<UActorComponent> OwnerComponent;

};

template<>
struct TStructOpsTypeTraits<FRPGEquipmentList> : public TStructOpsTypeTraitsBase2<FRPGEquipmentList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/**
 * Replicated equipment of a player, owned by the PlayerController.
 * Equipping is server only and always goes through UInventoryComponent::UseItem, which validates the item.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MAKHIA_API UEquipmentManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Equipment manager component constructor. */
	UEquipmentManagerComponent();

	/** Registers the component replicated properties. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Server only: equips an item, replacing whatever occupies its slot.
	 * The equipment definition is resolved from the item definition; calls without authority are ignored.
	 *
	 * @param Item The item to equip (its ItemID is kept by the equipment entry).
	 */
	void EquipItem(const FMKHItemEntry& Item);

	/**
	 * Returns whether a skill of an equipped weapon's two slots is running. False when the weapon is not in hand.
	 *
	 * @param ItemID   Weapon.
	 * @param Package  Abilities of the weapon.
	 * @param FromSlot First skill slot.
	 * @param ToSlot   Second skill slot.
	 */
	bool IsItemSkillActive(int64 ItemID, const FEquipmentEffectPackage& Package, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot) const;

	/**
	 * Server: when the item is equipped, copies its skill slots to the equipped copy and to the granted skills.
	 *
	 * @param Item The item, with its new skill slots.
	 */
	void ApplyItemSkillLayout(const FMKHItemEntry& Item);

	/** Unequips by ItemID by resolving its target slot. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
	void UnEquipItemByItemID(int64 ItemID);

	/** Unequips the entry currently in the given slot. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
	void UnEquipItem(const FGameplayTag& SlotTag);

	/** Returns the slot associated with an ItemID, or EmptyTag when missing. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Queries")
	const FGameplayTag& GetSlotTagByItemID(int64 ItemID) const;

	/** Returns the equipment instance associated with the slot, if any. */
	UEquipmentInstance* GetEquipmentInstanceBySlot(const FGameplayTag& SlotTag) const;

	/** Returns the equipment entry associated with the slot, if any. */
	FRPGEquipmentEntry* GetEquipmentEntryBySlot(const FGameplayTag& SlotTag) const;

	/** Hide or Show the Spawned Actor by the weapon Instance. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
	void HideEquippedWeaponMesh(const bool bHide = true) const;

	/** Debug utility that prints equipped entries on screen. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
	void PrintEquipmentList() const;

	/**
	 * Turns the Event.Weapon.Sheathed / Unsheathed events off while a loadout is rebuilt (not a gesture of the
	 * character, so no sound), and back on. Calls nest: each false needs its true.
	 *
	 * @param bEnabled  False to stop sending them, true to undo one false.
	 */
	void SetWeaponEventsEnabled(const bool bEnabled) { WeaponEventsMuteCount = FMath::Max(0, WeaponEventsMuteCount + (bEnabled ? -1 : 1)); }

private:

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Builds a replication-safe equipment entry from an item, resolving its equipment definition
	 * class through UMKHDataSubsystem. The slot is resolved later by FRPGEquipmentList::AddEntry.
	 *
	 * @param Item     The item to equip.
	 * @param OutEntry Entry filled with the item and its equipment definition.
	 * @return False when the item has no equipment definition.
	 */
	bool BuildEquipmentEntry(const FMKHItemEntry& Item, FRPGEquipmentEntry& OutEntry) const;

	/** Ability system of the owner, or nullptr. */
	UMKHAbilitySystemComponent* FindOwnerAbilitySystem() const;

	/**
	 * Sends a gameplay event to the owner's ability system (server), e.g. Event.Weapon.Sheathed for the sound effects.
	 *
	 * @param EventTag  Event to send.
	 */
	void SendOwnerEvent(const FGameplayTag& EventTag) const;

	/** Reliable server RPC for unequip requests coming from clients. */
	UFUNCTION(Server, Reliable)
	void ServerUnEquipItem(FGameplayTag SlotTag);

public:

	// ============================================================
	// Properties
	// ============================================================

	/** Replicated equipment state backed by FastArraySerializer. */
	UPROPERTY(replicated)
	FRPGEquipmentList EquipmentList;

private:
	/** Above zero while a loadout is rebuilt: weapon changes send no sheathe / unsheathe event. */
	int32 WeaponEventsMuteCount = 0;

};
