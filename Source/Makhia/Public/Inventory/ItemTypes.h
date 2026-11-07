// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Equipment/EquipmentDefinition.h"
#include "ItemTypes.generated.h"

class UGameplayEffect;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FEquipmentItemProps
{
	GENERATED_BODY()

	/** Equipment definition class used to instantiate and configure this equipment item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UEquipmentDefinition> EquipmentClass = nullptr;
};

USTRUCT(BlueprintType)
struct FConsumableProps
{
	GENERATED_BODY()

	/** Gameplay Effect class applied when this consumable item is used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> ItemEffectClass = nullptr;

	/** Level used when applying the consumable Gameplay Effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ItemEffectLevel = 1.f;

	/** Cooldown duration (seconds) applied to the user after consuming this item; 0 disables the cooldown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CooldownTime = 0.f;

	/** Gameplay tag granted for CooldownTime while this consumable is on cooldown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag CooldownTag = FGameplayTag();

};

USTRUCT(BlueprintType)
struct FMasterItemDefinition : public FTableRowBase
{
	GENERATED_BODY()

	/** Unique gameplay tag identifying the item type/category. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag ItemTag = FGameplayTag();

	/** Localized text describing the item's gameplay purpose or lore. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText ItemDescription = FText();

	/**
	 * Rarities this item can be generated with. Empty: any rarity of the rarity table.
	 * One tag forces that rarity; several restrict the roll to them, keeping the table's RollProbability weights
	 * (list every rarity but one to exclude it). See UEquipmentRollLibrary::RollRarityAmong.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Item.Rarity"))
	FGameplayTagContainer RarityTags = FGameplayTagContainer();

	/** Current stack quantity of the item instance. */
	UPROPERTY(BlueprintReadOnly)
	int32 ItemQuantity = 0;

	/** Display name shown in UI and inventory views. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText ItemName = FText();

	/** Icon texture displayed for this item in UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Icon texture displayed for this item in the weapon HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UTexture2D> WeaponHudIcon = nullptr;

	/** Consumable-specific configuration data for this item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FConsumableProps ConsumableProps = FConsumableProps();

	/** Equipment-specific configuration data for this item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FEquipmentItemProps EquipmentItemProps = FEquipmentItemProps();

	/** Static mesh shown by the world pickup actor when this item lies on the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
	TSoftObjectPtr<UStaticMesh> PickupMesh = nullptr;

	/** Per-item transform (offset, rotation, scale) composed on top of the pickup Blueprint mesh layout; identity keeps that layout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
	FTransform PickupMeshTransform = FTransform::Identity;

};