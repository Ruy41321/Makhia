// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ItemTypes.h"
#include "MKHItemEntry.h"
#include "MKHStartingItem.h"
#include "Delegates/DelegateCombinations.h"
#include "Equipment/EquipmentTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "QuickSlot/MKHQuickSlotPreferenceSaveGame.h"
#include "InventoryComponent.generated.h"

class APawn;
class UAbilitySystemComponent;
class UEquipmentManagerComponent;
class UInventoryComponent;
class UQuickSlotManagerComponent;
struct FStreamableHandle;
struct FStreamableManager;

DECLARE_MULTICAST_DELEGATE_OneParam(FEquipmentItemUsed, const FMKHItemEntry& /*UsedItem*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FEquipmentItemUnequipped, int64 /*ItemIDToRemove*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FStartingLoadoutGranted, const TArray<int64>& /*GrantedItemIDs*/);

/** One inventory slot: the stored item plus the state that only exists while it sits in the inventory. */
USTRUCT(BlueprintType)
struct FRPGInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Identity of the stored item (tag, ID, quantity, rarity, rolled effects). */
	UPROPERTY(BlueprintReadOnly)
	FMKHItemEntry Item = FMKHItemEntry();

	/** Used to track quick-slotted weapons currently in use (equipped while still in the inventory). */
	UPROPERTY(BlueprintReadOnly)
	bool bIsUsed = false;

	/** True when the item is currently assigned to a quick slot. */
	UPROPERTY(BlueprintReadOnly, NotReplicated)
	bool bIsQuickSlotted = false;

	/** Quick slot tag associated with this item when quick-slotted. */
	UPROPERTY(BlueprintReadOnly, NotReplicated)
	FGameplayTag QuickSlotTag = FGameplayTag();

	/** Returns whether this entry represents a valid item. */
	bool IsValid() const
	{
		return Item.IsValid();
	}
};

DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryItemChangedSignature, const FRPGInventoryEntry& /*DirtyEntry*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryItemRemovedSignature, const int64 /*RemovedItemID*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryQuickSlotRelocatedSignature, const FRPGInventoryEntry& /*EntryToRelocate*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryQuickSlotChangedSignature, const FRPGInventoryEntry& /*QuickSlotEntryToUpdate*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryQuickSlotRemovedSignature, const int64 /*QuickSlotEntryIDToRemove*/);

USTRUCT()
struct FRPGInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	/** Default constructor required by Unreal serialization. */
	FRPGInventoryList() : OwnerComponent(nullptr) {}

	/** Constructor that binds this list to its owning inventory component. */
	FRPGInventoryList(UInventoryComponent* InComponent) : OwnerComponent(InComponent) {}

	/**
	 * Stores an item keeping its ItemID. If an entry with the same ItemID is still in the list
	 * (weapons stay in the inventory while equipped) only its in-use flag is cleared; non-equipment
	 * items merge into an existing stack of the same tag, preferring one that is not quick-slotted
	 * (the incoming ItemID is dropped).
	 *
	 * @param Item The item to store; invalid entries are ignored.
	 * @return ID of the entry now holding the item (the stack it merged into, or its own), 0 when ignored.
	 */
	int64 AddItemEntry(const FMKHItemEntry& Item);

	/**
	 * Stores an item as a new entry, never merging it into an existing stack (e.g. the part split off a stack).
	 *
	 * @param Item The item to store; invalid entries and IDs already in the list are ignored.
	 */
	void AddSeparateEntry(const FMKHItemEntry& Item);

	/** Removes a specified number of items from an entry. */
	void RemoveItem(const FRPGInventoryEntry& InventoryEntry, int32 NumItems = 1);

	/** Checks if the inventory contains at least NumItems of the given ItemID. */
	bool HasEnough(int64 ItemID, int32 NumItems) const;

	/** Gets the inventory entry currently using the specified quick slot tag. */
	FRPGInventoryEntry* GetAlreadyQuickSlottedEntry(const FGameplayTag& QuickSlotTag);

	/** Associates an inventory entry with a specific quick slot tag. */
	void AddEntryToQuickSlot(int64 ItemID, const FGameplayTag& QuickSlotTag);

	/** Clears a quick slot tag from the associated item. */
	void RemoveEntryFromQuickSlot(const int64 ItemID);

	/** Finds and returns a pointer to the entry with the given ID, or nullptr if not found. */
	FRPGInventoryEntry* FindEntryByID(int64 ItemID);

	/** Const overload of FindEntryByID. */
	const FRPGInventoryEntry* FindEntryByID(int64 ItemID) const;

	// FFastArraySerializer Contract
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRPGInventoryEntry, FRPGInventoryList>(Entries, DeltaParms, *this);
	}

	/** Broadcast when a standard inventory entry changes. */
	FInventoryItemChangedSignature InventoryItemChangedDelegate;
	/** Broadcast when a standard inventory entry is removed. */
	FInventoryItemRemovedSignature InventoryItemRemovedDelegate;
	/** Broadcast when an entry is relocated across quick slots. */
	FInventoryQuickSlotRelocatedSignature QuickSlotItemRelocatedDelegate;
	/** Broadcast when a quick-slot entry changes. */
	FInventoryQuickSlotChangedSignature QuickSlotItemChangeDelegate;
	/** Broadcast when a quick-slot entry is removed. */
	FInventoryQuickSlotRemovedSignature QuickSlotItemRemovedDelegate;

private:

	friend class UInventoryComponent;

	/** Replicated fast-array storage for inventory entries. */
	UPROPERTY()
	TArray<FRPGInventoryEntry> Entries;

	/** Owning inventory component used for callbacks and authority checks. */
	UPROPERTY(NotReplicated)
	TObjectPtr<UInventoryComponent> OwnerComponent;

	/**
	 * Tries to find an existing stackable entry (see FindStackTarget) and increment its quantity.
	 *
	 * @param ItemTag   Tag of the item to merge.
	 * @param NumItems  Quantity to add.
	 * @return ID of the stack that received the quantity, 0 when none could.
	 */
	int64 TryStackItem(const FGameplayTag& ItemTag, int32 NumItems);

	/** Finds the first stack of ItemTag that is quick-slotted (owner's quick slot manager), or nullptr. */
	const FRPGInventoryEntry* FindQuickSlottedStack(const FGameplayTag& ItemTag) const;

	/**
	 * Finds the stack a quantity of ItemTag should merge into: the first stack that is not quick-slotted,
	 * otherwise (when allowed) the first quick-slotted one.
	 *
	 * @param ItemTag            Tag of the item to merge.
	 * @param ExcludedItemID     Entry to skip (the one being merged away); 0 skips nothing.
	 * @param bAllowQuickSlotted Whether a quick-slotted stack may receive the quantity.
	 * @return The target entry, or nullptr when there is none.
	 */
	FRPGInventoryEntry* FindStackTarget(const FGameplayTag& ItemTag, int64 ExcludedItemID, bool bAllowQuickSlotted);
	/** Marks an entry dirty and broadcasts a change event on authority. */
	void MarkEntryChanged(FRPGInventoryEntry& Entry);
	/** Handles relocation of any existing entry already assigned to a target quick slot. */
	void RelocateQuickSlotOccupant(FRPGInventoryEntry& EntryToAssign, const FGameplayTag& QuickSlotTag);

	/**
	 * Takes an entry that is about to be removed out of its quick slot on this machine (the flags only exist where
	 * the slot was assigned), broadcasting the same relocation as a manual removal so every quick slot view clears it.
	 *
	 * @param Entry The entry being removed.
	 */
	void ReleaseQuickSlotOnRemoval(FRPGInventoryEntry& Entry);

	/** Marks an entry dirty and broadcasts it if running with authority. */
	void BroadcastNewEntry(FRPGInventoryEntry& NewEntry);

	/**
	 * Dispatches the appropriate delegate based on the entry's QuickSlot state and the operation type.
	 * @param Entry    The inventory entry involved in the change or removal.
	 * @param bChanged If true, broadcasts a "changed/updated" delegate; if false, broadcasts a "removed" delegate.
	 */
	void BroadcastEntryUpdate(const FRPGInventoryEntry& Entry, bool bChanged);

};

template<>
struct TStructOpsTypeTraits<FRPGInventoryList> : public TStructOpsTypeTraitsBase2<FRPGInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/**
 * Replicated player inventory, owned by the PlayerController.
 * All mutations run on the server; clients receive the list through FastArray replication.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MAKHIA_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

	/** The list applies pending quick slot assignments when entries replicate in. */
	friend struct FRPGInventoryList;

public:

	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Default constructor */
	UInventoryComponent();

	/** Replicates the component properties. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	// --- Inventory Operations ---

	/**
	 * Server only: generates a new item through UMKHItemSubsystem (equipment is rolled) and stores it
	 * (consumables stack). Clients cannot request items: calls without authority are ignored.
	 *
	 * @param ItemTag  Tag of the item definition to generate.
	 * @param NumItems Quantity to add.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Operations")
	void AddItem(const FGameplayTag& ItemTag, int32 NumItems = 1);

	/** Uses an item from the inventory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	void UseItem(int64 ItemID, int32 NumItems = 1);

	/**
	 * Server only: generates every item of a starting loadout with its rarity rule and stores it. What gets worn and
	 * quick slotted is decided by the listeners of OnStartingLoadoutGranted (see FMKHDefaultQuickSlots), which need the
	 * owner's pawn for equipment visuals (call it once the pawn is possessed).
	 *
	 * @param Loadout Items to grant, in order.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Operations")
	void GrantStartingLoadout(const TArray<FMKHStartingItem>& Loadout);

	/**
	 * Server: broadcast at the end of every GrantStartingLoadout (first spawn and rematch), once the loadout is in the
	 * inventory, with the IDs of its items, in loadout order.
	 */
	FStartingLoadoutGranted OnStartingLoadoutGranted;

	/**
	 * Server only: destroys items owned by the player. When the whole entry goes away it is first
	 * unequipped (equipped armor returns to the inventory to be removed) and cleared from its quick slot.
	 * Calls without authority are ignored.
	 *
	 * @param ItemID   Item to remove, from the inventory or the equipment.
	 * @param NumItems Quantity to remove (at least 1); the entry is deleted when nothing is left.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Operations")
	void RemoveItem(int64 ItemID, int32 NumItems = 1);

	/**
	 * Drops items on the ground in front of the owning pawn as a world pickup; clients forward the
	 * request to the server. An equipped item is unequipped first; an item leaving the inventory
	 * entirely also leaves its quick slot. Dropping part of a stack gives the dropped part a new
	 * ItemID while the remaining stack keeps its own. Equipment always drops whole.
	 *
	 * @param ItemID   Item to drop, from the inventory or the equipment.
	 * @param Quantity Quantity to drop; 0 or less (or more than the stack) drops the whole stack.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	void DropItem(int64 ItemID, int32 Quantity = -1);

	/**
	 * Server only: destroys every item the player owns (bag, equipment, quick slots), e.g. before a rematch grants
	 * the starting loadout again. Each item goes through RemoveItem, so it is unequipped and leaves its quick slot
	 * first. Calls without authority are ignored.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Operations")
	void ResetInventory();

	/**
	 * Server only: stores an existing item keeping its ItemID (e.g. an item returned by the equipment);
	 * non-equipment items merge into an existing stack of the same tag.
	 * Clients receive the change through replication; calls without authority are ignored.
	 *
	 * @param Item The item to store.
	 * @return ID of the entry now holding the item (a merged stack keeps its own ID), 0 when ignored.
	 */
	int64 AddItemEntry(const FMKHItemEntry& Item);

	/**
	 * Server only: moves part of a consumable stack into another stack of the same tag (e.g. the potions just picked
	 * up into the quick-slotted stack, see FMKHPickupAutoEquip). The source entry goes away when emptied.
	 *
	 * @param SourceItemID  Stack giving the quantity.
	 * @param TargetItemID  Stack receiving it (same tag).
	 * @param Quantity      Quantity to move, clamped to the source stack.
	 */
	void MoveQuantityToStack(int64 SourceItemID, int64 TargetItemID, int32 Quantity);

	/**
	 * Gets the first stack of an item tag that is quick-slotted (owner's quick slot manager; on the server, its mirror).
	 *
	 * @param ItemTag Item definition tag.
	 * @return The stack's ItemID, 0 when no stack of that tag is quick-slotted.
	 */
	int64 FindQuickSlottedStackID(const FGameplayTag& ItemTag) const;

	// --- QuickSlot Management ---

	/**
	 * Assigns an entry to a quick slot on the owning machine (quick slot flags are local; the quick slot manager
	 * mirrors them to the server). A consumable stack pushed out of every quick slot by this assignment is merged
	 * back into its bag stack.
	 *
	 * @param ItemID       Entry to assign.
	 * @param QuickSlotTag Target quick slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	void AddEntryToQuickSlot(int64 ItemID, const FGameplayTag& QuickSlotTag);

	/**
	 * Takes an entry out of its quick slot on the owning machine; a consumable stack is then merged back into
	 * its bag stack of the same tag, if any.
	 *
	 * @param ItemID Entry to remove from its quick slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	void RemoveEntryFromQuickSlot(int64 ItemID);

	/**
	 * Moves part of a consumable stack into a new stack (new ItemID) assigned to a consumable quick slot; the rest
	 * stays in the original stack. When Quantity covers the whole stack, the stack itself is assigned. Clients
	 * forward the split to the server, which assigns the quick slot back on the owning machine.
	 *
	 * @param ItemID       Source consumable stack.
	 * @param Quantity     Quantity to move (at least 1).
	 * @param QuickSlotTag Consumable quick slot receiving the new stack.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	void SplitStackToQuickSlot(int64 ItemID, int32 Quantity, FGameplayTag QuickSlotTag);

	/**
	 * Returns whether an item is bound to a quick slot, according to the owner's quick slot manager
	 * (on the server: the mirror of the owning client's quick slots).
	 *
	 * @param ItemID Inventory item to look for.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	bool IsItemQuickSlotted(int64 ItemID) const;

	/**
	 * Server: assigns a quick slot where the flags live — directly on the listen-server host, through
	 * ClientAssignQuickSlot for a remote owner.
	 */
	void AssignQuickSlotOnOwner(int64 ItemID, const FGameplayTag& QuickSlotTag);

	// --- Weapon skills ---

	/**
	 * Returns whether the skills of a weapon's two slots can be exchanged now: the weapon holds a skill on FromSlot and
	 * neither skill is running (see Documentation/InventoryDashboard.md).
	 *
	 * @param ItemID   Weapon in the inventory.
	 * @param FromSlot Skill slot holding the skill to move.
	 * @param ToSlot   Destination skill slot.
	 */
	bool CanSwapWeaponSkills(int64 ItemID, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot) const;

	/**
	 * Moves a weapon's skill to another skill slot; a skill already there takes the slot left free. Clients forward the
	 * request to the server. Works whether the weapon is in hand or not.
	 *
	 * @param ItemID   Weapon in the inventory.
	 * @param FromSlot Skill slot holding the skill to move.
	 * @param ToSlot   Destination skill slot.
	 */
	void SwapWeaponSkills(int64 ItemID, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot);

	/**
	 * Server: sets the skill slots the owner remembers and applies them to the weapons already owned, except the one the
	 * player arranged last (a conflict must not undo that move) and those with a skill running. Weapons entering the
	 * inventory get them on arrival.
	 *
	 * @param InSkillLayout Remembered slot of each skill.
	 */
	void SetSkillLayout(const TArray<FMKHSkillSlotPreference>& InSkillLayout);

	/** Gets the skill slots the owner remembers (server). */
	const TArray<FMKHSkillSlotPreference>& GetSkillLayout() const { return SkillLayout; }

	// --- Lookups & Queries ---

	/** Returns a copy of the entry with the given ID. Check IsValid() on the result. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Queries")
	FRPGInventoryEntry FindInventoryEntryByID(int64 ItemID) const;

	/** Returns whether the inventory contains at least the requested quantity for the specified item ID. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Queries")
	bool HasEnough(int64 ItemID, int32 NumItems = 1) const;

	/**
	 * Gets the cooldown tag a consumable grants when used, read from its item definition.
	 *
	 * @param ItemID Inventory item to query.
	 * @return The cooldown tag, or an empty tag when the item is missing or has no cooldown.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Queries")
	FGameplayTag GetConsumableCooldownTag(int64 ItemID) const;

	/** Retrieves all inventory entries. */
	TArray<FRPGInventoryEntry> GetInventoryEntries() const;

	// --- Preloading ---

	/**
	 * Preloads the assets of the weapon being quickslotted.
	 * Allows immediate use (e.g., sheathed attacks) without loading hitches.
	 */
	void PreloadItem(FRPGInventoryEntry* Entry);

	/** Removes an item's preloaded asset references. */
	void RemovePreloadedItemRef(int64 ItemID);

protected:

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns true when the owner is valid and runs with authority (inventory mutations are server only). */
	bool HasInventoryAuthority() const;

	// --- Drop & removal ---

	/**
	 * Server side of DropItem: spawns the pickup first (thrown from the pawn, see EMKHPickupAppearance::Drop),
	 * then removes the dropped quantity, so a failed spawn never loses the item.
	 *
	 * @param ItemID   Item to drop.
	 * @param Quantity Requested quantity (see DropItem).
	 */
	void DropItemOnServer(int64 ItemID, int32 Quantity);

	/**
	 * Builds the item that lands on the ground: the whole stored item, or a split part with a
	 * new ItemID when only part of a stack is dropped.
	 *
	 * @param StoredItem        Item currently stored in the inventory.
	 * @param RequestedQuantity Requested quantity (see DropItem).
	 * @return The item to spawn; invalid when a new ItemID could not be generated.
	 */
	FMKHItemEntry MakeDroppedItem(const FMKHItemEntry& StoredItem, int32 RequestedQuantity) const;

	/**
	 * Computes where a dropped item lands: DropDistance in front of the pawn plus a random offset within
	 * DropScatterRadius (kept on this side of walls), snapped to the ground below (AMKHPickupItem::FindRestLocation).
	 *
	 * @param Pawn         The dropping pawn.
	 * @param OutTransform Landing transform, yaw aligned with the pawn.
	 * @return False when there is no world.
	 */
	bool ComputeDropTransform(const APawn& Pawn, FTransform& OutTransform) const;

	/** Gets the pawn of the owning controller, or nullptr. */
	APawn* GetOwnerPawn() const;

	/** Returns the IDs of every item the player owns: the bag entries, then the equipped items (no duplicates). */
	TArray<int64> CollectOwnedItemIDs() const;

	/** Unequips the item and clears its quick slot, before it leaves the player entirely. */
	void ReleaseItemBindings(int64 ItemID);

	/** Asks the equipment to unequip the item when it holds it; equipped armor returns to the inventory. */
	void UnequipIfEquipped(int64 ItemID);

	/**
	 * Clears the item's quick slot: the local entry flags (listen-server host) and the server quick
	 * slot manager, which replicates the change to remote owners.
	 */
	void ClearQuickSlotBinding(int64 ItemID);

	/** Gets the owner's quick slot manager through IQuickSlotInterface, or nullptr. */
	UQuickSlotManagerComponent* FindQuickSlotManager() const;

	/** Gets the owner's equipment through IEquipmentInterface, or nullptr. */
	UEquipmentManagerComponent* FindEquipmentManager() const;

	/** Returns whether a skill of an equipped weapon is running. */
	bool IsAnySkillOfItemActive(const FMKHItemEntry& Item) const;

	/**
	 * Server: publishes a weapon whose skill slots changed: its equipped copy and granted skills, then the entry.
	 *
	 * @param Entry The weapon's entry, already changed.
	 */
	void PublishWeaponSkillChange(FRPGInventoryEntry& Entry);

	/** Returns true when the owning PlayerController is controlled on this machine (listen-server host or client). */
	bool IsLocallyOwned() const;

	// --- Split & restack ---

	/**
	 * Server side of SplitStackToQuickSlot: moves Quantity into a new separate stack and assigns it to the quick
	 * slot on the owning machine; a quantity covering the whole stack assigns the stack itself.
	 *
	 * @param ItemID       Source consumable stack.
	 * @param Quantity     Quantity to move.
	 * @param QuickSlotTag Consumable quick slot receiving the stack.
	 */
	void SplitStackOnServer(int64 ItemID, int32 Quantity, const FGameplayTag& QuickSlotTag);

	/** Owning client: applies a quick slot assignment that arrived before its entry replicated (see PostReplicatedAdd). */
	void ApplyPendingQuickSlotAssignment(int64 ItemID);

	/** Asks the server to merge a consumable stack that just left the quick slots back into its bag stack. */
	void RequestRestack(int64 ItemID);

	/**
	 * Server: merges a consumable stack that is no longer quick-slotted into another stack of the same tag
	 * that is not quick-slotted either; does nothing when there is none.
	 *
	 * @param ItemID Stack to merge away.
	 */
	void RestackOnServer(int64 ItemID);

	/**
	 * Looks up an item master definition through UMKHDataSubsystem.
	 *
	 * @param ItemTag Tag of the item definition.
	 * @return Pointer to the DataTable row, or nullptr when not found. Do not store it.
	 */
	const FMasterItemDefinition* FindItemDefinition(const FGameplayTag& ItemTag) const;

	/** Tries to use a consumable item and applies its effect if valid and not on cooldown. */
	void TryUseConsumable(FRPGInventoryEntry* Entry, const FMasterItemDefinition& ItemDef, UAbilitySystemComponent* OwnerASC, int32 NumItems);

	/**
	 * Checks whether the consumable's cooldown is currently active on the owner ASC.
	 *
	 * @param ConsumableProps Consumable data of the item definition (cooldown tag).
	 * @param OwnerASC        Ability system component of the owning actor.
	 * @return True if the cooldown tag is valid and currently granted to the ASC.
	 */
	bool IsConsumableOnCooldown(const FConsumableProps& ConsumableProps, const UAbilitySystemComponent* OwnerASC) const;

	/**
	 * Applies the consumable's cooldown to the owner ASC via the generic set-by-caller
	 * cooldown effect, granting the cooldown tag for the cooldown duration.
	 *
	 * @param ConsumableProps Consumable data of the item definition (cooldown time and tag).
	 * @param OwnerASC        Ability system component of the owning actor.
	 */
	void ApplyConsumableCooldown(const FConsumableProps& ConsumableProps, UAbilitySystemComponent* OwnerASC) const;

	/** Tries to use an equipment item, broadcasting usage delegates. */
	void TryUseEquipment(FRPGInventoryEntry* Entry, const FMasterItemDefinition& ItemDef);

	/** Marks an equipped weapon entry as currently used and propagates list updates. */
	void MarkWeaponEntryUsed(FRPGInventoryEntry& Entry);

	/** Preloads all equipment actor assets for the given definition. */
	void PreloadEquipmentActors(const UEquipmentDefinition* EquipmentCDO, TArray<TSharedPtr<FStreamableHandle>>& Handles, FStreamableManager& Manager);

	/** Preloads all abilities specified in the effect package. */
	void PreloadAbilities(const FEquipmentEffectPackage& EffectPackage, TArray<TSharedPtr<FStreamableHandle>>& Handles, FStreamableManager& Manager);

	/** Preloads all passive stat effect classes. */
	void PreloadPassiveEffects(const FEquipmentEffectPackage& EffectPackage, TArray<TSharedPtr<FStreamableHandle>>& Handles, FStreamableManager& Manager);

private:

	/** Server RPC for using items. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUseItem(int64 ItemID, int32 NumItems);
	bool ServerUseItem_Validate(int64 ItemID, int32 NumItems);

	/** Server RPC for dropping items; rejects malformed item IDs. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerDropItem(int64 ItemID, int32 Quantity);
	bool ServerDropItem_Validate(int64 ItemID, int32 Quantity);

	/** Server RPC for splitting a stack into a quick slot; rejects malformed IDs, quantities and tags. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSplitStackToQuickSlot(int64 ItemID, int32 Quantity, FGameplayTag QuickSlotTag);
	bool ServerSplitStackToQuickSlot_Validate(int64 ItemID, int32 Quantity, FGameplayTag QuickSlotTag);

	/** Server RPC merging a stack that left the quick slots back into its bag stack; rejects malformed IDs. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRestackItem(int64 ItemID);
	bool ServerRestackItem_Validate(int64 ItemID);

	/** Server RPC moving a weapon's skill to another skill slot; rejects malformed IDs and tags. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSwapWeaponSkills(int64 ItemID, FGameplayTag FromSlot, FGameplayTag ToSlot);
	bool ServerSwapWeaponSkills_Validate(int64 ItemID, FGameplayTag FromSlot, FGameplayTag ToSlot);

	/** Client RPC: assigns a quick slot on the owning client (queued until the entry replicates). */
	UFUNCTION(Client, Reliable)
	void ClientAssignQuickSlot(int64 ItemID, FGameplayTag QuickSlotTag);

public:

	// ============================================================
	// Properties
	// ============================================================

	/** Broadcasts when an equipment item is used. */
	FEquipmentItemUsed EquipmentItemUsedDelegate;

	/** Broadcasts when an equipment item is unequipped. */
	FEquipmentItemUnequipped EquipmentItemUnequippedDelegate;

	/** Replicated list tracking all inventory entries safely. */
	UPROPERTY(Replicated)
	FRPGInventoryList InventoryList;

private:

	/** Horizontal distance in front of the pawn where dropped items land. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drop", meta = (ClampMin = "0", Units = "cm", AllowPrivateAccess = "true"))
	float DropDistance = 100.f;

	/** Radius of the random offset added to the drop point, so items dropped from the same spot do not overlap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drop", meta = (ClampMin = "0", Units = "cm", AllowPrivateAccess = "true"))
	float DropScatterRadius = 40.f;

	/** Gap kept between a dropped item and a wall standing between the pawn and the drop point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drop", meta = (ClampMin = "0", Units = "cm", AllowPrivateAccess = "true"))
	float DropWallClearance = 30.f;

	/** How far below the pawn's center the ground is searched for; without ground the item lands at the pawn's feet height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drop", meta = (ClampMin = "0", Units = "cm", AllowPrivateAccess = "true"))
	float DropGroundTraceDistance = 500.f;

	/** Maps item IDs to their asynchronously loaded asset handles. */
	TMap<int64, TArray<TSharedPtr<FStreamableHandle>>> PreloadedItemHandles;

	/** Server: skill slots the owner remembers, applied to weapons entering the inventory. */
	TArray<FMKHSkillSlotPreference> SkillLayout;

	/** Server: weapon the player arranged last, left out when the layout is applied to the owned weapons. */
	int64 LastArrangedItemID = 0;

	/** Owning client: quick slot assignments received before their entry replicated, keyed by ItemID. */
	TMap<int64, FGameplayTag> PendingQuickSlotAssignments;

};
