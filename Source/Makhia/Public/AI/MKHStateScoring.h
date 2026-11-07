// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "AI/MKHAttackScoring.h"
#include "MKHStateScoring.generated.h"

/** One utility consideration: an input normalized over a range, then shaped by a response curve (0-1). */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHUtilityCurve
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

	/** Linear 0-1 consideration. */
	FMKHUtilityCurve() = default;

	/**
	 * @param MinInput  Input mapped to 0.
	 * @param MaxInput  Input mapped to 1.
	 * @param Keys      Linear response curve keys (X normalized input, Y score); empty means linear.
	 */
	FMKHUtilityCurve(float MinInput, float MaxInput, std::initializer_list<FVector2f> Keys);

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * @param Input  Raw input value.
	 * @return The score, 0-1: the input normalized over InputRange (clamped), through the curve.
	 */
	float Evaluate(float Input) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Input range normalized to 0-1 before the curve: values outside are clamped to its ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Utility")
	FFloatInterval InputRange = FFloatInterval(0.f, 1.f);

	/** Response curve over the normalized input, X and Y in 0-1; empty means linear. */
	UPROPERTY(EditAnywhere, Category = "Utility")
	FRuntimeFloatCurve Curve;
};

/** Attack state score: the four considerations ST_DuelBot had on Attack, same ranges and curves. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAttackStateParams
{
	GENERATED_BODY()

	/** Expected score of the attack choice (AttackUtility). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FMKHUtilityCurve Utility = FMKHUtilityCurve(0.f, 0.3f, { { 0.f, 0.2f }, { 1.f, 1.f } });

	/** Damage advantage (Vitals.DamageAdvantage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FMKHUtilityCurve DamageAdvantage = FMKHUtilityCurve(0.5f, 2.f, { { 0.f, 0.5f }, { 0.33f, 0.9f }, { 1.f, 1.f } });

	/** Best open punish window (Perception.PunishWindowScore). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FMKHUtilityCurve PunishWindow = FMKHUtilityCurve(0.f, 1.f, { { 0.f, 0.8f }, { 1.f, 1.f } });

	/** Profile aggression (Tuning.Aggression). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FMKHUtilityCurve Aggression = FMKHUtilityCurve(0.f, 1.f, { { 0.f, 0.6f }, { 1.f, 1.f } });

	/** The bot's health ratio minus the opponent's (-1..1): a bot behind on health wants to attack less. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FMKHUtilityCurve HealthGap = FMKHUtilityCurve(-1.f, 1.f, { { 0.f, 0.5f }, { 0.5f, 1.f }, { 1.f, 1.f } });
};

/** Bait state score: BaitWilling x max(chase, danger). */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHBaitStateParams
{
	GENERATED_BODY()

	/** Seconds the bot has wanted to attack without landing a strike (Attack chosen, no attack started). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bait|Chase")
	FMKHUtilityCurve ChaseTime = FMKHUtilityCurve(0.f, 3.f, {});

	/** Opening speed from which the opponent counts as moving away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bait|Chase", meta = (Units = "cm/s", ClampMin = 0.f))
	float RetreatSpeedThreshold = 50.f;

	/** Time constant of the moving average of "the opponent moves away" (share of recent time). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bait|Chase", meta = (Units = "s", ClampMin = 0.05f))
	float RetreatAveragingTime = 1.5f;

	/** Seconds without a new Attack commit after which the chase is forgotten. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bait|Chase", meta = (Units = "s", ClampMin = 0.f))
	float ChaseMemory = 3.f;

	/** Highest danger among the opponent's ready abilities (Perception.OpponentReadyDanger). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bait|Danger")
	FMKHUtilityCurve Danger = FMKHUtilityCurve(0.5f, 1.f, { { 0.f, 0.f }, { 1.f, 0.8f } });

	/** The bot's health ratio: a bot low on health cannot afford to invite a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bait")
	FMKHUtilityCurve Health = FMKHUtilityCurve(0.f, 1.f, { { 0.f, 0.4f }, { 0.5f, 1.f }, { 1.f, 1.f } });
};

/** React children scores (Highest Utility, chosen once per opponent attack). */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHReactStateParams
{
	GENERATED_BODY()

	/** Share of the bot's stamina the incoming attack takes on a block (after the bot's block stability); 1+ = veto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Block")
	FMKHUtilityCurve BlockStaminaShare = FMKHUtilityCurve(0.f, 1.f, { { 0.f, 1.f }, { 0.6f, 0.5f }, { 1.f, 0.1f } });

	/** Stamina damage assumed for an attack the bot does not recognise (reading Low). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Block", meta = (ClampMin = 0.f))
	float AverageIncomingStaminaDamage = 25.f;

	/** Opponent health ratio minus the bot's (-1..1): a bot behind on health prefers the dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Dodge")
	FMKHUtilityCurve DodgeHealthGap = FMKHUtilityCurve(-1.f, 1.f, { { 0.f, 0.3f }, { 0.5f, 0.6f }, { 1.f, 1.f } });

	/** Counter value of an attack that stuns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Counter", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float StunCounterValue = 1.f;

	/** Counter value of an attack that staggers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Counter", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float StaggerCounterValue = 0.7f;

	/** Weight of an attack without crowd control (x its damage and status scores), when the bot can afford the trade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Counter", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float PlainCounterScale = 0.8f;

	/** Health ratio from which the bot also counters with attacks without crowd control (or when ahead on health). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Counter", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float PlainCounterHealthRatio = 0.6f;

	/** The bot's health ratio: enough health to take the incoming hit while countering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Counter")
	FMKHUtilityCurve CounterHealth = FMKHUtilityCurve(0.f, 1.f, { { 0.f, 0.2f }, { 0.5f, 0.8f }, { 1.f, 1.f } });

	/** Profile aggression. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Counter")
	FMKHUtilityCurve CounterAggression = FMKHUtilityCurve(0.f, 1.f, { { 0.f, 0.5f }, { 1.f, 1.f } });

	/** Chance, rolled once per opponent attack during Bait, that the bot answers with an attack whenever it has one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "React|Counter", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float BaitCounterChance = 0.85f;
};

/** PlayPassive state score: max(health, low stamina) x damage disadvantage x free window. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHPlayPassiveStateParams
{
	GENERATED_BODY()

	/** The bot's health ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlayPassive")
	FMKHUtilityCurve Health = FMKHUtilityCurve(0.f, 1.f, { { 0.f, 1.f }, { 0.25f, 0.95f }, { 0.5f, 0.5f }, { 1.f, 0.f } });

	/** The bot's stamina ratio: low stamina makes it step back (Recover); combined with Health by max. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlayPassive")
	FMKHUtilityCurve LowStamina = FMKHUtilityCurve(0.f, 1.f, { { 0.f, 1.f }, { 0.3f, 0.6f }, { 1.f, 0.f } });

	/** Damage advantage (inverse: a losing bot plays passive). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlayPassive")
	FMKHUtilityCurve DamageDisadvantage = FMKHUtilityCurve(0.5f, 2.f, { { 0.f, 1.f }, { 0.33f, 0.85f }, { 1.f, 0.2f } });

	/** Seconds of crowd control left on the opponent: time to heal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlayPassive")
	FMKHUtilityCurve FreeWindow = FMKHUtilityCurve(0.f, 2.f, { { 0.f, 0.7f }, { 1.f, 1.f } });
};

/** Every state score parameter, one row field of DT_DuelBotScoring. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHStateScoringParams
{
	GENERATED_BODY()

	/** Attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "States")
	FMKHAttackStateParams Attack;

	/** Bait. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "States")
	FMKHBaitStateParams Bait;

	/** React (Block, Dodge, ActivateAbility). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "States")
	FMKHReactStateParams React;

	/** PlayPassive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "States")
	FMKHPlayPassiveStateParams PlayPassive;
};

/** What the bot knows when it scores its states, gathered by the Decisions evaluator. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHStateScoringContext
{
	GENERATED_BODY()

	/** Expected score of the attack choice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float AttackUtility = 0.f;

	/** Damage advantage, 1 when even. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float DamageAdvantage = 1.f;

	/** Best open punish window, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float PunishWindowScore = 0.f;

	/** Profile aggression, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float Aggression = 0.5f;

	/** Profile will to bait, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float BaitWilling = 0.5f;

	/** Seconds since the first Attack commit not followed by an attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context", meta = (Units = "s"))
	float UnansweredAttackSeconds = 0.f;

	/** Share of recent time the opponent moved away, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float RetreatShare = 0.f;

	/** Highest danger among the opponent's ready abilities, as the bot's reading knows them, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float OpponentReadyDanger = 0.f;

	/** The bot's health ratio, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float BotHealthRatio = 1.f;

	/** The opponent's health ratio, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float OpponentHealthRatio = 1.f;

	/** The bot's stamina ratio, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float BotStaminaRatio = 1.f;

	/** The bot's current stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float BotStamina = 100.f;

	/** True when the bot can pay a dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	bool bCanAffordDodge = true;

	/** Seconds of crowd control left on the opponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context", meta = (Units = "s"))
	float OpponentCrowdControlTimeLeft = 0.f;

	/** True when the bot recognised the incoming attack (reading Medium+): IncomingStaminaDamage is its real value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	bool bKnowsIncomingAttack = false;

	/** Stamina damage of the recognised incoming attack, before the bot's block stability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float IncomingStaminaDamage = 0.f;

	/** Block stability of the bot's weapon, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float BotBlockStability = 0.f;

	/** Distance to the opponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context", meta = (Units = "cm"))
	float Distance = 0.f;

	/** True when the bait roll asks for an attack: Block and Dodge step aside whenever a counter is possible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	bool bBaitWantsCounter = false;

	/** Crowd control of the next strike of the recognised incoming attack (reading Medium+). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	EMKHCrowdControl IncomingCrowdControl = EMKHCrowdControl::None;

	/** Seconds until that strike lands; meaningful only when IncomingCrowdControl is not None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context", meta = (Units = "s"))
	float IncomingSecondsToHit = 0.f;
};

/** Next strike of the opponent's recognised attack, published by the Perception evaluator. */
struct FMKHIncomingStrike
{
	/** Crowd control of the strike; None without a recognised attack or strike left. */
	EMKHCrowdControl CrowdControl = EMKHCrowdControl::None;

	/** Seconds until the strike lands. */
	float SecondsToHit = 0.f;
};

/** Every state score and the counter chosen, written by the Decisions evaluator. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHStateScores
{
	GENERATED_BODY()

	/** Attack state score, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scores")
	float Attack = 0.f;

	/** Bait state score, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scores")
	float Bait = 0.f;

	/** Chase part of the bait score (before BaitWilling), 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scores")
	float BaitChase = 0.f;

	/** Danger part of the bait score (before BaitWilling), 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scores")
	float BaitDanger = 0.f;

	/** PlayPassive state score, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scores")
	float PlayPassive = 0.f;

	/** React > Block score, 0-1 (0 = veto: the block would break). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scores")
	float Block = 0.f;

	/** React > Dodge score, 0-1 (0 = veto: no stamina for it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scores")
	float Dodge = 0.f;

	/** React > ActivateAbility score: the best counterattack's, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scores")
	float Counter = 0.f;

	/** Index of the best counterattack in the candidates, INDEX_NONE without one in reach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scores")
	int32 CounterIndex = INDEX_NONE;
};

/**
 * Pure scoring library of the duel bot's states (Attack, Bait, PlayPassive and the React children): every function
 * reads only its arguments, so the scores are covered by the Automation Spec Makhia.AI.StateScoring. ST_DuelBot's
 * considerations only link these scores (linear, sharpened by the expertise).
 *
 * Full model: see Documentation/DuelBotAI.md (State scores).
 */
struct MAKHIA_API FMKHStateScoring
{
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Scores every state.
	 *
	 * @param Context     What the bot knows.
	 * @param Candidates  The scored attack candidates (counterattacks are picked among them).
	 * @param Params      Scoring parameters (DataTable row).
	 * @return Every score.
	 */
	static FMKHStateScores ScoreStates(const FMKHStateScoringContext& Context, TConstArrayView<FMKHAttackCandidate> Candidates,
		const FMKHStateScoringParams& Params);

	/** @return Attack: utility x damage advantage x punish window x aggression x health gap, each through its curve. */
	static float ScoreAttack(const FMKHStateScoringContext& Context, const FMKHAttackStateParams& Params);

	/**
	 * Bait = BaitWilling x health x max(chase, danger). Chase: how long the bot has wanted to attack without landing,
	 * while the opponent moves away; danger: the opponent's strongest ready ability (make it waste it).
	 *
	 * @param Context      What the bot knows.
	 * @param Params       Bait parameters.
	 * @param OutChase     Chase part, 0-1.
	 * @param OutDanger    Danger part, 0-1.
	 * @return The bait score, 0-1.
	 */
	static float ScoreBait(const FMKHStateScoringContext& Context, const FMKHBaitStateParams& Params, float& OutChase, float& OutDanger);

	/** @return PlayPassive: max(health, low stamina) x damage disadvantage x free window. */
	static float ScorePlayPassive(const FMKHStateScoringContext& Context, const FMKHPlayPassiveStateParams& Params);

	/**
	 * Stamina damage a block of the incoming attack takes, after the bot's block stability (UExecCalc_Damage rule);
	 * without a recognised attack, AverageIncomingStaminaDamage.
	 *
	 * @return The stamina damage the block takes.
	 */
	static float ComputeBlockStaminaDamage(const FMKHStateScoringContext& Context, const FMKHReactStateParams& Params);

	/** @return Block: high when the block takes little of the bot's stamina, 0 when it would break it. */
	static float ScoreBlock(const FMKHStateScoringContext& Context, const FMKHReactStateParams& Params);

	/** @return Dodge: rises as the bot falls behind on health; 0 without stamina for it. */
	static float ScoreDodge(const FMKHStateScoringContext& Context, const FMKHReactStateParams& Params);

	/**
	 * Counter value of one candidate: only attacks in reach without approaching and, against a stun or stagger strike,
	 * only those landing first; stun > stagger; attacks without crowd control only when the bot has health to spare or
	 * the bait asks for an attack (by their damage and status scores); times the health and aggression curves.
	 *
	 * @return The counter score, 0 when out of reach or too slow.
	 */
	static float ScoreCounterOption(const FMKHAttackCandidate& Candidate, const FMKHStateScoringContext& Context,
		const FMKHReactStateParams& Params);

	/**
	 * @param OutIndex  Index of the best counterattack, INDEX_NONE when none is in reach.
	 * @return The best counter score, 0 without one.
	 */
	static float ScoreCounter(const FMKHStateScoringContext& Context, TConstArrayView<FMKHAttackCandidate> Candidates,
		const FMKHReactStateParams& Params, int32& OutIndex);

	/** @return True when the bot would be interrupted before the candidate lands: its hit is not ahead of a CC strike. */
	static bool IsInterruptedBeforeHit(const FMKHAttackCandidate& Candidate, const FMKHStateScoringContext& Context);
};
