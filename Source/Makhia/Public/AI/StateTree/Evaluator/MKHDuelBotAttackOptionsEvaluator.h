// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeEvaluatorBase.h"
#include "AI/MKHAttackScoring.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "AI/StateTree/Evaluator/MKHDuelBotVitalsEvaluator.h"
#include "Inventory/MKHItemEntry.h"
#include "MKHDuelBotAttackOptionsEvaluator.generated.h"

class AAIController;
class ACharacterBase;
class AMKHDuelBot;
class UAbilitySystemComponent;
class UMKHAbilitySystemComponent;
class UMKHDataSubsystem;
struct FEquipmentAbilityDefinition;
struct FStateTreeExecutionContext;
struct FStreamableHandle;

/** The bot's running attack while a press would chain its next combo strike. */
struct FMKHOpenComboWindow
{
	/** AbilityTag of the running attack; invalid when no combo window is open. */
	FGameplayTag AbilityTag;

	/** Strike a press in the window performs, in combo order. */
	int32 NextStrikeIndex = 0;

	/** @return True while a combo window is open. */
	bool IsOpen() const { return AbilityTag.IsValid(); }
};

/**
 * Instance data of FMKHDuelBotAttackOptionsEvaluator: what the bot knows (bound from the other evaluators) and the
 * scored attack candidates.
 */
USTRUCT()
struct FMKHDuelBotAttackOptionsEvaluatorInstanceData
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

	/** Behaviour parameters of the bot's profile (Expertise sharpens the choice); bind to Context.Tuning. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHDuelBotTuning Tuning;

	/** Character the bot controls; bind to Context.BotCharacter. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<ACharacterBase> BotCharacter = nullptr;

	/** Character the bot fights; bind to Context.Opponent. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<ACharacterBase> Opponent = nullptr;

	/** Distance to the opponent; bind to Context.Distance. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float Distance = 0.f;

	/** The bot's weapon in hand; bind to Context.CurrentWeaponEntry. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHItemEntry CurrentWeaponEntry;

	/** The bot's sheathed weapon; bind to Context.SheathedWeaponEntry. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHItemEntry SheathedWeaponEntry;

	/** The bot's attributes; bind to Vitals.BotAttributes. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHCharacterAttributesData BotAttributes;

	/** The opponent's attributes; bind to Vitals.OpponentAttributes. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHCharacterAttributesData OpponentAttributes;

	/** Whether the bot sees the opponent blocking; bind to Perception.bIsOpponentBlocking. */
	UPROPERTY(EditAnywhere, Category = "Input")
	bool bIsOpponentBlocking = false;

	/** The opponent's crowd control; bind to Perception.OpponentCrowdControl. */
	UPROPERTY(EditAnywhere, Category = "Input")
	EMKHCrowdControl OpponentCrowdControl = EMKHCrowdControl::None;

	/** Seconds of crowd control left on the opponent; bind to Perception.OpponentCrowdControlTimeLeft. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float OpponentCrowdControlTimeLeft = 0.f;

	/** How open the opponent is to a punish; bind to Perception.PunishWindowScore. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float PunishWindowScore = 0.f;

	// ============================================================
	// Output
	// ============================================================

	/** Attacks the bot can start now, scored, from both weapons. */
	UPROPERTY(EditAnywhere, Category = "Output")
	TArray<FMKHAttackCandidate> AttackCandidates;

	/** Expected score of the choice, sum(p_i x s_i): the utility of attacking at all (0 without candidates). */
	UPROPERTY(EditAnywhere, Category = "Output")
	float AttackUtility = 0.f;

	/** True when at least one attack can be started. */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bHasAttackCandidates = false;

	/** AbilityTag of the last attack the bot started (variety consideration). */
	UPROPERTY(EditAnywhere, Category = "Output")
	FGameplayTag LastUsedAbilityTag;

	// ============================================================
	// Internal State
	// ============================================================

	/** True while the bot had an attack running last tick (rising edge = a new attack started). */
	bool bWasBotAttacking = false;

	/** Async loads of the sheathed weapon's ability classes (kept so the classes stay in memory once loaded). */
	TArray<TSharedPtr<FStreamableHandle>> AbilityClassLoads;

	/** Ability classes already requested, so each is loaded once. */
	TSet<FSoftObjectPath> RequestedAbilityClasses;

	/** Best candidate and count logged last, to log only when the ranking changes. */
	FString LastLoggedRanking;
};

/**
 * StateTree evaluator of the duel bot: every tick it lists the attacks the bot can start (both weapons: the swap is
 * instant), scores them with FMKHAttackScoring and publishes the candidates, their choice probabilities
 * (p_i = s_i^k / sum(s_j^k), k from the bot's expertise) and AttackUtility, the expected score of the choice. Vetoes:
 * weapon in hand, no granted spec or CanActivateAbility false; sheathed weapon, cooldown tag on the bot or no swap.
 * Persistent: it remembers the last attack started (variety). Place it after Context, Vitals and Perception.
 * Scoring parameters: AMKHDuelBotController::GetScoring (DT_DuelBotScoring), read every tick.
 *
 * Full model: see Documentation/DuelBotAI.md (Attack choice).
 */
USTRUCT(meta = (DisplayName = "MKH Duel Bot Attack Options", Category = "MKH"))
struct MAKHIA_API FMKHDuelBotAttackOptionsEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	/** Type of the per-tree data this evaluator reads and writes. */
	using FInstanceDataType = FMKHDuelBotAttackOptionsEvaluatorInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Lists, scores and publishes the attack candidates.
	 *
	 * @param Context    Execution context of the running StateTree, gives access to the instance data.
	 * @param DeltaTime  Seconds since the last tick.
	 */
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Formats candidates as a text table (one line each: tag, considerations, score, p), for the Visual Logger and
	 * MKH.Bot.DumpAttackOptions.
	 *
	 * @param Candidates   Candidates to print.
	 * @param ChosenIndex  Candidate to mark as chosen, or INDEX_NONE.
	 * @return The table, one line per candidate.
	 */
	static FString FormatCandidateTable(TConstArrayView<FMKHAttackCandidate> Candidates, int32 ChosenIndex = INDEX_NONE);

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/** Clears every output (no bot, opponent or data). */
	static void ResetOutputs(FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data);

	/**
	 * Debug (MKH.Bot.DumpAttackOptions): prints the scoring context and the candidate table to the log, the Visual
	 * Logger and the screen, without the bot acting.
	 *
	 * @param ScoringContext  Context the candidates were scored with.
	 * @param Data            Instance data holding the candidates.
	 */
	static void DumpAttackOptions(const FMKHAttackScoringContext& ScoringContext, const FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data);

	/**
	 * Remembers the AbilityTag of the attack the bot starts (rising edge of Ability.Attacking).
	 *
	 * @param AbilitySystem  The bot's ability system.
	 * @param DataSubsystem  Data subsystem (which spec tag is an AbilityTag: it has a tuning row).
	 * @param Data           Instance data to update.
	 */
	static void UpdateLastUsedAbility(const UMKHAbilitySystemComponent& AbilitySystem, const UMKHDataSubsystem& DataSubsystem,
		FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data);

	/**
	 * Finds the bot's running attack with an open combo window.
	 *
	 * @param AbilitySystem  The bot's ability system.
	 * @param DataSubsystem  Data subsystem (which spec tag is an AbilityTag: it has a tuning row).
	 * @return The open window; not open when no attack is in one.
	 */
	static FMKHOpenComboWindow FindOpenComboWindow(const UMKHAbilitySystemComponent& AbilitySystem, const UMKHDataSubsystem& DataSubsystem);

	/**
	 * Lists the attacks of both weapons that pass the vetoes, as scoring inputs and candidates (same order). Every
	 * candidate is the single strike a press performs: the next strike for the attack in an open combo window, the
	 * first strike for the others.
	 *
	 * @param AbilitySystem   The bot's ability system.
	 * @param DataSubsystem   Data subsystem.
	 * @param EngageDistanceFactor  Share of the reach the bot closes in to (scoring row).
	 * @param Data            Instance data (weapons; receives the class loads).
	 * @param OutInputs       Scoring inputs.
	 * @param OutCandidates   Candidates, without scores.
	 */
	static void GatherCandidates(const UMKHAbilitySystemComponent& AbilitySystem, const UMKHDataSubsystem& DataSubsystem,
		float EngageDistanceFactor, FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data, TArray<FMKHAttackCandidateInput>& OutInputs,
		TArray<FMKHAttackCandidate>& OutCandidates);

	/**
	 * Input tag of an attack of the weapon in hand, when it can be used now. In an open combo window the running attack
	 * blocks the others only until the window ends (they start then, see FMKHActivateAbilityByInputTask): they are
	 * vetoed by cooldown and cost only.
	 *
	 * @param AbilitySystem  The bot's ability system.
	 * @param Ability        Definition of the attack.
	 * @param ComboWindow    The open combo window, if any.
	 * @return Its input tag, or an empty tag when vetoed (not granted, CanActivateAbility false).
	 */
	static FGameplayTag FindHeldWeaponInputTag(const UMKHAbilitySystemComponent& AbilitySystem, const FEquipmentAbilityDefinition& Ability,
		const FMKHOpenComboWindow& ComboWindow);

	/**
	 * Input tag an attack of the sheathed weapon gets once swapped in, when it is usable then.
	 *
	 * @param AbilitySystem  The bot's ability system (cooldown tags).
	 * @param Ability        Definition of the attack.
	 * @param Data           Instance data, receives an async load of the class when it is not loaded.
	 * @return Its future input tag, or an empty tag when vetoed (cooldown, class not loaded yet).
	 */
	static FGameplayTag FindSheathedWeaponInputTag(const UMKHAbilitySystemComponent& AbilitySystem, const FEquipmentAbilityDefinition& Ability,
		FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data);

	/**
	 * Gathers what the bot knows of the duel for the scoring.
	 *
	 * @param BotAbilitySystem       The bot's ability system.
	 * @param OpponentAbilitySystem  The opponent's ability system.
	 * @param DataSubsystem          Data subsystem (opponent weapon block stability).
	 * @param Data                   Instance data (inputs).
	 * @return The scoring context.
	 */
	static FMKHAttackScoringContext BuildScoringContext(const UAbilitySystemComponent& BotAbilitySystem,
		const UAbilitySystemComponent& OpponentAbilitySystem, const UMKHDataSubsystem& DataSubsystem,
		const FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data);

	/**
	 * Scores the candidates, computes the choice probabilities and the attack utility, and writes the outputs.
	 *
	 * @param Inputs      Scoring inputs.
	 * @param Context     Scoring context.
	 * @param Params      Consideration parameters (scoring row).
	 * @param Candidates  Candidates matching Inputs; receive their scores.
	 * @param Data        Instance data to update.
	 */
	static void ScoreAndPublish(TConstArrayView<FMKHAttackCandidateInput> Inputs, const FMKHAttackScoringContext& Context,
		const FMKHAttackScoringParams& Params, TArray<FMKHAttackCandidate>& Candidates, FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data);
};
