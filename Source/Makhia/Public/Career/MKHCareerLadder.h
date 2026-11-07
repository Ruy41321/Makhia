// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MKHCareerLadder.generated.h"

/** One level of the career: who the player faces and the title won by beating it. */
USTRUCT(BlueprintType)
struct FMKHCareerLevel
{
	GENERATED_BODY()

	/** Difficulty of the opponent, matched against UMKHDuelBotProfile::GetProfileId. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Career")
	FName BotProfileId;

	/** Skin of DA_SkinCatalog the opponent wears; it also gives the opponent its name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Career")
	FName BotSkinId;

	/** Weapons the opponent quick slots, in order (the first one in hand), e.g. Item.Equipment.Weapon.Sword; each must match a weapon of its profile's loadout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Career", meta = (Categories = "Item.Equipment.Weapon"))
	TArray<FGameplayTag> BotWeapons;

	/** Title won by beating this level. Not awarded yet: see Documentation/OfflineModes.md. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Career")
	FText TitleAwarded;
};

/**
 * Levels of the career in play order (Project Settings -> Game -> Makhia Data -> CareerLadder), read through
 * UMKHCareerSubsystem. Full model: see Documentation/OfflineModes.md.
 */
UCLASS(BlueprintType)
class MAKHIA_API UMKHCareerLadder : public UDataAsset
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Returns every level, in play order. */
	const TArray<FMKHCareerLevel>& GetLevels() const { return Levels; }

	/** Returns the title of a player who has not beaten any level yet. */
	const FText& GetStartingTitle() const { return StartingTitle; }

protected:
	// ============================================================
	// Properties
	// ============================================================

	/** Title of a player who has not beaten any level yet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Career")
	FText StartingTitle;

	/** Levels in play order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Career", meta = (TitleProperty = "BotProfileId"))
	TArray<FMKHCareerLevel> Levels;
};
