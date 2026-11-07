// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inventory/MKHItemEntry.h"
#include "Inventory/MKHItemRarityRule.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHItemSubsystem.generated.h"

class AMKHPickupItem;
class UDataTable;
class UMKHDataSubsystem;
struct FMasterItemDefinition;
struct FRarityDefinition;

/**
 * Server-side authority over item instances: hands out ItemIDs that are unique across every
 * player for the whole game session, and generates new items (rarity, passive stats and abilities
 * rolled from the data exposed by UMKHDataSubsystem).
 * Every item source (inventory grants, world spawns, drops) goes through this subsystem.
 * It also tracks the world pickups alive on the server, so they can all be removed at once.
 *
 * Full model: see Documentation/PickupSystemArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHItemSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Convenience accessor for C++ callers.
	 *
	 * @param WorldContextObject Any object living in a world owned by a GameInstance.
	 * @return The item subsystem, or nullptr when no GameInstance is reachable.
	 */
	static UMKHItemSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Server only: returns a new ItemID, sequential and never reused during the game session.
	 *
	 * @return The new ID, or 0 when called without authority.
	 */
	int64 GenerateItemID();

	/**
	 * Server only: creates a new instance of an item definition with a fresh ItemID.
	 * Equipment gets its rarity (decided by RarityRule: by default pre-defined by the definition or rolled),
	 * passive stats and abilities rolled; any other item only copies the pre-defined rarity and ignores RarityRule.
	 *
	 * @param ItemTag    Tag of the item definition to instantiate.
	 * @param Quantity   Stack size of the new instance.
	 * @param RarityRule Optional rarity override for equipment (e.g. from an item spawn point).
	 * @return The generated item, or an invalid entry (ItemID 0) without authority or definition.
	 */
	FMKHItemEntry GenerateItem(const FGameplayTag& ItemTag, int32 Quantity, const FMKHItemRarityRule& RarityRule = FMKHItemRarityRule());

	// --- World pickups ---

	/**
	 * Server only: starts tracking a world pickup. Called by AMKHPickupItem when it begins play.
	 *
	 * @param Pickup The pickup to track.
	 */
	void RegisterPickup(AMKHPickupItem* Pickup);

	/**
	 * Server only: stops tracking a world pickup. Called by AMKHPickupItem when it ends play.
	 *
	 * @param Pickup The pickup to forget.
	 */
	void UnregisterPickup(AMKHPickupItem* Pickup);

	/** Server only: destroys every world pickup currently tracked (e.g. when the match is over). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Pickup")
	void DestroyAllPickups();

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns true when this GameInstance runs the authoritative world (server or standalone). */
	bool HasItemAuthority() const;

	/**
	 * Fills the equipment-specific fields of a new item: rarity, passive stats and, for weapons,
	 * guaranteed plus rolled abilities with their dynamic skill input tags.
	 *
	 * @param NewItem       Item being generated.
	 * @param ItemDef       Master definition of the item.
	 * @param DataSubsystem Source of the rarity table and roll tables.
	 * @param RarityRule    How the rarity is decided (see ResolveEntryRarity).
	 */
	void RollEquipmentEntry(FMKHItemEntry& NewItem, const FMasterItemDefinition& ItemDef, const UMKHDataSubsystem& DataSubsystem,
		const FMKHItemRarityRule& RarityRule) const;

	/**
	 * Picks the rarity of a new item among its definition's RarityTags (weighted by the rarity table).
	 *
	 * @param ItemDef        Master definition of the item.
	 * @param DataSubsystem  Source of the rarity table.
	 * @return The picked rarity, or an empty tag when RarityTags is empty or names no rarity of the table.
	 */
	FGameplayTag PickDefinitionRarity(const FMasterItemDefinition& ItemDef, const UMKHDataSubsystem& DataSubsystem) const;

	/**
	 * Resolves the rarity of a new item. A Fixed or Weighted rule decides it; when the rule cannot be applied
	 * (unknown rarities) it falls back, with a warning, to the default: the rarity already picked from the
	 * definition's RarityTags is looked up directly, otherwise a rarity is rolled from the rarity table.
	 *
	 * @param Item        The item whose RarityTag drives the default resolution.
	 * @param RarityTable DataTable of FRarityDefinition rows.
	 * @param RarityRule  Optional override of the default resolution.
	 * @return The resolved rarity row, or nullptr if none could be resolved.
	 */
	const FRarityDefinition* ResolveEntryRarity(const FMKHItemEntry& Item, const UDataTable* RarityTable, const FMKHItemRarityRule& RarityRule) const;

	/**
	 * Applies a Fixed or Weighted rarity rule.
	 *
	 * @param RarityTable DataTable of FRarityDefinition rows.
	 * @param RarityRule  The rule to apply.
	 * @return The rarity chosen by the rule, or nullptr for the Default mode or when the rule cannot be applied.
	 */
	const FRarityDefinition* ApplyRarityRule(const UDataTable* RarityTable, const FMKHItemRarityRule& RarityRule) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Last ItemID handed out by GenerateItemID. */
	int64 LastItemID = 0;

	/** World pickups alive on the server; weak so pickups left behind by level travel just expire. */
	TArray<TWeakObjectPtr<AMKHPickupItem>> RegisteredPickups;
};
