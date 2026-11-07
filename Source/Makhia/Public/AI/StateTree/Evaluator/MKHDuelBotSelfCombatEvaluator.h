// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "Inventory/MKHItemEntry.h"
#include "MKHDuelBotSelfCombatEvaluator.generated.h"

class ACharacterBase;
class AAIController;
class UAbilitySystemComponent;
struct FStateTreeExecutionContext;

/**
 * A duel character's weapon abilities sorted by cooldown, as AbilityTags of their item entries' effect packages;
 * filled by FMKHDuelBotSelfCombatEvaluator::UpdateAbilitiesStatus.
 */
USTRUCT()
struct FMKHAbilitiesStatus
{
	GENERATED_BODY()

	/** Abilities that are available to use, so that can be used. 
	 * 
	 * Retrieved analyzing all the abilities the current weapon gives to the Character and checking if they are in cooldown or not.
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	TArray<FGameplayTag> AvailableAbilitiesTags;
	
	/** Abilities that are available to use with the other weapon, so that can be used after switching.
	 * Unobtained for the opponent
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	TArray<FGameplayTag> OtherWeaponAvailableAbilitiesTags;
	
	/** Abilities that are in cooldown, so that cannot be used. 
	 * 
	 * The ability in cooldown are retrieved analyzing the cooldown tags applied to the Character by the ability system,
	 * for both the bot and the opponent also of the sheathed weapon.
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	TArray<FGameplayTag> InCooldownAbilitiesTags;
	
};

/**
 * Instance data of FMKHDuelBotSelfCombatEvaluator: the bot's controller (bound from the StateTree context) and the
 * self combat (the bot's own combat state) outputs read by conditions, considerations and tasks.
 */
USTRUCT()
struct FMKHDuelBotSelfCombatEvaluatorInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Context
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
		
	// ============================================================
	// Input
	// ============================================================
	
	/** Character the bot controls, null until the controller has one. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<ACharacterBase> BotCharacter = nullptr;
	
	/** The bot's current weapon MKHItemEntry (contains the effect package); bind to the Context evaluator's output. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHItemEntry CurrentWeaponEntry = FMKHItemEntry();

	/** The bot's sheathed weapon MKHItemEntry (contains the effect package). */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHItemEntry SheathedWeaponEntry = FMKHItemEntry();
		
	// ============================================================
	// Output
	// ============================================================
	
	/** Bot's Available and Unavailable abilities */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHAbilitiesStatus BotAbilitiesStatus;
		
	/** Seconds since the bot last had an attack running (Ability.Attacking); 0 while attacking. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float SecondSinceLastAttack = 0.f;
	
	// ============================================================
	// Internal State
	// ============================================================
	
	/** World time of the last tick with an attack running; far in the past so a round starts "rested". */
	float LastAttackTime = -1000.f;
};

/**
 * StateTree evaluator of the duel bot: every tick it publishes the self combat (the bot's own combat state) outputs, so every state reads them
 * through bindings.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Duel Bot Self Combat", Category = "MKH"))
struct MAKHIA_API FMKHDuelBotSelfCombatEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	/** Type of the per-tree data this evaluator reads and writes. */
	using FInstanceDataType = FMKHDuelBotSelfCombatEvaluatorInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Refreshes the bot's own combat state: its abilities sorted by cooldown and the seconds since its last attack.
	 *
	 * @param Context    Execution context of the running StateTree, gives access to the instance data.
	 * @param DeltaTime  Seconds since the last tick.
	 */
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Sorts the abilities of a character's two weapons (their item entries' effect packages) by cooldown: an ability
	 * is in cooldown while its CooldownTag is on the character's ability system (the tag outlives a weapon swap).
	 * Abilities are identified by their AbilityTag. Shared by the bot (SelfCombat) and its opponent (Perception).
	 *
	 * @param AbilitySystem              Ability system of the character (on the PlayerState for a player).
	 * @param CurrentWeapon              Weapon in hand, null when unarmed.
	 * @param SheathedWeapon             Other weapon, null when there is none.
	 * @param bListOtherWeaponAvailable  Whether to fill OtherWeaponAvailableAbilitiesTags (false for the opponent).
	 * @param OutStatus                  Lists to fill; emptied first.
	 */
	static void UpdateAbilitiesStatus(const UAbilitySystemComponent& AbilitySystem, const FMKHItemEntry* CurrentWeapon,
		const FMKHItemEntry* SheathedWeapon, bool bListOtherWeaponAvailable, FMKHAbilitiesStatus& OutStatus);

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/**
	 * Updates LastAttackTime while the bot has an attack running (Ability.Attacking) and outputs the seconds since then.
	 *
	 * @param Now            Current world time.
	 * @param AbilitySystem  The bot's ability system.
	 * @param Data           Instance data to update.
	 */
	static void UpdateAttackRecency(float Now, const UAbilitySystemComponent& AbilitySystem,
		FMKHDuelBotSelfCombatEvaluatorInstanceData& Data);

	/**
	 * Adds a weapon's abilities to the available or the in cooldown list.
	 *
	 * @param AbilitySystem   Ability system carrying the cooldown tags.
	 * @param Weapon          Weapon whose effect package to read.
	 * @param OutAvailable    Receives the abilities off cooldown; null to skip them.
	 * @param OutInCooldown   Receives the abilities in cooldown.
	 */
	static void SortWeaponAbilities(const UAbilitySystemComponent& AbilitySystem, const FMKHItemEntry& Weapon,
		TArray<FGameplayTag>* OutAvailable, TArray<FGameplayTag>& OutInCooldown);
};
