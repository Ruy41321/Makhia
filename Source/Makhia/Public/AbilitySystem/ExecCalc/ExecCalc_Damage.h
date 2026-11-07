// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ExecCalc_Damage.generated.h"

struct FMKHGameplayEffectContext;
class AMKHWeaponBase;

/**
 * Custom execution calculation for determining the final damage value applied to a target.
 *
 * The full damage formula lives here, in explicit order (buff/debuff attributes are
 * decimal fractions: 0.3 = 30%):
 *   1. Base damage: weapon base damage * (attack multiplier from set-by-caller + AdditiveBaseDamage)
 *   2. Offensive multiplier: 1 + (Empower - Weaken) + critical bonus (CritDamageMod on a successful
 *      roll, or always when the attack lands in the defender's rear cone - see IsBackAttack)
 *   3. Defensive multiplier: 1 + (Exposed - Reinforced)
 *   4. Armor mitigation: 1 - UMKHAttributeSet::CalculateArmorDamageReduction(defender Armor)
 *
 * The result is written into the IncomingDamage meta-attribute; its application to
 * Health and all side effects happen in UMKHAttributeSet::PostGameplayEffectExecute.
 */
UCLASS()
class MAKHIA_API UExecCalc_Damage : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
	
public:

	UExecCalc_Damage();

	/**
	 * Executes the custom damage calculation.
	 * Extensively modifies damage to account for crits and other contextual values.
	 *
	 * @param ExecutionParams Parameters containing execution information.
	 * @param OutExecutionOutput Output structure for modifying target attributes.
	 */
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

// ============================================================
// Protected / Internal Logic
// ============================================================
private:

	/** Extracts common properties and context required for damage evaluation. */
	void RetrieveProperties(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FGameplayEffectSpec*& OutEffectSpec, FAggregatorEvaluateParameters& OutEvalParams, FMKHGameplayEffectContext*& OutRPGContext, AActor*& OutAttacker, AActor*& OutDefender) const;

	/**
	 * Dot product between the defender's flattened forward vector and the flattened
	 * direction going from the defender to the attacker: +1 when the attacker stands
	 * right in front of the defender, -1 when it stands exactly behind.
	 *
	 * Single source of truth for the front/back geometry shared by the block check and
	 * the back attack check, so the two can never disagree on where the attacker is.
	 *
	 * @return  The dot product, or 0 when either actor is invalid.
	 */
	static float ComputeDefenderFacingDot(const AActor* Attacker, const AActor* Defender);

	/** Checks if the attack was blocked based on defender's state and facing direction. */
	bool IsAttackBlocked(const FAggregatorEvaluateParameters& EvalParams, const AActor* Attacker, const AActor* Defender) const;

	/**
	 * Checks whether the attack lands on the defender's back, i.e. the attacker stands
	 * inside the rear cone behind the defender (BackAttackRearConeAngleDegrees).
	 * Back attacks always critical-hit; the cone is narrower than the half space the
	 * block check rejects, so a back attack can never be blocked.
	 *
	 * @return  True when the attacker is inside the defender's rear cone.
	 */
	bool IsBackAttack(const AActor* Attacker, const AActor* Defender) const;

	/** Modifies stamima damage based on the block stability percent of the defender's weapon. */
	void RefineStaminaDamage(const AActor* Defender, float& InOutStaminaDamage) const;

	/**
	 * Resolves the weapon currently equipped in the actor's weapon slot via the
	 * equipment manager on its instigator controller (server-only data, valid here
	 * because executions run on the authority).
	 *
	 * @param OwnerActor  Avatar actor whose equipped weapon should be resolved.
	 * @return            The equipped weapon, or nullptr if none could be found.
	 */
	static const AMKHWeaponBase* GetEquippedWeapon(const AActor* OwnerActor);

	/**
	 * Returns the base damage of the attacker's equipped weapon.
	 * Logs a warning and returns 0 when no weapon is found (resulting in zero damage).
	 */
	static float GetWeaponBaseDamage(const AActor* Attacker);

	/**
	 * Computes the pre-modifier damage of the hit:
	 * weapon base damage * (attack damage multiplier + AdditiveBaseDamage buff).
	 * The two multipliers stack additively so the weapon damage is scaled exactly once.
	 *
	 * @return  The base damage before offensive/defensive multipliers, never below zero.
	 */
	float ComputeBaseDamage(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, const FGameplayEffectSpec& EffectSpec, const AActor* Attacker) const;
	
	/**
	 * Evaluates a captured attribute and returns its magnitude, clamped to be non-negative.
	 *
	 * @param ExecutionParams  Execution parameters holding the captured attributes.
	 * @param CaptureDef       Capture definition of the attribute to evaluate.
	 * @param EvalParams       Aggregator evaluation parameters (source/target tags).
	 * @return                 The evaluated magnitude, never below zero.
	 */
	static float GetCapturedMagnitude(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FGameplayEffectAttributeCaptureDefinition& CaptureDef, const FAggregatorEvaluateParameters& EvalParams);

	/**
	 * Applies the attacker's offensive multiplier to the damage:
	 * 1 + (Empower - Weaken) + critical bonus. The crit bonus stacks additively with
	 * the net Empower, intentionally allowing the total to exceed the Empower cap.
	 *
	 * @param bGuaranteedCrit  Skips the crit roll and treats the hit as critical (back attack).
	 * @param InOutDamage      Damage value modified in place, never below zero.
	 */
	void ApplyOffensiveModifiers(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, FMKHGameplayEffectContext* RPGContext, bool bGuaranteedCrit, float& InOutDamage) const;

	/**
	 * Applies the defender's defensive multiplier to the final damage:
	 * 1 + (Exposed - Reinforced).
	 *
	 * @param InOutDamage  Damage value modified in place, never below zero.
	 */
	void ApplyDefensiveModifiers(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, float& InOutDamage) const;

	/**
	 * Scales the damage down by the defender's Armor (diminishing returns, never 100%).
	 * Armor is a permanent rating, so nothing is consumed here. See Documentation/ArmorMitigation.md.
	 *
	 * @param InOutDamage  Damage value modified in place, never below zero.
	 */
	void ApplyArmorMitigation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, float& InOutDamage) const;

	/**
	 * Rolls the critical hit and flags the result on the effect context.
	 *
	 * @param bGuaranteedCrit  When true the roll is skipped and the hit is always critical.
	 * @return                 CritDamageMod on a critical hit, 0 otherwise. The caller folds this
	 *                         bonus into the offensive multiplier instead of applying it separately.
	 */
	float RollCriticalHitBonus(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	                           const FAggregatorEvaluateParameters& EvalParams, FMKHGameplayEffectContext* RPGContext,
	                           bool bGuaranteedCrit) const;

	/** Processes and applies damage to stamina when an attack is blocked. */
	void HandleStaminaDamage(const FGameplayEffectSpec& EffectSpec, const AActor* Defender, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const;

	/** Runs the full health-damage pipeline (base damage, offensive and defensive multipliers, armor) and outputs IncomingDamage. */
	void HandleIncomingDamage(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, const FGameplayEffectSpec& EffectSpec, FMKHGameplayEffectContext* RPGContext, const AActor* Attacker, const AActor* Defender, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const;

	/** Checks if the attack was dodged based on defender's movement state (if in dodging). */
	bool IsAttackDodged(const FAggregatorEvaluateParameters& EvalParams) const;
};
