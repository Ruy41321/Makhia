// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/MKHAbilityTuning.h"
#include "Equipment/Rarity/RarityDefinition.h"
#include "Inventory/ItemTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHDataSubsystem.generated.h"

class AMKHPickupItem;
class UDataTable;
class UEquipmentStatEffects;
class UItemTypesToTables;
class UMKHCareerLadder;
class UMKHSkinCatalog;
struct FEquipmentAbilityDefinition;
struct FPerkDefinition;

/**
 * Single runtime access point for the shared gameplay data referenced by UMKHDataSettings.
 * Loads every configured asset once when the GameInstance starts (on server and clients alike)
 * and exposes typed lookups to C++ and Blueprints, so no system needs its own asset references.
 *
 * Full model: see Documentation/DataSettingsArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Loads every asset configured in UMKHDataSettings. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Convenience accessor for C++ callers.
	 *
	 * @param WorldContextObject Any object living in a world owned by a GameInstance.
	 * @return The data subsystem, or nullptr when no GameInstance is reachable (e.g. editor preview worlds).
	 */
	static UMKHDataSubsystem* Get(const UObject* WorldContextObject);

	// --- Items ---

	/**
	 * Finds the master definition of an item.
	 *
	 * @param ItemTag Tag identifying the item (e.g. Equip.Category.Weapon.Sword.X).
	 * @return Pointer to the DataTable row, or nullptr when the tag has no definition. Do not store it.
	 */
	const FMasterItemDefinition* FindItemDefinition(const FGameplayTag& ItemTag) const;

	/**
	 * Blueprint lookup of an item master definition.
	 *
	 * @param ItemTag       Tag identifying the item.
	 * @param OutDefinition Copy of the definition, left default-constructed when not found.
	 * @return True if a definition exists for the tag.
	 */
	UFUNCTION(BlueprintPure, Category = "Data|Items")
	bool GetItemDefinition(const FGameplayTag& ItemTag, FMasterItemDefinition& OutDefinition) const;

	/**
	 * Lists every item whose definition tag matches the given tag (the tag itself or any of its children),
	 * e.g. "Item.Equipment.Weapon" lists every weapon.
	 *
	 * @param ParentTag Tag the item tags must match.
	 * @return The matching item tags in DataTable order; empty when none match.
	 */
	UFUNCTION(BlueprintPure, Category = "Data|Items")
	TArray<FGameplayTag> GetItemTagsMatching(const FGameplayTag& ParentTag) const;

	/**
	 * Item definition lookup usable from any context, including editor code without a GameInstance (actor
	 * previews, data validation): there it reads the item tables configured in UMKHDataSettings instead.
	 *
	 * @param WorldContextObject Object used to reach the GameInstance; may be nullptr in the editor.
	 * @param ItemTag            Tag identifying the item.
	 * @return Pointer to the DataTable row, or nullptr when not found. Do not store it.
	 */
	static const FMasterItemDefinition* FindItemDefinitionForContext(const UObject* WorldContextObject, const FGameplayTag& ItemTag);

	/**
	 * GetItemTagsMatching usable from any context, with the same editor fallback as FindItemDefinitionForContext.
	 *
	 * @param WorldContextObject Object used to reach the GameInstance; may be nullptr in the editor.
	 * @param ParentTag          Tag the item tags must match.
	 * @return The matching item tags; empty when none match or no item tables are reachable.
	 */
	static TArray<FGameplayTag> GetItemTagsMatchingForContext(const UObject* WorldContextObject, const FGameplayTag& ParentTag);

	/**
	 * Localized category name of an item (e.g. "Hatchet"), from UItemTypesToTables::CategoryNames: the entry of
	 * the item tag itself or of its closest parent. Never derive UI names from gameplay tags instead.
	 *
	 * @param ItemTag Tag identifying the item.
	 * @return The category name, or an empty text when no entry matches.
	 */
	UFUNCTION(BlueprintPure, Category = "Data|Items")
	FText GetItemCategoryName(const FGameplayTag& ItemTag) const;

	/**
	 * GetItemCategoryName usable from any context, with the same editor fallback as FindItemDefinitionForContext.
	 *
	 * @param WorldContextObject Object used to reach the GameInstance; may be nullptr in the editor.
	 * @param ItemTag            Tag identifying the item.
	 * @return The category name, or an empty text when no entry matches or no item tables are reachable.
	 */
	static FText GetItemCategoryNameForContext(const UObject* WorldContextObject, const FGameplayTag& ItemTag);

	/** Gets the loaded world pickup class (UMKHDataSettings::PickupItemClass), or nullptr if not configured. */
	UFUNCTION(BlueprintPure, Category = "Data|Items")
	TSubclassOf<AMKHPickupItem> GetPickupItemClass() const { return PickupItemClass; }

	// --- Rarity ---

	/**
	 * Finds the rarity row whose RarityTag exactly matches the given tag.
	 *
	 * @param RarityTag Rarity tier tag (e.g. Rarity.Legendary).
	 * @return Pointer to the DataTable row, or nullptr when not found. Do not store it.
	 */
	const FRarityDefinition* FindRarityDefinition(const FGameplayTag& RarityTag) const;

	/**
	 * Blueprint lookup of a rarity tier definition (display name, colors, roll counts).
	 *
	 * @param RarityTag     Rarity tier tag.
	 * @param OutDefinition Copy of the definition, left default-constructed when not found.
	 * @return True if a definition exists for the tag.
	 */
	UFUNCTION(BlueprintPure, Category = "Data|Rarity")
	bool GetRarityDefinition(const FGameplayTag& RarityTag, FRarityDefinition& OutDefinition) const;

	/**
	 * Blueprint lookup of a rarity tier definition usable from any context, including widget designer previews.
	 * Uses the data subsystem when a GameInstance is reachable; in the editor without one, reads the rarity table
	 * configured in UMKHDataSettings instead (same data, same lookup rule).
	 *
	 * @param WorldContextObject Object used to reach the GameInstance (e.g. the calling widget or actor).
	 * @param RarityTag          Rarity tier tag.
	 * @param OutDefinition      Copy of the definition, left default-constructed when not found.
	 * @return True if a definition exists for the tag.
	 */
	UFUNCTION(BlueprintPure, Category = "Data|Rarity", meta = (WorldContext = "WorldContextObject"))
	static bool FindRarityDefinitionForContext(const UObject* WorldContextObject, const FGameplayTag& RarityTag, FRarityDefinition& OutDefinition);

	/** Gets the loaded rarity DataTable (FRarityDefinition rows), or nullptr if not configured. */
	UFUNCTION(BlueprintPure, Category = "Data|Rarity")
	UDataTable* GetRarityTable() const { return RarityTable; }

	/**
	 * Gets how high a rarity is: its position among the rows of the rarity table, from the lowest.
	 *
	 * @param RarityTag Rarity tier tag.
	 * @return The position from 0, or -1 for an empty or unknown tag (it sorts below every rarity).
	 */
	int32 GetRarityRank(const FGameplayTag& RarityTag) const;

	// --- Equipment ---

	/** Gets the loaded equipment stat/ability roll tables asset, or nullptr if not configured. */
	UFUNCTION(BlueprintPure, Category = "Data|Equipment")
	UEquipmentStatEffects* GetEquipmentStatEffects() const { return EquipmentStatEffects; }

	// --- Abilities ---

	/**
	 * Finds the balancing and AI data of a damage ability (UMKHDataSettings::AbilityTuningTable).
	 *
	 * @param AbilityTag AbilityTag of the ability (e.g. GameplayAbility.Sword.Basic.Light), also the row name.
	 * @return Pointer to the DataTable row, or nullptr when the ability has no row. Do not store it: the table can be
	 *         edited or reimported in the editor.
	 */
	const FMKHAbilityTuningRow* FindAbilityTuning(const FGameplayTag& AbilityTag) const;

	/**
	 * Blueprint lookup of the balancing and AI data of a damage ability.
	 *
	 * @param AbilityTag AbilityTag of the ability.
	 * @param OutTuning  Copy of the row, left default-constructed when not found.
	 * @return True if the ability has a tuning row.
	 */
	UFUNCTION(BlueprintPure, Category = "Data|Abilities")
	bool GetAbilityTuning(const FGameplayTag& AbilityTag, FMKHAbilityTuningRow& OutTuning) const;

	/**
	 * Finds when the strikes of a damage ability land, read once from its montage notifies and cached per
	 * AbilityTag (FMKHAbilityTiming). The ability class is never loaded here: while it is not in memory (e.g. the
	 * abilities of a weapon never equipped) there is no timing and nothing is cached, so the next call retries.
	 *
	 * @param AbilityTag AbilityTag of the ability (its FEquipmentAbilityDefinition in the equipment ability tables).
	 * @return Pointer to the cached timing (StrikeHitTimes empty when the montage has no strike notify), or nullptr
	 *         when the ability is unknown or its class is not loaded. Do not store it: the cache grows.
	 */
	const FMKHAbilityTiming* FindAbilityTiming(const FGameplayTag& AbilityTag) const;

	/**
	 * Blueprint lookup of the strike timing of a damage ability (see FindAbilityTiming).
	 *
	 * @param AbilityTag AbilityTag of the ability.
	 * @param OutTiming  Copy of the timing, left default-constructed when not available.
	 * @return True if a timing is available.
	 */
	UFUNCTION(BlueprintPure, Category = "Data|Abilities")
	bool GetAbilityTiming(const FGameplayTag& AbilityTag, FMKHAbilityTiming& OutTiming) const;

	/**
	 * Finds the definition of a status effect (perk), e.g. StatEffect.Perk.Stun.ThunderShock: effect class,
	 * duration and value, as UMKHAbilitySystemLibrary::ApplyStatusEffects applies them.
	 *
	 * @param PerkTag Perk tag, also the row name in the perk table of the equipment stat effects.
	 * @return Pointer to the DataTable row, or nullptr when not found. Do not store it.
	 */
	const FPerkDefinition* FindPerkDefinition(const FGameplayTag& PerkTag) const;

	// --- Cosmetics ---

	/** Gets the loaded skin catalog, or nullptr if not configured. Read it through UMKHSkinSubsystem. */
	const UMKHSkinCatalog* GetSkinCatalog() const { return SkinCatalog; }

	/** Gets the loaded career levels, or nullptr if not configured. Read them through UMKHCareerSubsystem. */
	const UMKHCareerLadder* GetCareerLadder() const { return CareerLadder; }

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Returns the item tables of the reachable data subsystem or, in the editor without a GameInstance,
	 * the ones configured in UMKHDataSettings.
	 *
	 * @param WorldContextObject Object used to reach the GameInstance; may be nullptr.
	 * @return The item tables, or nullptr when none are reachable.
	 */
	static const UItemTypesToTables* ResolveItemDefinitionsForContext(const UObject* WorldContextObject);

	/**
	 * Finds the definition of an item in the given item tables.
	 *
	 * @param Definitions Item category -> DataTable mapping to search.
	 * @param ItemTag     Tag identifying the item.
	 * @return Pointer to the DataTable row, or nullptr when not found.
	 */
	static const FMasterItemDefinition* FindItemDefinitionIn(const UItemTypesToTables* Definitions, const FGameplayTag& ItemTag);

	/**
	 * Collects the tags of every definition matching a tag in the given item tables.
	 *
	 * @param Definitions Item category -> DataTable mapping to search.
	 * @param ParentTag   Tag the item tags must match.
	 * @return The matching item tags in DataTable order.
	 */
	static TArray<FGameplayTag> CollectItemTagsIn(const UItemTypesToTables* Definitions, const FGameplayTag& ParentTag);

	/**
	 * Finds the category name of an item in the given item tables, walking up the tag hierarchy.
	 *
	 * @param Definitions Item tables holding the CategoryNames map.
	 * @param ItemTag     Tag identifying the item.
	 * @return The category name of the closest matching tag, or an empty text.
	 */
	static FText FindCategoryNameIn(const UItemTypesToTables* Definitions, const FGameplayTag& ItemTag);

	/**
	 * Finds the definition of an ability in the equipment ability tables (rows of FEquipmentAbilityDefinition).
	 *
	 * @param AbilityTag AbilityTag of the ability.
	 * @return Pointer to the DataTable row, or nullptr when no table defines it.
	 */
	const FEquipmentAbilityDefinition* FindAbilityDefinition(const FGameplayTag& AbilityTag) const;

#if WITH_EDITOR
	/**
	 * Warns about every damage ability of the equipment ability tables without a tuning row, so a missing row is
	 * found when the game starts in the editor and not as a weaker attack in a match.
	 */
	void ValidateAbilityTuning() const;
#endif

	// ============================================================
	// Properties
	// ============================================================

	/** Loaded item category -> definition tables mapping (UMKHDataSettings::ItemDefinitions). */
	UPROPERTY(Transient)
	TObjectPtr<UItemTypesToTables> ItemDefinitions;

	/** Loaded world pickup class (UMKHDataSettings::PickupItemClass). */
	UPROPERTY(Transient)
	TSubclassOf<AMKHPickupItem> PickupItemClass;

	/** Loaded rarity table (UMKHDataSettings::RarityTable). */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> RarityTable;

	/** Loaded equipment roll tables (UMKHDataSettings::EquipmentStatEffects). */
	UPROPERTY(Transient)
	TObjectPtr<UEquipmentStatEffects> EquipmentStatEffects;

	/** Loaded ability tuning table (UMKHDataSettings::AbilityTuningTable). */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> AbilityTuningTable;

	/** Strike timings read from the ability montages, per AbilityTag (FindAbilityTiming); plain data, filled lazily. */
	mutable TMap<FGameplayTag, FMKHAbilityTiming> AbilityTimingCache;

	/** Loaded skin catalog (UMKHDataSettings::SkinCatalog). */
	UPROPERTY(Transient)
	TObjectPtr<UMKHSkinCatalog> SkinCatalog;

	/** Loaded career levels (UMKHDataSettings::CareerLadder). */
	UPROPERTY(Transient)
	TObjectPtr<UMKHCareerLadder> CareerLadder;
};
