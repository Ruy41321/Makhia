// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/EnemyBase.h"
#include "GameplayTagContainer.h"
#include "Interfaces/EquipmentInterface.h"
#include "Interfaces/InventoryInterface.h"
#include "Interfaces/MKHDuelParticipantInterface.h"
#include "Interfaces/QuickSlotInterface.h"
#include "Inventory/MKHStartingItem.h"
#include "MKHDuelBot.generated.h"

class UEquipmentManagerComponent;
class UInventoryComponent;
class UMKHSkinComponent;
class UQuickSlotManagerComponent;
struct FGameplayEventData;
struct FRPGInventoryEntry;

/**
 * Opponent of the offline duel (EMKHMatchMode::OfflineBot), spawned by AMKHGameMode on the PlayerStart left free by
 * the player and driven by AMKHDuelBotController. It has the attributes of its CharacterTag and the player's movement
 * framework (ACharacterBase), takes damage from the player's abilities through its own ability system component, dies
 * on Event.Combat.Death through the movement state machine like the player (Dead state: State.Movement.Dead and the
 * dead capsule collision; ABP_MKHPlayerCharacter plays the A_Die animation, the optional ragdoll is off on BP_DuelBot) and is revived by the game mode at every round. It owns the
 * player's item components (inventory, equipment, quick slots, wired by FMKHItemComponentLinks) on the pawn: the
 * game mode's loadout fills them (EquipLoadout), and the bot swaps weapons and drinks consumables by pressing the
 * player's quick slot inputs (GA_QuickSlot), so it gets the same abilities, timings and rules as the player.
 * The game mode chooses its skin (SetOpponentSkin), whose name it carries: it is presented to the player as a named
 * opponent, never as a bot.
 *
 * Full flow: see Documentation/MatchFlowArchitecture.md (Match Modes & the Offline Bot).
 */
UCLASS()
class MAKHIA_API AMKHDuelBot : public AEnemyBase, public IMKHDuelParticipant, public IInventoryInterface,
	public IEquipmentInterface, public IQuickSlotInterface
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Sets up an AI-possessed character that keeps simulating movement even while unpossessed. */
	AMKHDuelBot();

	/** Registers bIsDead for replication. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Wires the item components to each other (as AMKHPlayerController does) and listens for loadout grants. */
	virtual void PostInitializeComponents() override;

	bool IsBaiting() const { return bIsBaiting; }
	void SetIsBaiting(bool bNewIsBaiting);
	float GetBaitElapsedTime() const { return BaitElapsedTime; }
	void SetBaitElapsedTime(const float NewBaitElapsedTime) { BaitElapsedTime = NewBaitElapsedTime; } 
	
protected:
	/**
	 * Puts on the fixed skin, caches the mesh collision and layout restored on revive and, on the server, listens for
	 * the weapon equip events and the death event.
	 */
	virtual void BeginPlay() override;

	// ============================================================
	// Public Interface (IMKHDuelParticipant)
	// ============================================================

public:
	/** Revives the bot if needed, then teleports it back to its spawn. Server only. */
	virtual void ResetForNewRound(const FTransform& SpawnTransform) override;

	/**
	 * Dresses the bot and gives it the name of the skin. Called by the game mode right after the spawn, before the bot
	 * registers as a participant, so the match UI reads the right name.
	 *
	 * @param NewSkinId  Skin of DA_SkinCatalog; unknown ids keep the current name and fall back to the default look.
	 */
	void SetOpponentSkin(FName NewSkinId);

	/** Returns DisplayName. */
	virtual FText GetParticipantDisplayName() const override;

	/** A bot never votes. */
	virtual bool IsHumanParticipant() const override { return false; }

	/** Returns the levels set by SetTitleCareerLevels. */
	virtual int32 GetParticipantCareerLevels() const override { return TitleCareerLevels; }

	/**
	 * Gives the bot a title, read like a player's. Called by the game mode right after the spawn.
	 *
	 * @param NewCareerLevels  Career levels of the title, INDEX_NONE for no title.
	 */
	void SetTitleCareerLevels(int32 NewCareerLevels) { TitleCareerLevels = NewCareerLevels; }

	/** True between the bot's death and its revive at the next round. */
	UFUNCTION(BlueprintPure, Category = "Duel|Bot")
	bool IsDead() const { return bIsDead; }

	// ============================================================
	// Public Interface (Loadout)
	// ============================================================

	/**
	 * Server: grants a loadout through the bot's inventory with UInventoryComponent::GrantStartingLoadout, as for the
	 * player (items from UMKHItemSubsystem). The player's default rules then apply (FMKHDefaultQuickSlots): the rarest
	 * armor of each type is worn and the first kinds fill the quick slots, except for the weapons, when given. The bot,
	 * which must start armed, equips its first weapon quick slot. Called before BeginPlay (the bot can spawn before the
	 * world begins play), the loadout is kept and granted at BeginPlay, after the default attributes: equipment stats
	 * must scale a full Health.
	 *
	 * @param Loadout  Items to grant, from the difficulty profile (UMKHDuelBotProfile::BuildLoadout).
	 * @param Weapons  Weapons to quick slot, in order; the others stay in the bag. Empty uses the default rule.
	 */
	void EquipLoadout(const TArray<FMKHStartingItem>& Loadout, const TArray<FGameplayTag>& Weapons);

	/**
	 * Server, rematch: empties the inventory, equipment and quick slots (UInventoryComponent::ResetInventory, as for
	 * the player), then grants the loadout again.
	 *
	 * @param Loadout  Items to grant, from the difficulty profile (UMKHDuelBotProfile::BuildLoadout).
	 * @param Weapons  Weapons to quick slot (see EquipLoadout).
	 */
	void ResetLoadout(const TArray<FMKHStartingItem>& Loadout, const TArray<FGameplayTag>& Weapons);

	/**
	 * Server: makes the given weapons the ones in the weapon quick slots, in order, and puts the first in hand. The
	 * weapons must be among the items the bot owns (GetOwnedWeaponTags); the others stay in its bag.
	 *
	 * @param Weapons  Weapon tags, at most one per weapon quick slot.
	 */
	void AssignWeaponQuickSlots(const TArray<FGameplayTag>& Weapons);

	/** @return The tags of every weapon the bot owns, the one in hand included. */
	TArray<FGameplayTag> GetOwnedWeaponTags() const;

	/** @return The equipment the bot wears, reached by AMKHDuelBotController's IEquipmentInterface (damage, blocking). */
	UEquipmentManagerComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	/** @return The bot's inventory, reached by AMKHDuelBotController's IInventoryInterface (GA_QuickSlot). */
	UInventoryComponent* GetInventory() const { return InventoryComponent; }

	/** @return The bot's quick slots, reached by AMKHDuelBotController's IQuickSlotInterface (GA_QuickSlot). */
	UQuickSlotManagerComponent* GetQuickSlots() const { return QuickSlotManagerComponent; }

	/**
	 * IInventoryInterface: the item components live on the pawn, which owns the inventory list, so the list's quick
	 * slot and equipment lookups go through the pawn.
	 *
	 * @return The bot's inventory.
	 */
	virtual UInventoryComponent* GetInventoryComponent_Implementation() const override { return InventoryComponent; }

	/** @return IEquipmentInterface: the bot's equipment (see GetInventoryComponent_Implementation). */
	virtual UEquipmentManagerComponent* GetEquipmentManagerComponent_Implementation() const override { return EquipmentComponent; }

	/** @return IQuickSlotInterface: the bot's quick slots (see GetInventoryComponent_Implementation). */
	virtual UQuickSlotManagerComponent* GetQuickSlotManagerComponent_Implementation() const override { return QuickSlotManagerComponent; }

	// ============================================================
	// Public Interface (Quick slot queries, for the StateTree)
	// ============================================================

	/**
	 * The weapon quick slot of the weapon in hand.
	 *
	 * @return Input.Ability.QuickSlot.Weapon.Primary or .Secondary, or an empty tag when unarmed.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|Bot|Items")
	FGameplayTag GetActiveWeaponQuickSlot() const;

	/**
	 * The weapon quick slot a swap would equip: the other weapon slot, when it holds a weapon.
	 *
	 * @return The quick slot input to press to swap, or an empty tag when there is nothing to swap to.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|Bot|Items")
	FGameplayTag GetOtherWeaponQuickSlot() const;

	/** @return True when pressing GetOtherWeaponQuickSlot would swap the weapon now (CanUseQuickSlot). */
	UFUNCTION(BlueprintPure, Category = "Duel|Bot|Items")
	bool CanSwapWeapon() const;

	/**
	 * Whether a quick slot use runs (GA_QuickSlot active: State.General.QuickSlot.Use). A weapon swap lasts one frame
	 * (no sheathe montage), drinking a consumable lasts its montage.
	 *
	 * @return True while the bot swaps or drinks.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|Bot|Items")
	bool IsUsingQuickSlot() const;

	/**
	 * Whether pressing a quick slot input would use it now, with the player's rules: an item is bound to the slot, it
	 * is not the weapon already in hand, a consumable is off cooldown, and GA_QuickSlot can activate (not running,
	 * not blocked by jumping, falling or death).
	 *
	 * @param QuickSlotTag  Quick slot input (Input.Ability.QuickSlot.Weapon.* or .Consumable.*).
	 * @return True when the press would use the slot.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|Bot|Items")
	bool CanUseQuickSlot(const FGameplayTag& QuickSlotTag) const;

	/**
	 * Number of items left in a quick slot.
	 *
	 * @param QuickSlotTag  Quick slot input.
	 * @return The stack size (1 for a weapon), 0 when the slot is empty.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|Bot|Items")
	int32 GetQuickSlotQuantity(const FGameplayTag& QuickSlotTag) const;

	/**
	 * Finds the consumable quick slot holding a kind of item, e.g. Item.Consumable.HealthPotion.
	 *
	 * @param ItemTag  Item tag to look for (the slot's item must match it, children included).
	 * @return The first matching consumable quick slot, or an empty tag when none holds it.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|Bot|Items")
	FGameplayTag FindConsumableQuickSlot(const FGameplayTag& ItemTag) const;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Server: death event callback, bound on the bot's own ability system component (the same event the game mode
	 * scores on).
	 *
	 * @param Payload  The death event payload (unused).
	 */
	void HandleDeathEvent(const FGameplayEventData* Payload);

	/** Server: keeps bIsHoldingWeapon in sync with the weapon slot of EquipmentComponent (armed locomotion). */
	void SubscribeToEquipEvents();

	/**
	 * Server: grants a loadout through the inventory (see EquipLoadout).
	 *
	 * @param Loadout  Items to grant.
	 */
	void GrantLoadoutItems(const TArray<FMKHStartingItem>& Loadout);

	/**
	 * Server: UInventoryComponent::OnStartingLoadoutGranted callback: wears the armor and quick slots the items (the
	 * player's default rules; the bot has no saved preferences), then equips the first weapon quick slot if unarmed.
	 *
	 * @param GrantedItemIDs  The loadout's items, in loadout order.
	 */
	void HandleLoadoutGranted(const TArray<int64>& GrantedItemIDs);

	/**
	 * Server: quick slots the default items of one category into its free slots.
	 *
	 * @param ItemIDs      The loadout's items, in loadout order.
	 * @param CategoryTag  Item category root (Item.Equipment.Weapon or Item.Consumable).
	 * @param SlotTags     The category's quick slots, in fill order.
	 */
	void AssignDefaultQuickSlots(TConstArrayView<int64> ItemIDs, const FGameplayTag& CategoryTag, TConstArrayView<FGameplayTag> SlotTags);

	/**
	 * Server: equips the first weapon quick slot when no weapon is in hand. The player starts unarmed and quick
	 * equips on the first attack (AMKHPlayerController::EnsureWeaponEquipped); the bot has no such path.
	 */
	void EquipFirstWeaponIfUnarmed();

	/**
	 * Finds an owned weapon.
	 *
	 * @param WeaponTag  Weapon tag (the item must match it, children included).
	 * @return The item's id, 0 when the bot owns no such weapon.
	 */
	int64 FindOwnedWeaponID(const FGameplayTag& WeaponTag) const;

	/**
	 * Resolves the inventory entry bound to a quick slot.
	 *
	 * @param QuickSlotTag  Quick slot input.
	 * @param OutEntry      The entry, when found.
	 * @return True when an item is bound to the slot and still in the inventory.
	 */
	bool FindQuickSlotEntry(const FGameplayTag& QuickSlotTag, FRPGInventoryEntry& OutEntry) const;

	/**
	 * Server: enters or leaves the dead state: forced Dead / Idle transition of the movement state machine on every
	 * machine (as GA_Die and AMKHPlayerCharacter::ResetForNewRound do for the player), replicated flag and ragdoll.
	 *
	 * @param bNewDead  True to kill the bot, false to revive it.
	 */
	void SetDead(bool bNewDead);

	/** Replication callback: applies the death presentation on clients. */
	UFUNCTION()
	void OnRep_IsDead();

	/** Applies the ragdoll matching bIsDead on this machine (the capsule collision belongs to UDeadState). */
	void ApplyDeathPresentation();

	/**
	 * Turns the mesh into a ragdoll, or puts it back on the capsule with its original layout.
	 *
	 * @param bRagdoll  True to start simulating, false to restore the animated mesh.
	 */
	void SetMeshRagdoll(bool bRagdoll);

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** Applies the bot's skin; the skin component is local, every machine dresses the bot the same way. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Duel|Bot")
	TObjectPtr<UMKHSkinComponent> SkinComponent;
	

	/** Equipment worn by the bot; equipping an item grants its abilities to the bot's ability system component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Duel|Bot")
	TObjectPtr<UEquipmentManagerComponent> EquipmentComponent;

	/** Items the bot carries (its loadout); using one equips it or applies the consumable, as for the player. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Duel|Bot")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	/** Weapon and consumable quick slots, used by GA_QuickSlot when the bot presses a quick slot input. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Duel|Bot")
	TObjectPtr<UQuickSlotManagerComponent> QuickSlotManagerComponent;

	/** Skin of DA_SkinCatalog the bot wears until SetOpponentSkin (unknown ids fall back to the default skin). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Bot")
	FName SkinId = TEXT("Abyss");

	/** Name shown in the match UI (scoreboard) for this bot: the name of its skin (SetOpponentSkin). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Bot")
	FText DisplayName;

	/** Career levels of the bot's title, INDEX_NONE for no title. The bot only exists offline: not replicated. */
	int32 TitleCareerLevels = INDEX_NONE;

	/** Whether the mesh ragdolls on death, replacing the A_Die animation of the Dead state; off on BP_DuelBot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Bot")
	bool bRagdollOnDeath = true;

	/** Collision profile the mesh uses while ragdolling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duel|Bot", meta = (EditCondition = "bRagdollOnDeath"))
	FName RagdollCollisionProfile = TEXT("Ragdoll");

	/** True while the bot is dead; replicated so clients show the same presentation. */
	UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Duel|Bot")
	bool bIsDead = false;

	/** Server: loadout received before BeginPlay, equipped at BeginPlay once the default attributes exist. */
	TArray<FMKHStartingItem> PendingLoadout;

	/** Server: weapons the next loadout quick slots (EquipLoadout); empty uses the default rule. */
	TArray<FGameplayTag> StartingWeapons;

	/** Mesh collision profile at BeginPlay, restored when the ragdoll ends. */
	FName DefaultMeshProfile = NAME_None;

	/** Mesh transform relative to the capsule at BeginPlay, restored when the ragdoll ends. */
	FTransform DefaultMeshRelativeTransform = FTransform::Identity;
	
	bool bIsBaiting = false;
	
	float BaitElapsedTime = 0.f;
	
};
