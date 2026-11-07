// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"

#include "MKHLogChannels.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "AbilitySystem/MKHAbilityTypes.h"
#include "Equipment/EquipmentInstance.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Equipment/Weapon/MKHWeaponBase.h"
#include "GameFramework/Controller.h"
#include "Interfaces/EquipmentInterface.h"

struct RPGDamageStatics
{
	// Source Captures
	DECLARE_ATTRIBUTE_CAPTUREDEF(CritChance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(CritDamageMod);
	DECLARE_ATTRIBUTE_CAPTUREDEF(AdditiveBaseDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Empower);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Weaken);

	// Target Captures
	DECLARE_ATTRIBUTE_CAPTUREDEF(IncomingDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(IncomingStaminaDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Reinforced);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Exposed);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);

	RPGDamageStatics()
	{
		// Source Defines (snapshot: read the attacker's buffs when the spec is created)
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, CritChance, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, CritDamageMod, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, AdditiveBaseDamage, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, Empower, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, Weaken, Source, true);

		// Target Defines (no snapshot: read the defender's state when the effect executes)
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, IncomingDamage, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, IncomingStaminaDamage, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, Reinforced, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, Exposed, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMKHAttributeSet, Armor, Target, false);
	}
};

static const RPGDamageStatics& DamageStatics()
{
	static RPGDamageStatics DStatics;
	return DStatics;
}

/**
 * Total aperture of the rear cone that qualifies as a hit on the defender's back:
 * 120 degrees means the attacker must stand within 60 degrees of the defender's
 * backward vector. Narrower than the front half space the block check rejects, so the
 * two outcomes stay mutually exclusive with a neutral side band in between.
 */
static constexpr float BackAttackRearConeAngleDegrees = 120.f;

// ============================================================
// Lifecycle
// ============================================================

UExecCalc_Damage::UExecCalc_Damage()
{
	// Source Captures
	RelevantAttributesToCapture.Add(DamageStatics().CritChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritDamageModDef);
	RelevantAttributesToCapture.Add(DamageStatics().AdditiveBaseDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().EmpowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().WeakenDef);

	// Target Captures
	RelevantAttributesToCapture.Add(DamageStatics().IncomingDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().IncomingStaminaDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().ReinforcedDef);
	RelevantAttributesToCapture.Add(DamageStatics().ExposedDef);
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
}

// ============================================================
// Public Interface
// ============================================================

void UExecCalc_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, 
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec* EffectSpec = nullptr;
	FAggregatorEvaluateParameters EvalParams;
	FMKHGameplayEffectContext* RPGContext = nullptr;
	AActor* Attacker = nullptr;
	AActor* Defender = nullptr;

	RetrieveProperties(ExecutionParams, EffectSpec, EvalParams, RPGContext, Attacker, Defender);

	// If is dead dont do anything
	if (EvalParams.TargetTags && EvalParams.TargetTags->HasTag(MKHGameplayTags::State::Movement::Dead))
	{
		return;
	}
	
	if (!EffectSpec)
	{
		return;
	}
	
	if (IsAttackBlocked(EvalParams, Attacker, Defender))
	{
		HandleStaminaDamage(*EffectSpec, Defender, OutExecutionOutput);
	}
	else
	{
		HandleIncomingDamage(ExecutionParams, EvalParams, *EffectSpec, RPGContext, Attacker, Defender, OutExecutionOutput);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UExecCalc_Damage::RetrieveProperties(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FGameplayEffectSpec*& OutEffectSpec, FAggregatorEvaluateParameters& OutEvalParams, FMKHGameplayEffectContext*& OutRPGContext, AActor*& OutAttacker, AActor*& OutDefender) const
{
	OutEffectSpec = &ExecutionParams.GetOwningSpec();
	
	OutEvalParams.SourceTags = OutEffectSpec->CapturedSourceTags.GetAggregatedTags();
	OutEvalParams.TargetTags = OutEffectSpec->CapturedTargetTags.GetAggregatedTags();

	const FGameplayEffectContextHandle EffectContextHandle = OutEffectSpec->GetContext();
	OutRPGContext = FMKHGameplayEffectContext::GetEffectContext(EffectContextHandle);
	
	OutAttacker = Cast<AActor>(EffectContextHandle.GetSourceObject());
	
	const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	OutDefender = IsValid(TargetASC) ? TargetASC->GetAvatarActor() : nullptr;
}

float UExecCalc_Damage::ComputeDefenderFacingDot(const AActor* Attacker, const AActor* Defender)
{
	if (!IsValid(Attacker) || !IsValid(Defender))
	{
		return 0.f;
	}

	FVector DefenderForward = Defender->GetActorForwardVector();
	DefenderForward.Z = 0.0f;
	DefenderForward.Normalize();

	FVector AttackDirection = Attacker->GetActorLocation() - Defender->GetActorLocation();
	AttackDirection.Z = 0.0f;
	AttackDirection.Normalize();

	return FVector::DotProduct(DefenderForward, AttackDirection);
}

bool UExecCalc_Damage::IsAttackBlocked(const FAggregatorEvaluateParameters& EvalParams, const AActor* Attacker, const AActor* Defender) const
{
	if (!EvalParams.TargetTags || !EvalParams.TargetTags->HasTag(MKHGameplayTags::State::Movement::Blocking))
	{
		return false;
	}

	if (!IsValid(Attacker) || !IsValid(Defender))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("UExecCalc_Damage::IsAttackBlocked - Attacker or Defender is invalid."));
		return false;
	}

	return ComputeDefenderFacingDot(Attacker, Defender) > 0.0f;
}

bool UExecCalc_Damage::IsBackAttack(const AActor* Attacker, const AActor* Defender) const
{
	if (!IsValid(Attacker) || !IsValid(Defender))
	{
		return false;
	}

	// Half aperture measured from the defender's backward vector, expressed as a dot threshold.
	const float RearDotThreshold = -FMath::Cos(FMath::DegreesToRadians(BackAttackRearConeAngleDegrees * 0.5f));

	return ComputeDefenderFacingDot(Attacker, Defender) <= RearDotThreshold;
}

void UExecCalc_Damage::HandleStaminaDamage(const FGameplayEffectSpec& EffectSpec, const AActor* Defender, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	float StaminaDamage = EffectSpec.GetSetByCallerMagnitude(MKHGameplayTags::Combat::Data_StaminaDamage);
	StaminaDamage = FMath::Max<float>(StaminaDamage, 0.f);
	
	RefineStaminaDamage(Defender, StaminaDamage);
	
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(DamageStatics().IncomingStaminaDamageProperty, EGameplayModOp::Additive, StaminaDamage));
}

const AMKHWeaponBase* UExecCalc_Damage::GetEquippedWeapon(const AActor* OwnerActor)
{
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	const AController* InstigatorController = OwnerActor->GetInstigatorController();
	if (!IsValid(InstigatorController) || !InstigatorController->Implements<UEquipmentInterface>())
	{
		return nullptr;
	}

	UEquipmentManagerComponent* EquipmentManager = IEquipmentInterface::Execute_GetEquipmentManagerComponent(InstigatorController);
	if (!IsValid(EquipmentManager))
	{
		return nullptr;
	}

	UEquipmentInstance* WeaponInstance = EquipmentManager->GetEquipmentInstanceBySlot(MKHGameplayTags::Equip::WeaponSlot);
	if (!IsValid(WeaponInstance))
	{
		return nullptr;
	}

	const TArray<TObjectPtr<AActor>>& SpawnedActors = WeaponInstance->GetSpawnedActors();
	if (SpawnedActors.IsEmpty() || !IsValid(SpawnedActors[0]))
	{
		return nullptr;
	}

	return Cast<AMKHWeaponBase>(SpawnedActors[0]);
}

void UExecCalc_Damage::RefineStaminaDamage(const AActor* Defender, float& InOutStaminaDamage) const
{
	const AMKHWeaponBase* OwningWeapon = GetEquippedWeapon(Defender);
	if (!IsValid(OwningWeapon))
	{
		return;
	}

	InOutStaminaDamage *= (1.f - OwningWeapon->GetBlockStabilityPercent());
}

float UExecCalc_Damage::GetWeaponBaseDamage(const AActor* Attacker)
{
	const AMKHWeaponBase* OwningWeapon = GetEquippedWeapon(Attacker);
	if (!IsValid(OwningWeapon))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("UExecCalc_Damage::GetWeaponBaseDamage - No equipped weapon found on attacker '%s'. Damage will be zero."), *GetNameSafe(Attacker));
		return 0.f;
	}

	return OwningWeapon->GetWeaponDamage();
}

float UExecCalc_Damage::GetCapturedMagnitude(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FGameplayEffectAttributeCaptureDefinition& CaptureDef, const FAggregatorEvaluateParameters& EvalParams)
{
	float Magnitude = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(CaptureDef, EvalParams, Magnitude);
	return FMath::Max<float>(Magnitude, 0.f);
}

float UExecCalc_Damage::ComputeBaseDamage(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, const FGameplayEffectSpec& EffectSpec, const AActor* Attacker) const
{
	// The set-by-caller carries only the attack's own damage multiplier (e.g. 1.5 for a heavy attack).
	float AttackMultiplier = EffectSpec.GetSetByCallerMagnitude(MKHGameplayTags::Combat::Data_Damage);
	AttackMultiplier = FMath::Max<float>(AttackMultiplier, 0.f);

	// AdditiveBaseDamage stacks additively with the attack multiplier (1.5 + 0.3 = 1.8),
	// so the weapon's base damage is scaled exactly once.
	const float AdditiveBaseDamage = GetCapturedMagnitude(ExecutionParams, DamageStatics().AdditiveBaseDamageDef, EvalParams);

	return GetWeaponBaseDamage(Attacker) * (AttackMultiplier + AdditiveBaseDamage);
}

void UExecCalc_Damage::ApplyOffensiveModifiers(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, FMKHGameplayEffectContext* RPGContext, bool bGuaranteedCrit, float& InOutDamage) const
{
	// Empower and Weaken balance each other; the critical bonus stacks additively on top,
	// which intentionally lets a crit push the total multiplier beyond the Empower cap.
	const float Empower = GetCapturedMagnitude(ExecutionParams, DamageStatics().EmpowerDef, EvalParams);
	const float Weaken = GetCapturedMagnitude(ExecutionParams, DamageStatics().WeakenDef, EvalParams);
	const float CritBonus = RollCriticalHitBonus(ExecutionParams, EvalParams, RPGContext, bGuaranteedCrit);

	InOutDamage = FMath::Max<float>(InOutDamage * (1.f + (Empower - Weaken) + CritBonus), 0.f);
}

void UExecCalc_Damage::ApplyDefensiveModifiers(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, float& InOutDamage) const
{
	// Exposed and Reinforced balance each other; the net result scales the final (post-crit) damage.
	const float Exposed = GetCapturedMagnitude(ExecutionParams, DamageStatics().ExposedDef, EvalParams);
	const float Reinforced = GetCapturedMagnitude(ExecutionParams, DamageStatics().ReinforcedDef, EvalParams);

	InOutDamage = FMath::Max<float>(InOutDamage * (1.f + (Exposed - Reinforced)), 0.f);
}

void UExecCalc_Damage::ApplyArmorMitigation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, float& InOutDamage) const
{
	// Armor is never consumed: it only scales the hit. Formula in Documentation/ArmorMitigation.md.
	const float Armor = GetCapturedMagnitude(ExecutionParams, DamageStatics().ArmorDef, EvalParams);

	InOutDamage *= 1.f - UMKHAttributeSet::CalculateArmorDamageReduction(Armor);
}

float UExecCalc_Damage::RollCriticalHitBonus(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, FMKHGameplayEffectContext* RPGContext, bool bGuaranteedCrit) const
{
	const float CritChance = GetCapturedMagnitude(ExecutionParams, DamageStatics().CritChanceDef, EvalParams);
	const float CritDamageMod = GetCapturedMagnitude(ExecutionParams, DamageStatics().CritDamageModDef, EvalParams);

	const bool bCriticalHit = bGuaranteedCrit || FMath::RandRange(1, 100) <= (CritChance * 100);

	if (RPGContext)
	{
		RPGContext->SetIsCriticalHit(bCriticalHit);
	}

	return bCriticalHit ? CritDamageMod : 0.f;
}

void UExecCalc_Damage::HandleIncomingDamage(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FAggregatorEvaluateParameters& EvalParams, const FGameplayEffectSpec& EffectSpec, FMKHGameplayEffectContext* RPGContext, const AActor* Attacker, const AActor* Defender, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// Base damage: weapon damage * (attack multiplier + AdditiveBaseDamage buff).
	float Damage = ComputeBaseDamage(ExecutionParams, EvalParams, EffectSpec, Attacker);

	// Executions stay pure: only compute values and annotate the context here. Side effects
	// (hit/dodge events, status effects) live in UMKHAttributeSet::PostGameplayEffectExecute.
	if (IsAttackDodged(EvalParams))
	{
		// Flag the dodge and forward the would-be damage as event magnitude; the
		// attribute set reads bDodged and skips applying it.
		if (RPGContext)
		{
			RPGContext->SetIsDodged(true);
		}
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(DamageStatics().IncomingDamageProperty, EGameplayModOp::Additive, Damage));
		return;
	}

	// A hit landing in the defender's rear cone always crits.
	const bool bBackAttack = IsBackAttack(Attacker, Defender);
	if (bBackAttack)
	{
		UE_LOG(LogMKHAbility, Verbose, TEXT("UExecCalc_Damage - Back attack from '%s' on '%s': guaranteed critical hit."),
			*GetNameSafe(Attacker), *GetNameSafe(Defender));
	}

	// Damage pipeline, in order:
	//   1. Offensive multiplier from the attacker: 1 + (Empower - Weaken) + crit bonus
	//   2. Defensive multiplier from the defender: 1 + (Exposed - Reinforced)
	//   3. Armor mitigation from the defender: 1 - Armor / (Armor + ArmorForHalfReduction)
	ApplyOffensiveModifiers(ExecutionParams, EvalParams, RPGContext, bBackAttack, Damage);
	ApplyDefensiveModifiers(ExecutionParams, EvalParams, Damage);
	ApplyArmorMitigation(ExecutionParams, EvalParams, Damage);

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(DamageStatics().IncomingDamageProperty, EGameplayModOp::Additive, Damage));
}

bool UExecCalc_Damage::IsAttackDodged(const FAggregatorEvaluateParameters& EvalParams) const
{
	return (EvalParams.TargetTags &&
		EvalParams.TargetTags->HasTag(MKHGameplayTags::State::Movement::Dodging));
}
