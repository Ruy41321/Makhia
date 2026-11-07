// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "RarityDefinition.generated.h"

USTRUCT(BlueprintType)
struct FRarityColors
{
	GENERATED_BODY()
	
	/** Color used in the UI for this rarity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor MainColor = FLinearColor::White;
	
	/** Material instance used in the UI as banner background for this rarity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> MI_Banner = nullptr;
	
	/** Material instance used in the UI as square background for this rarity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> MI_Square = nullptr;
	
};

/**
 * Row struct for the Rarity DataTable.
 * Each row defines a rarity tier with its visual info, roll counts, and probability.
 */
USTRUCT(BlueprintType)
struct FRarityDefinition : public FTableRowBase
{
	GENERATED_BODY()

	/** Gameplay tag that uniquely identifies this rarity tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag RarityTag = FGameplayTag();

	/** Display name shown in the UI (e.g. "Common", "Legendary"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName = FText();
	
	/** Color and material instance for the UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FRarityColors RarityColors;

	/** Number of passive stat effects rolled for items of this rarity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 NumPassiveStats = 0;

	/** Number of active abilities rolled for weapons of this rarity (armour always gets 0). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 NumActiveAbilities = 0;

	/** Relative probability weight used when rolling a rarity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float RollProbability = 0.f;
};
