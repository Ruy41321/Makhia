// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MKHDataSettings.generated.h"

class AMKHPickupItem;
class UDataTable;
class UEquipmentStatEffects;
class UItemTypesToTables;
class UMKHCareerLadder;
class UMKHSkinCatalog;

/**
 * Project-wide gameplay data references exposed in Project Settings -> Game -> Makhia Data.
 * Holds only soft references to the data assets shared by every system; they are loaded
 * once at runtime by UMKHDataSubsystem, which is the only intended reader of these fields.
 *
 * Values are stored in Config/DefaultGame.ini under [/Script/Makhia.MKHDataSettings].
 * Full model: see Documentation/DataSettingsArchitecture.md.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Makhia Data"))
class MAKHIA_API UMKHDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Places the settings under Project Settings -> Game -> Makhia Data. */
	UMKHDataSettings();

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Convenience accessor for the project data settings.
	 *
	 * @return The CDO of UMKHDataSettings holding the configured references.
	 */
	static const UMKHDataSettings* Get();

	// ============================================================
	// Properties
	// ============================================================

	// --- Items ---

	/** Data asset mapping item category tags to the DataTables holding their FMasterItemDefinition rows. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Items")
	TSoftObjectPtr<UItemTypesToTables> ItemDefinitions;

	/** Actor class spawned for items lying on the ground (the Blueprint subclass holding the pickup visuals). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Items")
	TSoftClassPtr<AMKHPickupItem> PickupItemClass;

	// --- Equipment ---

	/** DataTable of FRarityDefinition rows used to roll and display item rarity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (RequiredAssetDataTags = "RowStructure=/Script/Makhia.RarityDefinition"))
	TSoftObjectPtr<UDataTable> RarityTable;

	/** Data asset mapping stat/ability category tags to the DataTables used to roll equipment passives and abilities. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TSoftObjectPtr<UEquipmentStatEffects> EquipmentStatEffects;

	/** DataTable of FMKHAbilityTuningRow rows: balancing and AI data of every damage ability, by AbilityTag (see Documentation/AbilityTuning.md). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (RequiredAssetDataTags = "RowStructure=/Script/Makhia.MKHAbilityTuningRow"))
	TSoftObjectPtr<UDataTable> AbilityTuningTable;

	// --- Cosmetics ---

	/** Every skin of the player character, read through UMKHSkinSubsystem (see Documentation/CharacterSkins.md). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	TSoftObjectPtr<UMKHSkinCatalog> SkinCatalog;

	/** Levels of the career, read through UMKHCareerSubsystem (see Documentation/OfflineModes.md). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Career")
	TSoftObjectPtr<UMKHCareerLadder> CareerLadder;
};
