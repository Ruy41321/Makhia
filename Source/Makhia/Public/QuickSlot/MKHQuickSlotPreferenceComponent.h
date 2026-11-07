// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "QuickSlot/MKHQuickSlotPreferenceSaveGame.h"
#include "MKHQuickSlotPreferenceComponent.generated.h"

class UEquipmentManagerComponent;
class UInventoryComponent;
class UQuickSlotManagerComponent;
struct FMKHItemEntry;
struct FRPGEquipmentEntry;
struct FRPGInventoryEntry;
struct FRarityDefinition;

/**
 * Remembers how the player arranges the starting loadout: which kind of weapon / consumable goes in each quick slot
 * and which armor types they took off, and applies it when the next loadout is granted. Lives on AMKHPlayerController
 * (never on the duel bot).
 *
 * - Owning machine: loads the choices from the "QuickSlotPreferences" save slot, records every quick slot assignment,
 *   every slot emptied and armor piece taken off by hand (other removals change nothing), saves them and sends them to
 *   the server (tags only), with the "Remember Quick Slots" setting (UMKHGameUserSettings::IsAutoQuickSlotLoadoutEnabled,
 *   resent on change).
 * - Server: after UInventoryComponent::OnStartingLoadoutGranted, wears the rarest armor of each type not taken off,
 *   then quick slots for each preference the best item of that kind (highest rarity) through
 *   UInventoryComponent::AssignQuickSlotOnOwner; the first match ever fills the slots with FMKHDefaultQuickSlots. With
 *   the setting off the rarest armor of every type is worn and the quick slots start empty, while the owner keeps
 *   recording.
 *
 * See Documentation/HUDQuickSlots.md, "Starting loadout quick slot preferences".
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHQuickSlotPreferenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Creates the component (replicated for its server RPC, initialized before the first loadout grant). */
	UMKHQuickSlotPreferenceComponent();

	/** Server: listens for starting loadout grants (can happen before BeginPlay). */
	virtual void InitializeComponent() override;

	/** Owning machine: loads the saved preferences, sends them to the server and records new quick slot assignments. */
	virtual void BeginPlay() override;

	/** Stops listening to the inventory and drops a pending commit. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the kind an item is remembered by: the tag right below Item.Equipment.Weapon or Item.Consumable
	 * (Item.Equipment.Weapon.Sword.BronzeSword → Item.Equipment.Weapon.Sword; Item.Consumable.HealthPotion stays).
	 *
	 * @param ItemTag Item definition tag.
	 * @return The kind tag, or an invalid tag for items that cannot be quick slotted.
	 */
	UFUNCTION(BlueprintPure, Category = "Quick Slot|Preferences")
	static FGameplayTag GetItemKindTag(const FGameplayTag& ItemTag);

	/**
	 * Returns whether a tag is an armor type (FMKHDefaultQuickSlots::GetArmorType), as saves and clients must send.
	 *
	 * @param ArmorType Tag to check.
	 */
	static bool IsValidArmorType(const FGameplayTag& ArmorType);

	/**
	 * Returns whether a preference pairs a quick slot with a kind that fits it (weapon kind in a weapon slot,
	 * consumable kind in a consumable slot).
	 *
	 * @param Preference Preference to check.
	 */
	static bool IsValidPreference(const FMKHQuickSlotPreference& Preference);

	/** Gets the preferences known here (owning machine: saved; server: last received). */
	UFUNCTION(BlueprintPure, Category = "Quick Slot|Preferences")
	const TArray<FMKHQuickSlotPreference>& GetPreferences() const { return Preferences; }

	/**
	 * Owning machine: the player emptied a quick slot by hand, so it stays empty from the next loadout on. Never called
	 * for a stack used up, a dropped item or the rematch reset.
	 *
	 * @param QuickSlotTag The slot emptied.
	 */
	void NotifyQuickSlotEmptiedByPlayer(const FGameplayTag& QuickSlotTag);

	/**
	 * Owning machine: the player took off a piece of armor by hand, so its type stays off from the next loadout on,
	 * until a piece of that type is worn again.
	 *
	 * @param ItemTag Tag of the piece; items that are not armor are ignored.
	 */
	void NotifyArmorUnequippedByPlayer(const FGameplayTag& ItemTag);

	/**
	 * Owning machine: the player moved a skill to a skill slot, so it goes there on every weapon that grants it, from
	 * the next weapon received on.
	 *
	 * @param AbilityTag   The skill.
	 * @param SkillSlotTag The slot it now stands on.
	 */
	void NotifySkillSlotChosenByPlayer(const FGameplayTag& AbilityTag, const FGameplayTag& SkillSlotTag);

	/**
	 * Returns whether a skill preference names a skill and a skill slot, as saves and clients must send.
	 *
	 * @param Preference Preference to check.
	 */
	static bool IsValidSkillSlotPreference(const FMKHSkillSlotPreference& Preference);

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	// --- Owning machine ---

	/** Reads the preferences from the save slot (none on the first match ever). */
	void LoadPreferences();

	/** Records an item that just took a quick slot; entries leaving their slot are ignored. */
	void HandleQuickSlotRelocated(const FRPGInventoryEntry& Entry);

	/** A piece of armor is worn (by any path, pickups included): its type is no longer off. */
	void HandleEquipmentEntryChanged(const FRPGEquipmentEntry& Entry);

	/**
	 * Stores Kind for QuickSlotTag and forgets the same kind in slots that are empty now (the item moved).
	 *
	 * @return True when the preferences changed.
	 */
	bool RecordPreference(const FGameplayTag& QuickSlotTag, const FGameplayTag& Kind);

	/** Returns whether an inventory entry is assigned to the quick slot on this machine. */
	bool IsQuickSlotFilledLocally(const FGameplayTag& QuickSlotTag) const;

	/** Commits on the next tick, so a swap (two assignments in a row) is saved and sent once. */
	void ScheduleCommit();

	/** Writes the preferences to the save slot and hands them to the server. */
	void CommitPreferences();

	/** Hands the preferences and the setting to the server: directly with authority, through ServerSetPreferences otherwise. */
	void SendPreferencesToServer();

	/**
	 * Resends the preferences when the "Remember Quick Slots" setting changes, so the next loadout follows it.
	 *
	 * @param bEnabled New value of the setting.
	 */
	void HandleAutoQuickSlotSettingChanged(bool bEnabled);

	/** Reads the "Remember Quick Slots" setting (on when the user settings are unavailable). */
	static bool IsAutoQuickSlotSettingEnabled();

	// --- Server ---

	/**
	 * Server RPC: receives the owner's choices (tags only, never item data) and setting.
	 *
	 * @param NewPreferences          Quick slot / kind pairs.
	 * @param NewUnequippedArmorTypes Armor types taken off by hand.
	 * @param bNewDefaultsApplied     True once the first match's defaults must not apply any more.
	 * @param NewSkillSlotPreferences Slot chosen for each skill moved by hand.
	 * @param bApplyEnabled           "Remember Quick Slots": false wears all the rarest armor and leaves the quick slots empty; the skill order applies anyway.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetPreferences(const TArray<FMKHQuickSlotPreference>& NewPreferences, const TArray<FGameplayTag>& NewUnequippedArmorTypes, bool bNewDefaultsApplied, const TArray<FMKHSkillSlotPreference>& NewSkillSlotPreferences, bool bApplyEnabled);
	bool ServerSetPreferences_Validate(const TArray<FMKHQuickSlotPreference>& NewPreferences, const TArray<FGameplayTag>& NewUnequippedArmorTypes, bool bNewDefaultsApplied, const TArray<FMKHSkillSlotPreference>& NewSkillSlotPreferences, bool bApplyEnabled);

	/**
	 * Server: stores the owner's choices and setting, and applies them if a loadout is waiting for them.
	 *
	 * @param NewPreferences          Quick slot / kind pairs.
	 * @param NewUnequippedArmorTypes Armor types taken off by hand.
	 * @param bNewDefaultsApplied     True once the first match's defaults must not apply any more.
	 * @param NewSkillSlotPreferences Slot chosen for each skill moved by hand.
	 * @param bApplyEnabled           Whether the player's choices apply to the loadout.
	 */
	void ReceivePreferences(const TArray<FMKHQuickSlotPreference>& NewPreferences, const TArray<FGameplayTag>& NewUnequippedArmorTypes, bool bNewDefaultsApplied, const TArray<FMKHSkillSlotPreference>& NewSkillSlotPreferences, bool bApplyEnabled);

	/**
	 * Server: marks the new loadout as waiting for the preferences, then tries to apply them.
	 *
	 * @param GrantedItemIDs The loadout's items, in loadout order.
	 */
	void HandleStartingLoadoutGranted(const TArray<int64>& GrantedItemIDs);

	/** Server: wears and quick slots the loadout once both the loadout and the preferences are there. */
	void TryApplyPreferences();

	/**
	 * Server: wears the rarest piece of each armor type the player did not take off (of every type with the setting off).
	 *
	 * @param Inventory Owner's inventory.
	 */
	void ApplyArmor(UInventoryComponent& Inventory) const;

	/**
	 * Server: assigns each preference's best matching item to its slot; slots already filled are left alone.
	 *
	 * @param Inventory    Owner's inventory.
	 * @param QuickSlots   Owner's quick slot manager (server mirror of the owner's quick slots).
	 */
	void ApplyPreferences(UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots) const;

	/**
	 * Server: until bDefaultsApplied, for each category (weapons, consumables) the preferences leave empty, quick slots
	 * the rarest item of each kind into the category's free slots, kinds in loadout order.
	 *
	 * @param Inventory    Owner's inventory.
	 * @param QuickSlots   Owner's quick slot manager (server mirror of the owner's quick slots).
	 */
	void ApplyDefaultQuickSlots(UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots) const;

	/**
	 * Quick slots the default items of one category into its free slots (rule shared with the duel bot:
	 * FMKHDefaultQuickSlots::PickForCategory).
	 *
	 * @param Inventory    Owner's inventory.
	 * @param QuickSlots   Owner's quick slot manager.
	 * @param CategoryTag  Item category root (Item.Equipment.Weapon or Item.Consumable).
	 * @param SlotTags     The category's quick slots, in fill order.
	 */
	void ApplyDefaultQuickSlotsOfCategory(UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots,
		const FGameplayTag& CategoryTag, TConstArrayView<FGameplayTag> SlotTags) const;

	/** Returns whether a saved preference targets an item of the given category root. */
	bool HasPreferenceForCategory(const FGameplayTag& CategoryTag) const;

	/**
	 * Finds the item of a kind to quick slot: not quick slotted, not already picked, highest rarity, then biggest stack.
	 *
	 * @param Inventory   Owner's inventory.
	 * @param QuickSlots  Owner's quick slot manager.
	 * @param Kind        Item kind (GetItemKindTag).
	 * @param ClaimedIDs  Items already picked for another slot.
	 * @return The item ID, or 0 when the inventory holds none.
	 */
	int64 FindBestItemOfKind(const UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots,
		const FGameplayTag& Kind, const TSet<int64>& ClaimedIDs) const;

	/** Returns whether Candidate should be preferred over Best (rarer, then bigger stack). */
	bool IsBetterCandidate(const FMKHItemEntry& Candidate, const FMKHItemEntry& Best) const;

	/**
	 * Compares two rarity rows: more rolled stats and abilities is rarer, then a lower roll weight.
	 *
	 * @param Rarity Row to rank; nullptr (unknown rarity) ranks lowest.
	 * @param Other  Row to compare against.
	 * @return True when Rarity is strictly rarer than Other.
	 */
	static bool IsRarer(const FRarityDefinition* Rarity, const FRarityDefinition* Other);

	// --- Helpers ---

	/** Gets the owner's inventory through IInventoryInterface, or nullptr. */
	UInventoryComponent* FindInventory() const;

	/** Gets the owner's equipment through IEquipmentInterface, or nullptr. */
	UEquipmentManagerComponent* FindEquipment() const;

	/** Gets the owner's quick slot manager through IQuickSlotInterface, or nullptr. */
	UQuickSlotManagerComponent* FindQuickSlotManager() const;

	/** Returns true when the owning controller is controlled on this machine. */
	bool IsLocallyControlled() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Preferences known here: saved ones on the owning machine, the owner's last sent ones on the server. */
	UPROPERTY(Transient)
	TArray<FMKHQuickSlotPreference> Preferences;

	/** Armor types taken off by hand, known like Preferences. */
	UPROPERTY(Transient)
	TArray<FGameplayTag> UnequippedArmorTypes;

	/** Slot chosen for each skill moved by hand, known like Preferences. */
	UPROPERTY(Transient)
	TArray<FMKHSkillSlotPreference> SkillSlotPreferences;

	/** True once the player's choices exist, known like Preferences: the first match's defaults never apply again. */
	bool bDefaultsApplied = false;

	/** Server: true once the owner sent its preferences (an empty list counts). */
	bool bHasReceivedPreferences = false;

	/** Server: the owner's "Remember Quick Slots" setting; false wears all the rarest armor and leaves the quick slots empty. */
	bool bApplyPreferences = true;

	/** Server: true while a granted loadout still waits for the preferences. */
	bool bLoadoutAwaitingPreferences = false;

	/** Server: items of the loadout waiting for the preferences, in loadout order. */
	TArray<int64> PendingLoadoutItemIDs;

	/** Owning machine: true while a commit is scheduled for the next tick. */
	bool bCommitScheduled = false;

	/** Subscription to the inventory's QuickSlotItemRelocatedDelegate (owning machine). */
	FDelegateHandle QuickSlotRelocatedHandle;

	/** Subscription to the equipment's EquipmentEntryDelegate (owning machine). */
	FDelegateHandle EquipmentEntryHandle;

	/** Subscription to UInventoryComponent::OnStartingLoadoutGranted (server). */
	FDelegateHandle LoadoutGrantedHandle;

	/** Subscription to UMKHGameUserSettings::OnAutoQuickSlotLoadoutChanged (owning machine). */
	FDelegateHandle SettingChangedHandle;

	/** Highest number of preferences accepted from a client (2 weapon + 3 consumable slots, with margin). */
	static constexpr int32 MaxPreferences = 8;

	/** Highest number of armor types accepted from a client (three today, with margin). */
	static constexpr int32 MaxUnequippedArmorTypes = 8;

	/** Highest number of remembered skills; the oldest choice goes first. */
	static constexpr int32 MaxSkillSlotPreferences = 64;
};
