// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/MKHAttackCandidateBuilder.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHAbilityTuning.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "Data/MKHDataSubsystem.h"
#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentTypes.h"
#include "Inventory/ItemTypes.h"
#include "Inventory/MKHItemEntry.h"

// ============================================================
// Public Interface
// ============================================================

bool FMKHAttackCandidateBuilder::BuildCandidate(const UMKHDataSubsystem& Data, const FEquipmentAbilityDefinition& Ability,
	const float WeaponBaseDamage, FMKHAttackCandidateInput& OutCandidate)
{
	const FMKHAbilityTuningRow* Tuning = Data.FindAbilityTuning(Ability.AbilityTag);
	if (Tuning == nullptr)
	{
		return false;
	}

	// No timing while the ability class is not loaded: strikes count as instant until it is.
	const FMKHAbilityTiming* Timing = Data.FindAbilityTiming(Ability.AbilityTag);

	OutCandidate = FMKHAttackCandidateInput();
	OutCandidate.AbilityTag = Ability.AbilityTag;
	OutCandidate.Reach = Tuning->GetEffectiveReach();
	OutCandidate.WeaponBaseDamage = WeaponBaseDamage;
	OutCandidate.CooldownTime = Ability.bIsSkillAbility ? Tuning->CooldownTime : 0.f;
	for (int32 StrikeIndex = 0; StrikeIndex < Tuning->Attacks.Num(); ++StrikeIndex)
	{
		const float TimeToHit = Timing != nullptr ? Timing->GetTimeToHit(StrikeIndex) : 0.f;
		OutCandidate.Strikes.Add(BuildStrike(Data, Tuning->Attacks[StrikeIndex], TimeToHit));
	}
	return true;
}

bool FMKHAttackCandidateBuilder::BuildStrikeCandidate(const UMKHDataSubsystem& Data, const FEquipmentAbilityDefinition& Ability,
	const float WeaponBaseDamage, const int32 StrikeIndex, FMKHAttackCandidateInput& OutCandidate)
{
	if (!BuildCandidate(Data, Ability, WeaponBaseDamage, OutCandidate) || !OutCandidate.Strikes.IsValidIndex(StrikeIndex))
	{
		return false;
	}

	FMKHAttackStrikeInput Strike = OutCandidate.Strikes[StrikeIndex];
	if (StrikeIndex > 0)
	{
		Strike.TimeToHit = FMath::Max(Strike.TimeToHit - OutCandidate.Strikes[StrikeIndex - 1].TimeToHit, 0.f);
	}
	OutCandidate.Strikes = { Strike };
	return true;
}

float FMKHAttackCandidateBuilder::GetWeaponBaseDamage(const UMKHDataSubsystem& Data, const FMKHItemEntry& Weapon)
{
	const UEquipmentDefinition* Definition = FindEquipmentDefinition(Data, Weapon);
	return IsValid(Definition) ? Definition->BaseDamage : 0.f;
}

float FMKHAttackCandidateBuilder::GetWeaponBlockStability(const UMKHDataSubsystem& Data, const FMKHItemEntry& Weapon)
{
	const UEquipmentDefinition* Definition = FindEquipmentDefinition(Data, Weapon);
	return IsValid(Definition) ? FMath::Clamp(Definition->BlockStabilityPercent, 0.f, 1.f) : 0.f;
}

FMKHDamageModifierShift FMKHAttackCandidateBuilder::ComputeModifierRoom(const UAbilitySystemComponent& BotAbilitySystem,
	const UAbilitySystemComponent& OpponentAbilitySystem)
{
	FMKHDamageModifierShift Room;
	Room.BotOutput = GetAttributeRoom(BotAbilitySystem, UMKHAttributeSet::GetEmpowerAttribute(), UMKHAttributeSet::GetMaxEmpowerAttribute());
	Room.BotIncoming = GetAttributeRoom(BotAbilitySystem, UMKHAttributeSet::GetReinforcedAttribute(), UMKHAttributeSet::GetMaxReinforcedAttribute());
	Room.OpponentOutput = GetAttributeRoom(OpponentAbilitySystem, UMKHAttributeSet::GetWeakenAttribute(), UMKHAttributeSet::GetMaxWeakenAttribute());
	Room.OpponentIncoming = GetAttributeRoom(OpponentAbilitySystem, UMKHAttributeSet::GetExposedAttribute(), UMKHAttributeSet::GetMaxExposedAttribute());
	return Room;
}

EMKHCrowdControl FMKHAttackCandidateBuilder::GetCrowdControl(const UAbilitySystemComponent& AbilitySystem)
{
	if (AbilitySystem.HasMatchingGameplayTag(MKHGameplayTags::State::Stunned))
	{
		return EMKHCrowdControl::Stun;
	}
	return AbilitySystem.HasMatchingGameplayTag(MKHGameplayTags::State::Staggered) ? EMKHCrowdControl::Stagger : EMKHCrowdControl::None;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

FMKHAttackStrikeInput FMKHAttackCandidateBuilder::BuildStrike(const UMKHDataSubsystem& Data, const FAttackData& Attack,
	const float TimeToHit)
{
	FMKHAttackStrikeInput Strike;
	Strike.DamagePercent = Attack.DamagePercent;
	Strike.StaminaDamage = Attack.StaminaDamageValue;
	Strike.TimeToHit = TimeToHit;

	for (const FGameplayTag& PerkTag : Attack.TargetStatusEffectsContainer)
	{
		if (const FPerkDefinition* Perk = Data.FindPerkDefinition(PerkTag))
		{
			AddTargetStatusEffect(PerkTag, Perk->EffectValue, Perk->EffectDuration, Strike);
		}
	}

	// Activation and on-hit self effects both count: the bot only cares about attacks that land.
	for (const FGameplayTagContainer* SelfEffects : { &Attack.SelfActivationStatusEffectsContainer, &Attack.SelfOnHitStatusEffectsContainer })
	{
		for (const FGameplayTag& PerkTag : *SelfEffects)
		{
			if (const FPerkDefinition* Perk = Data.FindPerkDefinition(PerkTag))
			{
				AddSelfStatusEffect(PerkTag, Perk->EffectValue, Strike);
			}
		}
	}
	return Strike;
}

void FMKHAttackCandidateBuilder::AddTargetStatusEffect(const FGameplayTag& PerkTag, const float Value, const float Duration,
	FMKHAttackStrikeInput& Strike)
{
	using namespace MKHGameplayTags;
	const auto SetCrowdControl = [&Strike, Duration](const EMKHCrowdControl CrowdControl)
	{
		if (CrowdControl >= Strike.CrowdControl)
		{
			Strike.CrowdControl = CrowdControl;
			Strike.CrowdControlDuration = FMath::Max(Strike.CrowdControlDuration, Duration);
		}
	};

	if (PerkTag.MatchesTag(Perk::Stun))           { SetCrowdControl(EMKHCrowdControl::Stun); }
	else if (PerkTag.MatchesTag(Perk::Stagger))   { SetCrowdControl(EMKHCrowdControl::Stagger); }
	else if (PerkTag.MatchesTag(Perk::Exposed))   { Strike.ModifierShift.OpponentIncoming += Value; }
	else if (PerkTag.MatchesTag(Perk::Reinforced)) { Strike.ModifierShift.OpponentIncoming -= Value; }
	else if (PerkTag.MatchesTag(Perk::Weaken))    { Strike.ModifierShift.OpponentOutput -= Value; }
	else if (PerkTag.MatchesTag(Perk::Empower))   { Strike.ModifierShift.OpponentOutput += Value; }
}

void FMKHAttackCandidateBuilder::AddSelfStatusEffect(const FGameplayTag& PerkTag, const float Value, FMKHAttackStrikeInput& Strike)
{
	using namespace MKHGameplayTags;
	if (PerkTag.MatchesTag(Perk::Empower))         { Strike.ModifierShift.BotOutput += Value; }
	else if (PerkTag.MatchesTag(Perk::Weaken))     { Strike.ModifierShift.BotOutput -= Value; }
	else if (PerkTag.MatchesTag(Perk::Reinforced)) { Strike.ModifierShift.BotIncoming -= Value; }
	else if (PerkTag.MatchesTag(Perk::Exposed))    { Strike.ModifierShift.BotIncoming += Value; }
}

const UEquipmentDefinition* FMKHAttackCandidateBuilder::FindEquipmentDefinition(const UMKHDataSubsystem& Data,
	const FMKHItemEntry& Weapon)
{
	const FMasterItemDefinition* ItemDefinition = Data.FindItemDefinition(Weapon.ItemTag);
	const TSubclassOf<UEquipmentDefinition> EquipmentClass = ItemDefinition != nullptr ? ItemDefinition->EquipmentItemProps.EquipmentClass : nullptr;
	return IsValid(EquipmentClass) ? GetDefault<UEquipmentDefinition>(EquipmentClass) : nullptr;
}

float FMKHAttackCandidateBuilder::GetAttributeRoom(const UAbilitySystemComponent& AbilitySystem, const FGameplayAttribute& Value,
	const FGameplayAttribute& MaxValue)
{
	return FMath::Max(AbilitySystem.GetNumericAttribute(MaxValue) - AbilitySystem.GetNumericAttribute(Value), 0.f);
}
