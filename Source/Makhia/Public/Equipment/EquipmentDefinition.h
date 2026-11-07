// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "EquipmentDefinition.generated.h"

class UEquipmentInstance;
class AEquipmentActor;

/** Defines one actor to spawn when the equipment instance gets equipped. */
USTRUCT()
struct FEquipmentActorToSpawn
{
	GENERATED_BODY()

	/** Soft class of the actor to spawn and attach to the owning character. */
	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<AEquipmentActor> EquipmentClass = nullptr;

	/** Socket or bone name on the character mesh where the actor is attached. */
	UPROPERTY(EditDefaultsOnly)
	FName AttachName = FName();
};

/**
 * Immutable definition asset that describes slot, visuals, base damage, and roll pools for one equipment item type.
 */
UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UEquipmentDefinition : public UObject
{
	GENERATED_BODY()
	
public:

	/** Unique gameplay tag that identifies this equipment item type. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Info")
	FGameplayTag ItemTag;

	/** Slot tag that determines where this item can be equipped. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Info")
	FGameplayTag SlotTag;

	/** Base weapon damage applied to spawned weapon actors from this definition. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Combat")
	float BaseDamage = 0.f;
	
	/** Value for which the stamina damage is reduced (stam_damage * (1 - BlockStabilityPercent)) */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Combat", meta = (UIMin = "0.0", UIMax = "1.0"))
	float BlockStabilityPercent = 0.f;
	
	/** Base armor value applied to the player when equipping this item. Armor only. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Combat")
	float BaseArmorGiven = 0.f;
	
	/** Runtime instance class used to manage equip/unequip callbacks and spawned actors. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Info")
	TSubclassOf<UEquipmentInstance> InstanceType;

	/** Candidate stat effect tags used by the roll system for this equipment definition. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Stats")
	FGameplayTagContainer PossibleStatRolls;

	/** Visual actors that are spawned and attached while this item is equipped. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Actors")
	TArray<FEquipmentActorToSpawn> ActorsToSpawn;

	/** Candidate active ability tags used by the roll system for this equipment definition. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Ability")
	FGameplayTagContainer PossibleAbilityRolls;
	
	/** Ability tags always granted by this item regardless of random rolls. */
	UPROPERTY(EditDefaultsOnly, Category = "Custom Values | Ability")
	FGameplayTagContainer BasicAbilitiesGranted;
};
