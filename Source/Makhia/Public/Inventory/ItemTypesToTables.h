// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ItemTypesToTables.generated.h"

/**
 * Item definition tables grouped by item category, plus the player-facing name of each category.
 * Referenced once by UMKHDataSettings and read through UMKHDataSubsystem.
 */
UCLASS()
class MAKHIA_API UItemTypesToTables : public UDataAsset
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Item category tag -> DataTable of FMasterItemDefinition rows for that category. */
	UPROPERTY(EditDefaultsOnly)
	TMap<FGameplayTag, TObjectPtr<UDataTable>> TagsToTables;

	/**
	 * Localized category name shown in the UI (tooltips, pickups), keyed by item tag.
	 * An item uses the entry of its closest parent tag (e.g. Item.Equipment.Weapon.Hatchet);
	 * items without a matching entry show no category.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Display")
	TMap<FGameplayTag, FText> CategoryNames;
};
