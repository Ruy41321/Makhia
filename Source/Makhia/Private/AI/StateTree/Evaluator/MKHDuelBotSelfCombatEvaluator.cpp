// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Evaluator/MKHDuelBotSelfCombatEvaluator.h"

#include "AbilitySystemComponent.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Character/CharacterBase.h"

// ============================================================
// Public Interface
// ============================================================

void FMKHDuelBotSelfCombatEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	const UWorld* World = Context.GetWorld();
	const UAbilitySystemComponent* AbilitySystem = IsValid(Data.BotCharacter) ? Data.BotCharacter->GetAbilitySystemComponent() : nullptr;
	if (!IsValid(World) || !IsValid(AbilitySystem))
	{
		// Normal for a few ticks at round start: Verbose, not Warning.
		UE_LOG(LogMKHAI, Verbose, TEXT("MKHDuelBotSelfCombatEvaluator: no bot (%s) or ability system yet."), *GetNameSafe(Data.BotCharacter));
		Data.BotAbilitiesStatus = FMKHAbilitiesStatus();
		Data.SecondSinceLastAttack = 0.f;
		return;
	}

	UpdateAttackRecency(World->GetTimeSeconds(), *AbilitySystem, Data);

	// Both weapons come from the Context evaluator's outputs.
	const FMKHItemEntry* CurrentWeapon = Data.CurrentWeaponEntry.IsValid() ? &Data.CurrentWeaponEntry : nullptr;
	const FMKHItemEntry* SheathedWeapon = Data.SheathedWeaponEntry.IsValid() ? &Data.SheathedWeaponEntry : nullptr;
	UpdateAbilitiesStatus(*AbilitySystem, CurrentWeapon, SheathedWeapon, true, Data.BotAbilitiesStatus);
}

void FMKHDuelBotSelfCombatEvaluator::UpdateAbilitiesStatus(const UAbilitySystemComponent& AbilitySystem,
	const FMKHItemEntry* CurrentWeapon, const FMKHItemEntry* SheathedWeapon, const bool bListOtherWeaponAvailable,
	FMKHAbilitiesStatus& OutStatus)
{
	// Reset keeps the allocations: this runs every tick.
	OutStatus.AvailableAbilitiesTags.Reset();
	OutStatus.OtherWeaponAvailableAbilitiesTags.Reset();
	OutStatus.InCooldownAbilitiesTags.Reset();

	if (CurrentWeapon != nullptr)
	{
		SortWeaponAbilities(AbilitySystem, *CurrentWeapon, &OutStatus.AvailableAbilitiesTags, OutStatus.InCooldownAbilitiesTags);
	}
	if (SheathedWeapon != nullptr)
	{
		SortWeaponAbilities(AbilitySystem, *SheathedWeapon,
			bListOtherWeaponAvailable ? &OutStatus.OtherWeaponAvailableAbilitiesTags : nullptr, OutStatus.InCooldownAbilitiesTags);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void FMKHDuelBotSelfCombatEvaluator::UpdateAttackRecency(const float Now, const UAbilitySystemComponent& AbilitySystem,
	FMKHDuelBotSelfCombatEvaluatorInstanceData& Data)
{
	// Ability.Attacking is an activation owned tag of UMKHDamageAbility: present only while an attack runs.
	if (AbilitySystem.HasMatchingGameplayTag(MKHGameplayTags::Ability::Attacking))
	{
		Data.LastAttackTime = Now;
	}
	Data.SecondSinceLastAttack = Now - Data.LastAttackTime;
}

void FMKHDuelBotSelfCombatEvaluator::SortWeaponAbilities(const UAbilitySystemComponent& AbilitySystem,
	const FMKHItemEntry& Weapon, TArray<FGameplayTag>* OutAvailable, TArray<FGameplayTag>& OutInCooldown)
{
	for (const FEquipmentAbilityDefinition& Ability : Weapon.EffectPackage.Abilities)
	{
		if (!Ability.AbilityTag.IsValid())
		{
			continue;
		}

		// CooldownTag is what a skill's cooldown effect grants (UMKHAbilitySystemComponent::GrantEquipmentAbility).
		const bool bInCooldown = Ability.CooldownTag.IsValid() && AbilitySystem.HasMatchingGameplayTag(Ability.CooldownTag);
		if (bInCooldown)
		{
			OutInCooldown.AddUnique(Ability.AbilityTag);
		}
		else if (OutAvailable != nullptr)
		{
			OutAvailable->AddUnique(Ability.AbilityTag);
		}
	}
}
