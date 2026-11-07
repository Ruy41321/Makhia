// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "MKHDuelBotVitalsEvaluator.generated.h"

class AAIController;
class ACharacterBase;
class UAbilitySystemComponent;
struct FGameplayAttribute;
struct FStateTreeExecutionContext;

/**
 * Snapshot of one duel character's attributes (UMKHAttributeSet), read every tick by FMKHDuelBotVitalsEvaluator.
 * Ratios are value / cap in 0-1; every field is 0 while the character or its ability system is missing.
 */
USTRUCT()
struct FMKHCharacterAttributesData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Health / MaxHealth in 0-1. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float HealthRatio = 0.f;

	/** Stamina / MaxStamina in 0-1. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float StaminaRatio = 0.f;

	/** Current Health, in health points. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float CurrentHealth = 0.f;

	/** Fraction of incoming damage the current Armor removes, in 0-1 (UMKHAttributeSet::CalculateArmorDamageReduction). */
	UPROPERTY(EditAnywhere, Category = "Output")
	float CurrentArmorMitigation = 0.f;

	/** Current Stamina, in stamina points. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float CurrentStamina = 0.f;

	/** DodgeStaminaCost / MaxStamina in 0-1: the share of the stamina bar one dodge costs. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float DodgeStaminaCostRatio = 0.f;

	/** Probability of a critical hit in 0-1, as the damage execution rolls it. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float CritChance = 0.f;

	/** Damage bonus added to the multiplier on a critical hit. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float CritDamageMod = 0.f;

	/** AdditiveBaseDamage / MaxAdditiveBaseDamage in 0-1. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float AdditiveBaseDamageRatio = 0.f;

	/** Empower / MaxEmpower in 0-1. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float EmpowerRatio = 0.f;

	/** Weaken / MaxWeaken in 0-1. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float WeakenRatio = 0.f;

	/** Reinforced / MaxReinforced in 0-1. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float ReinforcedRatio = 0.f;

	/** Exposed / MaxExposed in 0-1. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float ExposedRatio = 0.f;

	/** Multiplier on the damage this character deals, 1 + (Empower - Weaken), before critical hits (ExecCalc_Damage). */
	UPROPERTY(EditAnywhere, Category = "Output")
	float EffectiveOutputDamageMultiplier = 0.f;

	/** Multiplier on the damage this character takes, 1 + (Exposed - Reinforced), before armor (ExecCalc_Damage). */
	UPROPERTY(EditAnywhere, Category = "Output")
	float EffectiveIncomingDamageMultiplier = 0.f;
};

/**
 * Instance data of FMKHDuelBotVitalsEvaluator: the bot's controller (bound from the StateTree context) and the
 * attributes of both sides, read by conditions, considerations and tasks.
 */
USTRUCT()
struct FMKHDuelBotVitalsEvaluatorInstanceData
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

	/** Character the bot fights, null until the controller has found one. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<ACharacterBase> Opponent = nullptr;

	// ============================================================
	// Output
	// ============================================================
	
	/** Attributes of the bot. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHCharacterAttributesData BotAttributes;

	/** Attributes of the bot's opponent. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHCharacterAttributesData OpponentAttributes;

	/**
	 * How much more the bot's hits hurt than the opponent's, from the damage multipliers of both sides:
	 * (BotOut x OppIn) / (OppOut x BotIn). 1 when even, above 1 when the bot has the advantage.
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	float DamageAdvantage = 1.f;
};

/**
 * StateTree evaluator of the duel bot: every tick it reads the attributes of the bot and of its opponent and
 * publishes them with the damage advantage between the two, so every state reads the same vitals through bindings.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Duel Bot Vitals", Category = "MKH"))
struct MAKHIA_API FMKHDuelBotVitalsEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	/** Type of the per-tree data this evaluator reads and writes. */
	using FInstanceDataType = FMKHDuelBotVitalsEvaluatorInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Refreshes the attributes of the bot and of its opponent and the damage advantage.
	 *
	 * @param Context    Execution context of the running StateTree, gives access to the instance data.
	 * @param DeltaTime  Seconds since the last tick.
	 */
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/**
	 * Reads a character's attributes from its ability system (on the PlayerState for players).
	 *
	 * @param Character      Character to read.
	 * @param OutAttributes  Snapshot to fill; reset to its defaults when the character has no ability system.
	 */
	static void UpdateCharacterAttributes(const ACharacterBase& Character, FMKHCharacterAttributesData& OutAttributes);

	/**
	 * Reads an attribute as a fraction of its cap.
	 *
	 * @param AbilitySystem  Ability system holding both attributes.
	 * @param Value          Current value attribute (e.g. Health).
	 * @param MaxValue       Cap attribute (e.g. MaxHealth).
	 * @return Value / MaxValue, 0 with a zero cap.
	 */
	static float GetAttributeRatio(const UAbilitySystemComponent& AbilitySystem, const FGameplayAttribute& Value,
		const FGameplayAttribute& MaxValue);
};
