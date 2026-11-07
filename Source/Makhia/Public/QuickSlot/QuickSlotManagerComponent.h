// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "QuickSlotManagerComponent.generated.h"

/**
 * Broadcasts the gameplay tag of the quick slot that was activated by player input.
 *
 * @param QuickSlotTag The activated quick slot tag.
 */
DECLARE_DELEGATE_OneParam(FQuickSlotActivatedSignature, const FGameplayTag& /*QuickSlotTag*/);

/**
 * Broadcasts the weapon quick slot equipped by the unarmed-attack quick equip, on every change (local or replicated).
 *
 * @param QuickEquipWeaponSlotTag The new quick-equip weapon slot; invalid when no weapon is quick slotted.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FQuickEquipWeaponSlotChangedSignature, const FGameplayTag& /*QuickEquipWeaponSlotTag*/);

/** Broadcasts that the quick slot bindings changed (local assignment or replicated update). */
DECLARE_MULTICAST_DELEGATE(FQuickSlotsChangedSignature);

class UInventoryComponent;

/** Replicated quick-slot binding between a gameplay tag and an inventory item identifier. */
USTRUCT()
struct FRPGQuickSlotEntry
{
	GENERATED_BODY()

	/** Quick-slot gameplay tag. */
	UPROPERTY()
	FGameplayTag QuickSlotTag = FGameplayTag();

	/** Inventory item identifier assigned to the quick slot. */
	UPROPERTY()
	int64 ItemID = 0;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UQuickSlotManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Creates the quick slot manager component. */
	UQuickSlotManagerComponent();

	/** Registers replicated properties for the quick-slot manager state. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Delegate invoked when a quick slot is activated through player input. */
	FQuickSlotActivatedSignature QuickSlotActivatedDelegate;

	/** Delegate invoked when QuickEquipWeaponSlotTag changes, locally or through replication. */
	FQuickEquipWeaponSlotChangedSignature OnQuickEquipWeaponSlotChanged;

	/** Delegate invoked when the quick slot bindings change, locally or through replication. */
	FQuickSlotsChangedSignature OnQuickSlotsChanged;

	/** Assigns an item to a quick slot tag and updates weapon quick-equip tracking when needed. */
	void AddQuickSlot(const FGameplayTag& QuickSlotTag, int64 ItemID);

	/** Removes the slot entry associated with the provided item identifier. */
	void RemoveQuickSlot(int64 ItemID);

	/** Uses the quick slot identified by the provided gameplay tag. */
	UFUNCTION(BlueprintCallable, Category = "Quick Slot")
	void UseQuickSlot(const FGameplayTag& QuickSlotTag);

	/** Returns the item identifier currently bound to a quick slot tag, or 0 if unassigned. */
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	int64 GetQuickSlotItemID(const FGameplayTag& QuickSlotTag) const;

	/**
	 * Returns whether an item is bound to any quick slot. On the server this is the mirror of every owner's
	 * quick slots (the owning client assigns them and replicates the change), so it is the server's reference.
	 *
	 * @param ItemID Inventory item to look for.
	 */
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	bool IsItemQuickSlotted(int64 ItemID) const;

	/**
	 * Returns the quick slot an item is bound to.
	 *
	 * @param ItemID Inventory item to look for.
	 * @return The quick slot tag, or an invalid tag when the item is not quick slotted.
	 */
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	FGameplayTag FindQuickSlotTagByItemID(int64 ItemID) const;

	/**
	 * Attempts to equip the last weapon quick slot used while skipping activation callbacks.
	 *
	 * @return True if a valid weapon quick slot was found and used.
	 */
	bool TryEquipWeapon();

	/** Weapon quick slot equipped by the unarmed-attack quick equip; invalid when no weapon is quick slotted. */
	FGameplayTag GetQuickEquipWeaponSlotTag() const {return QuickEquipWeaponSlotTag;}
	
protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Synchronizes the tracked weapon quick slot locally and on the server when needed. */
	void SetQuickEquipWeaponSlotTag(const FGameplayTag& NewQuickEquipWeaponSlotTag);

	/** Applies the activation callback or restores activation gating after a forced use. */
	UFUNCTION(BlueprintCallable, Category = "Quick Slot")
	void HandleQuickSlotActivation(const FGameplayTag& QuickSlotTag);

	/** Resolves the owner's inventory component through the inventory interface. */
	UInventoryComponent* ResolveInventoryComponent() const;

	/** Updates the tracked quick-equip weapon tag when the used slot belongs to the weapon category. */
	void UpdateQuickEquipWeaponSlotOnUse(const FGameplayTag& QuickSlotTag);

	/** Tells the ability system a weapon swap is in flight when this is a client using a weapon slot; no-op otherwise. */
	void NotifyWeaponSwapRequestedIfClient(const FGameplayTag& QuickSlotTag) const;

	/** Restores a fallback weapon quick slot after removing the currently tracked one. */
	void RefreshQuickEquipWeaponSlotTag();

	/** Reliable server RPC used to mirror quick-equip weapon slot changes from the client. */
	UFUNCTION(Server, Reliable)
	void ServerSetQuickEquipWeaponSlotTag(FGameplayTag NewQuickEquipWeaponSlotTag);

	/** Reliable server RPC used to mirror quick-slot additions from the client. */
	UFUNCTION(Server, Reliable)
	void ServerAddQuickSlot(const FGameplayTag& QuickSlotTag, int64 ItemID);

	/** Reliable server RPC used to mirror quick-slot removals from the client. */
	UFUNCTION(Server, Reliable)
	void ServerRemoveQuickSlot(int64 ItemID);

	/** Reliable server RPC used to mirror activation gating changes from the client. */
	UFUNCTION(Server, Reliable)
	void ServerSetCanTriggerActivation(bool bNewCanTriggerActivation);

	/** Sets the activation gate locally and mirrors the change to the server when needed. */
	void SetCanTriggerActivation(bool bNewCanTriggerActivation);

	/** Called when the replicated quick-slot map changes on clients. */
	UFUNCTION()
	void OnRep_QuickSlotTagMap();

	/** Called when the replicated quick-equip weapon slot changes on clients; broadcasts the change. */
	UFUNCTION()
	void OnRep_QuickEquipWeaponSlotTag();

	/** Rebuilds the local quick-slot cache from the replicated entry list. */
	void SyncQuickSlotMapFromEntries();

	/** Rebuilds the replicated entry list from the local quick-slot cache. */
	void SyncQuickSlotEntriesFromMap();

private:
	// ============================================================
	// Properties
	// ============================================================

	/** Stores the replicated quick slot tag used for fast weapon equip actions. */
	UPROPERTY(ReplicatedUsing = OnRep_QuickEquipWeaponSlotTag)
	FGameplayTag QuickEquipWeaponSlotTag;

	/** Stores the replicated quick-slot bindings used to rebuild the local cache on remote peers. */
	UPROPERTY(ReplicatedUsing = OnRep_QuickSlotTagMap)
	TArray<FRPGQuickSlotEntry> QuickSlotTagEntries;

	/** Local cache mapping quick-slot gameplay tags to inventory item identifiers. */
	TMap<FGameplayTag, int64> QuickSlotTagMap;

	/** Stores whether quick-slot activation callbacks are currently allowed and replicates to both peers. */
	UPROPERTY(Replicated)
	bool bCanTriggerActivation = true;
};
