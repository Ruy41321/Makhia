// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MKHAbilityGrantPayload.generated.h"

/**
 * Per-grant data attached to an ability spec via FGameplayAbilitySpec::SourceObject.
 *
 * Equipment can grant the same ability class to different weapons with different data
 * (dynamic skill input tags, skill flag, cooldown tags). Writing that on the ability
 * CDO would make it shared mutable state across every owner of the class, so
 * GrantEquipmentAbility fills this payload instead and the instance reads it back in
 * OnGiveAbility, which runs synchronously on the authority.
 *
 * Not replicated: SourceObject doesn't resolve on remote clients, but that's fine since
 * this data (cooldowns, projectile spawning) is only used server-side anyway. Balancing data
 * (strikes, cooldown time, reach) is not here: every machine reads it from the ability tuning
 * table through the AbilityTag added to the spec's dynamic source tags (Documentation/AbilityTuning.md).
 */
UCLASS()
class MAKHIA_API UMKHAbilityGrantPayload : public UObject
{
	GENERATED_BODY()

public:

	// ============================================================
	// Properties
	// ============================================================

	/** Input tag bound to the granted spec (dynamic for skills, CDO default otherwise). */
	UPROPERTY()
	FGameplayTag InputTag;

	/** Projectile identifier consumed by UMKHProjectileAbility instances. */
	UPROPERTY()
	FGameplayTag ProjectileToSpawnTag;

	/** True when the granted ability is a skill (custom cooldown pipeline). */
	UPROPERTY()
	bool bIsSkillAbility = false;

	/** Tags granted while the skill cooldown effect is active. */
	UPROPERTY()
	FGameplayTagContainer CooldownTags;
};
