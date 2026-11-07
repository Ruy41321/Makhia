// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Evaluator/MKHDuelBotVitalsEvaluator.h"

#include "AbilitySystemComponent.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "AI/MKHAttackScoring.h"
#include "AI/Controller/MKHDuelBotController.h"
#include "Character/CharacterBase.h"

// ============================================================
// Public Interface
// ============================================================

void FMKHDuelBotVitalsEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	// Normal for a few ticks at round start: Verbose, not Warning.
	if (!IsValid(Data.BotCharacter) || !IsValid(Data.Opponent))
	{
		UE_LOG(LogMKHAI, Verbose, TEXT("MKHDuelBotVitalsEvaluator: no bot (%s) or opponent (%s) yet."),
			*GetNameSafe(Data.BotCharacter), *GetNameSafe(Data.Opponent));
		return;
	}

	UpdateCharacterAttributes(*(Data.BotCharacter), Data.BotAttributes);
	UpdateCharacterAttributes(*(Data.Opponent), Data.OpponentAttributes);

	// Multipliers are already within their attribute caps (the ability system clamps Empower, Weaken...).
	Data.DamageAdvantage = FMKHAttackScoring::ComputeDamageAdvantage(
		Data.BotAttributes.EffectiveOutputDamageMultiplier, Data.BotAttributes.EffectiveIncomingDamageMultiplier,
		Data.OpponentAttributes.EffectiveOutputDamageMultiplier, Data.OpponentAttributes.EffectiveIncomingDamageMultiplier);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void FMKHDuelBotVitalsEvaluator::UpdateCharacterAttributes(const ACharacterBase& Character,
	FMKHCharacterAttributesData& OutAttributes)
{
	const UAbilitySystemComponent* AbilitySystem = Character.GetAbilitySystemComponent();
	if (!IsValid(AbilitySystem))
	{
		OutAttributes = FMKHCharacterAttributesData();
		return;
	}
	const UAbilitySystemComponent& ASC = *AbilitySystem;

	// Health, armor, stamina.
	OutAttributes.HealthRatio = GetAttributeRatio(ASC, UMKHAttributeSet::GetHealthAttribute(), UMKHAttributeSet::GetMaxHealthAttribute());
	OutAttributes.CurrentHealth = ASC.GetNumericAttribute(UMKHAttributeSet::GetHealthAttribute());
	OutAttributes.CurrentArmorMitigation = UMKHAttributeSet::CalculateArmorDamageReduction(ASC.GetNumericAttribute(UMKHAttributeSet::GetArmorAttribute()));
	OutAttributes.StaminaRatio = GetAttributeRatio(ASC, UMKHAttributeSet::GetStaminaAttribute(), UMKHAttributeSet::GetMaxStaminaAttribute());
	OutAttributes.CurrentStamina = ASC.GetNumericAttribute(UMKHAttributeSet::GetStaminaAttribute());
	OutAttributes.DodgeStaminaCostRatio = GetAttributeRatio(ASC, UMKHAttributeSet::GetDodgeStaminaCostAttribute(), UMKHAttributeSet::GetMaxStaminaAttribute());

	// Critical hits.
	OutAttributes.CritChance = ASC.GetNumericAttribute(UMKHAttributeSet::GetCritChanceAttribute());
	OutAttributes.CritDamageMod = ASC.GetNumericAttribute(UMKHAttributeSet::GetCritDamageModAttribute());

	// Buffs and debuffs, each against its cap.
	OutAttributes.AdditiveBaseDamageRatio = GetAttributeRatio(ASC, UMKHAttributeSet::GetAdditiveBaseDamageAttribute(), UMKHAttributeSet::GetMaxAdditiveBaseDamageAttribute());
	OutAttributes.EmpowerRatio = GetAttributeRatio(ASC, UMKHAttributeSet::GetEmpowerAttribute(), UMKHAttributeSet::GetMaxEmpowerAttribute());
	OutAttributes.WeakenRatio = GetAttributeRatio(ASC, UMKHAttributeSet::GetWeakenAttribute(), UMKHAttributeSet::GetMaxWeakenAttribute());
	OutAttributes.ReinforcedRatio = GetAttributeRatio(ASC, UMKHAttributeSet::GetReinforcedAttribute(), UMKHAttributeSet::GetMaxReinforcedAttribute());
	OutAttributes.ExposedRatio = GetAttributeRatio(ASC, UMKHAttributeSet::GetExposedAttribute(), UMKHAttributeSet::GetMaxExposedAttribute());

	// Same formulas as UExecCalc_Damage::ApplyOffensiveModifiers / ApplyDefensiveModifiers.
	const float Empower = ASC.GetNumericAttribute(UMKHAttributeSet::GetEmpowerAttribute());
	const float Weaken = ASC.GetNumericAttribute(UMKHAttributeSet::GetWeakenAttribute());
	const float Exposed = ASC.GetNumericAttribute(UMKHAttributeSet::GetExposedAttribute());
	const float Reinforced = ASC.GetNumericAttribute(UMKHAttributeSet::GetReinforcedAttribute());
	OutAttributes.EffectiveOutputDamageMultiplier = FMath::Max(1.f + (Empower - Weaken), 0.f);
	OutAttributes.EffectiveIncomingDamageMultiplier = FMath::Max(1.f + (Exposed - Reinforced), 0.f);
}

float FMKHDuelBotVitalsEvaluator::GetAttributeRatio(const UAbilitySystemComponent& AbilitySystem,
	const FGameplayAttribute& Value, const FGameplayAttribute& MaxValue)
{
	const float Max = AbilitySystem.GetNumericAttribute(MaxValue);
	if (FMath::IsNearlyZero(Max))
	{
		return 0.f;
	}
	return AbilitySystem.GetNumericAttribute(Value) / Max;
}
