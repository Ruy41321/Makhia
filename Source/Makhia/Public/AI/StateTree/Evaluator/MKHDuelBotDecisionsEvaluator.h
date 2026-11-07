// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeEvaluatorBase.h"
#include "AI/MKHAttackScoring.h"
#include "AI/MKHStateScoring.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "AI/StateTree/Evaluator/MKHDuelBotVitalsEvaluator.h"
#include "Inventory/MKHItemEntry.h"
#include "MKHDuelBotDecisionsEvaluator.generated.h"

class AAIController;
class ACharacterBase;
struct FStateTreeExecutionContext;

/**
 * Instance data of FMKHDuelBotDecisionsEvaluator: what the bot knows (bound from the other evaluators) and the
 * scores of its states, which ST_DuelBot's considerations link.
 */
USTRUCT()
struct FMKHDuelBotDecisionsEvaluatorInstanceData
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

	/** Behaviour parameters of the bot's profile; bind to Context.Tuning. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHDuelBotTuning Tuning;

	/** Character the bot controls; bind to Context.BotCharacter. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<ACharacterBase> BotCharacter = nullptr;

	/** Distance to the opponent; bind to Context.Distance. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float Distance = 0.f;

	/** The bot's weapon in hand (block stability); bind to Context.CurrentWeaponEntry. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHItemEntry CurrentWeaponEntry;

	/** The bot's attributes; bind to Vitals.BotAttributes. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHCharacterAttributesData BotAttributes;

	/** The opponent's attributes; bind to Vitals.OpponentAttributes. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHCharacterAttributesData OpponentAttributes;

	/** Damage advantage; bind to Vitals.DamageAdvantage. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float DamageAdvantage = 1.f;

	/** Best open punish window; bind to Perception.PunishWindowScore. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float PunishWindowScore = 0.f;

	/** Crowd control time left on the opponent; bind to Perception.OpponentCrowdControlTimeLeft. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float OpponentCrowdControlTimeLeft = 0.f;

	/** The opponent's recognised attack; bind to Perception.OpponentAttackAbilityTag. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FGameplayTag OpponentAttackAbilityTag;

	/** Stamina damage of the recognised attack; bind to Perception.OpponentAttackStaminaDamage. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float OpponentAttackStaminaDamage = 0.f;

	/** Danger of the opponent's ready abilities; bind to Perception.OpponentReadyDanger. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float OpponentReadyDanger = 0.f;

	/** Expected score of the attack choice; bind to Attack Options.AttackUtility. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float AttackUtility = 0.f;

	/** Scored attacks; bind to Attack Options.AttackCandidates. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<FMKHAttackCandidate> AttackCandidates;

	// ============================================================
	// Output
	// ============================================================

	/** Score of every state (Attack, Bait, PlayPassive, React children), 0-1: the considerations bind to them. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHStateScores Scores;

	/** Best counterattack in reach (React > ActivateAbility). */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHAttackCandidate CounterAttack;

	/** CounterAttack alone, probability 1, for the Commit Attack task of React > ActivateAbility (empty without one). */
	UPROPERTY(EditAnywhere, Category = "Output")
	TArray<FMKHAttackCandidate> CounterAttackCandidates;

	/** True when a counterattack is in reach. */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bHasCounterAttack = false;

	/** Seconds since the first Attack commit not followed by an attack (Bait chase). */
	UPROPERTY(EditAnywhere, Category = "Output")
	float UnansweredAttackSeconds = 0.f;

	/** Share of recent time the opponent moved away, 0-1 (Bait chase). */
	UPROPERTY(EditAnywhere, Category = "Output")
	float RetreatShare = 0.f;

	// ============================================================
	// Internal State
	// ============================================================

	/** World time the first unanswered Attack commit happened; negative when none is pending. */
	float ChaseStartTime = -1.f;

	/** World time the bot last started an attack. */
	float LastAttackStartTime = -1000.f;

	/** Distance of the previous tick (opening speed); negative before the first tick. */
	float PreviousDistance = -1.f;
};

/**
 * StateTree evaluator of the duel bot: every tick it scores the states with the pure library FMKHStateScoring
 * (Attack, Bait, PlayPassive, React > Block / Dodge / ActivateAbility) and picks the best counterattack in reach; the
 * considerations of ST_DuelBot only link these 0-1 scores (linear, sharpened by the expertise). It also tracks the
 * chase of the Bait score (attacks committed without landing while the opponent moves away). Place it last.
 * Parameters: the States field of AMKHDuelBotController::GetScoring (DT_DuelBotScoring).
 *
 * Full model: see Documentation/DuelBotAI.md (State scores).
 */
USTRUCT(meta = (DisplayName = "MKH Duel Bot Decisions", Category = "MKH"))
struct MAKHIA_API FMKHDuelBotDecisionsEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	/** Type of the per-tree data this evaluator reads and writes. */
	using FInstanceDataType = FMKHDuelBotDecisionsEvaluatorInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Updates the chase, scores the states and publishes the counterattack.
	 *
	 * @param Context    Execution context of the running StateTree, gives access to the instance data.
	 * @param DeltaTime  Seconds since the last tick.
	 */
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Formats the state scores on one line (Visual Logger, MKH.Bot.DumpAttackOptions).
	 *
	 * @param Scores  Scores to print.
	 * @return The line.
	 */
	static FString FormatScores(const FMKHStateScores& Scores);

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/**
	 * Tracks the attack commits not followed by an attack and how much of the recent time the opponent moved away.
	 *
	 * @param Now        Current world time.
	 * @param DeltaTime  Seconds since the last tick.
	 * @param Params     Bait parameters.
	 * @param Data       Instance data to update.
	 */
	static void UpdateChase(float Now, float DeltaTime, const FMKHBaitStateParams& Params, FMKHDuelBotDecisionsEvaluatorInstanceData& Data);

	/**
	 * Gathers the scoring context from the inputs.
	 *
	 * @param Data  Instance data.
	 * @return The context.
	 */
	static FMKHStateScoringContext BuildScoringContext(const FMKHDuelBotDecisionsEvaluatorInstanceData& Data);

	/**
	 * Writes the scores and the counterattack outputs.
	 *
	 * @param Scores  Computed scores.
	 * @param Data    Instance data to update.
	 */
	static void PublishScores(const FMKHStateScores& Scores, FMKHDuelBotDecisionsEvaluatorInstanceData& Data);
};
