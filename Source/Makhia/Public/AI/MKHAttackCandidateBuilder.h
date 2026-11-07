// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/MKHAttackScoring.h"

class UAbilitySystemComponent;
class UEquipmentDefinition;
class UMKHDataSubsystem;
struct FAttackData;
struct FEquipmentAbilityDefinition;
struct FGameplayAttribute;
struct FMKHItemEntry;

/**
 * Turns the game data of an attack (tuning row, montage timing, perk rows, weapon definition) into the plain inputs of
 * FMKHAttackScoring, so the scoring stays pure. Used for the bot's candidates (Attack Options evaluator) and for the
 * opponent's abilities (danger, Perception evaluator).
 *
 * Full model: see Documentation/DuelBotAI.md (Attack choice).
 */
struct MAKHIA_API FMKHAttackCandidateBuilder
{
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Builds the scoring input of one ability: one strike per tuning row entry, with its timing and the effect of its
	 * status effects on the damage multipliers.
	 *
	 * @param Data              Data subsystem (tuning, timing and perk rows).
	 * @param Ability           Definition of the ability, from the weapon's effect package.
	 * @param WeaponBaseDamage  Base damage of the weapon granting it (GetWeaponBaseDamage).
	 * @param OutCandidate      Filled input; strike times are 0 while the ability class is not loaded.
	 * @return False when the ability has no tuning row (not a damage ability).
	 */
	static bool BuildCandidate(const UMKHDataSubsystem& Data, const FEquipmentAbilityDefinition& Ability,
		float WeaponBaseDamage, FMKHAttackCandidateInput& OutCandidate);

	/**
	 * Builds the scoring input of the single strike a press would perform now: the bot decides one strike at a time and
	 * re-evaluates in each combo window. A strike after the first is timed from the previous one.
	 *
	 * @param Data              Data subsystem (tuning, timing and perk rows).
	 * @param Ability           Definition of the ability, from the weapon's effect package.
	 * @param WeaponBaseDamage  Base damage of the weapon granting it (GetWeaponBaseDamage).
	 * @param StrikeIndex       Strike in combo order: 0 for a new attack, the next one in an open combo window.
	 * @param OutCandidate      The scoring input, with that strike only.
	 * @return False when the ability has no tuning row or no such strike.
	 */
	static bool BuildStrikeCandidate(const UMKHDataSubsystem& Data, const FEquipmentAbilityDefinition& Ability,
		float WeaponBaseDamage, int32 StrikeIndex, FMKHAttackCandidateInput& OutCandidate);

	/** @return Base damage of a weapon item (UEquipmentDefinition::BaseDamage), 0 when unknown. */
	static float GetWeaponBaseDamage(const UMKHDataSubsystem& Data, const FMKHItemEntry& Weapon);

	/** @return Block stability of a weapon item, 0-1 (UEquipmentDefinition::BlockStabilityPercent), 0 when unknown. */
	static float GetWeaponBlockStability(const UMKHDataSubsystem& Data, const FMKHItemEntry& Weapon);

	/**
	 * Room left before the caps for the shifts in the bot's favour: Empower of the bot, Reinforced of the bot, Weaken
	 * of the opponent, Exposed of the opponent.
	 *
	 * @param BotAbilitySystem       The bot's ability system.
	 * @param OpponentAbilitySystem  The opponent's ability system.
	 * @return The room, every field >= 0.
	 */
	static FMKHDamageModifierShift ComputeModifierRoom(const UAbilitySystemComponent& BotAbilitySystem,
		const UAbilitySystemComponent& OpponentAbilitySystem);

	/** @return The crowd control a character suffers now (State.Combat.CC.Stunned / .Staggered). */
	static EMKHCrowdControl GetCrowdControl(const UAbilitySystemComponent& AbilitySystem);

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/**
	 * Builds one strike from its tuning entry.
	 *
	 * @param Data       Data subsystem (perk rows).
	 * @param Attack     Tuning entry of the strike.
	 * @param TimeToHit  Seconds from the activation to the strike's hit.
	 * @return The strike input.
	 */
	static FMKHAttackStrikeInput BuildStrike(const UMKHDataSubsystem& Data, const FAttackData& Attack, float TimeToHit);

	/**
	 * Adds a status effect the strike applies to its target.
	 *
	 * @param PerkTag   Perk tag (StatEffect.Perk.*).
	 * @param Value     Perk value (e.g. 0.15 for 15%).
	 * @param Duration  Perk duration in seconds.
	 * @param Strike    Strike to update.
	 */
	static void AddTargetStatusEffect(const FGameplayTag& PerkTag, float Value, float Duration, FMKHAttackStrikeInput& Strike);

	/**
	 * Adds a status effect the strike applies to the attacker.
	 *
	 * @param PerkTag  Perk tag (StatEffect.Perk.*).
	 * @param Value    Perk value.
	 * @param Strike   Strike to update.
	 */
	static void AddSelfStatusEffect(const FGameplayTag& PerkTag, float Value, FMKHAttackStrikeInput& Strike);

	/** @return The equipment definition CDO of a weapon item, or null when the item has none. */
	static const UEquipmentDefinition* FindEquipmentDefinition(const UMKHDataSubsystem& Data, const FMKHItemEntry& Weapon);

	/** @return Max - Value of an attribute pair, never below 0. */
	static float GetAttributeRoom(const UAbilitySystemComponent& AbilitySystem, const FGameplayAttribute& Value,
		const FGameplayAttribute& MaxValue);
};
