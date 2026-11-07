// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MKHAbilityTuning.generated.h"

class UAnimMontage;

/** Struct to represent the Attack Damage Data (power, stamina dmg, status effects) of one combo strike. */
USTRUCT(BlueprintType)
struct FAttackData
{
	GENERATED_BODY()

	/** Damage multiplier of this strike, applied on top of the weapon's base damage by UExecCalc_Damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	float DamagePercent = 1.f;

	/** Damage to apply to the target stamina if he blocks the attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	float StaminaDamageValue = 20.f;

	/** Status effects applied to the hit target, only when this attack lands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "StatEffect.Perk"))
	FGameplayTagContainer TargetStatusEffectsContainer;

	/**
	 * Status effects applied to the attacker as soon as this attack starts, regardless
	 * of whether it lands. Buffs applied here (e.g. Empower) are active before the damage
	 * spec is created, so they are captured by this same attack's damage calculation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "StatEffect.Perk"))
	FGameplayTagContainer SelfActivationStatusEffectsContainer;

	/**
	 * Status effects applied to the attacker only after this attack lands a hit.
	 * They are applied after the damage pipeline, so they never affect the damage
	 * of the hit that triggered them.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "StatEffect.Perk"))
	FGameplayTagContainer SelfOnHitStatusEffectsContainer;
};

/** How a damage ability reaches its target; decides which reach fields of its tuning row apply. */
UENUM(BlueprintType)
enum class EMKHAbilityArchetype : uint8
{
	/** Weapon swing (UMKHMeleeAbility): Directional Magnetic Targeting + weapon hit scan. */
	Melee UMETA(DisplayName = "Melee"),

	/** Sphere around the attacker (UMKHAOEAbility): DMT + AOE radius. */
	AOE UMETA(DisplayName = "AOE"),

	/** Spawned projectile (UMKHProjectileAbility). */
	Projectile UMETA(DisplayName = "Projectile")
};

/**
 * Balancing and AI data of one damage ability, one row per AbilityTag (the row name is the tag name), in the
 * table set in Project Settings -> Game -> Makhia Data -> AbilityTuningTable. Read at runtime through
 * UMKHDataSubsystem::FindAbilityTuning, never copied into item entries: a change applies to every item, even
 * already generated ones. Identity, UI and roll data stay in FEquipmentAbilityDefinition, technical and
 * anti-cheat values on the ability class.
 *
 * Full model: see Documentation/AbilityTuning.md.
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAbilityTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Keeps AbilityTag and the row name together: fills an empty AbilityTag from the row name and warns when the
	 * two differ (lookups go by row name).
	 *
	 * @param InDataTable  Table owning the row.
	 * @param InRowName    Name of this row.
	 */
	virtual void OnDataTableChanged(const UDataTable* InDataTable, const FName InRowName) override;

#if WITH_EDITOR
	/**
	 * Checks the row: a valid AbilityTag, at least one strike, reach values consistent with the archetype.
	 *
	 * @param Context  Receives the errors and warnings.
	 * @return Invalid when the row cannot work in game, Valid otherwise.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** @return Distance up to which the ability can take its target: DMT radius, AOE radius or projectile range. */
	float GetEffectiveReach() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Ability this row tunes: the AbilityTag of its FEquipmentAbilityDefinition; must equal the row name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (Categories = "GameplayAbility"))
	FGameplayTag AbilityTag;

	/** How the ability reaches its target; shows the matching reach fields. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EMKHAbilityArchetype Archetype = EMKHAbilityArchetype::Melee;

	/** One entry per combo strike, in order (a single entry for non combo abilities). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TArray<FAttackData> Attacks{FAttackData()};

	/** Seconds before a skill can be used again; ignored by basic attacks (only skills have a cooldown). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown", meta = (ClampMin = "0", Units = "s"))
	float CooldownTime = 0.f;

	/** If true, Directional Magnetic Targeting turns and moves the attacker toward the best target when the attack starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reach|DMT", meta = (EditCondition = "Archetype != EMKHAbilityArchetype::Projectile", EditConditionHides))
	bool bEnableDMT = true;

	/** Radius around the attacker in which DMT looks for a target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reach|DMT", meta = (EditCondition = "Archetype != EMKHAbilityArchetype::Projectile && bEnableDMT", EditConditionHides, ClampMin = "0", Units = "cm"))
	float DMTRadius = 300.f;

	/** Maximum angle between the aim direction and a target for DMT to take it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reach|DMT", meta = (EditCondition = "Archetype != EMKHAbilityArchetype::Projectile && bEnableDMT", EditConditionHides, ClampMin = "0", ClampMax = "180", Units = "deg"))
	float DMTMaxAngle = 35.f;

	/** Distance from the target at which DMT stops the attacker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reach|DMT", meta = (EditCondition = "Archetype != EMKHAbilityArchetype::Projectile", EditConditionHides, ClampMin = "0", Units = "cm"))
	float AttackRange = 150.f;

	/** Radius of the AOE sphere centred on the attacker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reach|AOE", meta = (EditCondition = "Archetype == EMKHAbilityArchetype::AOE", EditConditionHides, ClampMin = "0", Units = "cm"))
	float AOERadius = 150.f;

	/** Distance up to which the projectile is worth throwing (aim and AI evaluation; the projectile itself flies on). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reach|Projectile", meta = (EditCondition = "Archetype == EMKHAbilityArchetype::Projectile", EditConditionHides, ClampMin = "0", Units = "cm"))
	float EffectiveRange = 1500.f;
};

/**
 * When the strikes of a damage ability land, read from the notifies of its montage (MontageToPlay of the ability
 * class): the time of every Event.Animation.HitScan.Start or .SpawnProjectile notify, in order, divided by the
 * montage's RateScale (abilities play their montage at rate 1, from the start, with no section jumps). The duel bot
 * uses it to know how long an attack takes to hit. Built and cached by UMKHDataSubsystem::FindAbilityTiming.
 *
 * Full model: see Documentation/AbilityTuning.md (Timing).
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAbilityTiming
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Reads the strike notifies of a montage.
	 *
	 * @param Montage  Montage the ability plays.
	 * @return The timing; StrikeHitTimes is empty when the montage has no strike notify.
	 */
	static FMKHAbilityTiming BuildFromMontage(const UAnimMontage& Montage);

	/** @return True when at least one strike time was found. */
	bool HasStrikes() const { return !StrikeHitTimes.IsEmpty(); }

	/**
	 * Seconds from the activation to a strike's hit.
	 *
	 * @param StrikeIndex  Strike in combo order; clamped to the known strikes.
	 * @return The strike time, or 0 when no strike is known.
	 */
	float GetTimeToHit(int32 StrikeIndex = 0) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Seconds from the activation to each strike's hit, in combo order. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Timing")
	TArray<float> StrikeHitTimes;

	/** Seconds the whole montage lasts (every strike of the combo chained). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Timing", meta = (Units = "s"))
	float MontageDuration = 0.f;
};
