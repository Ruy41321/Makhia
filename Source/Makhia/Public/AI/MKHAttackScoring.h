// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "GameplayTagContainer.h"
#include "MKHAttackScoring.generated.h"

struct FRandomStream;

/** Crowd control a hit applies or a character suffers, from the weakest to the strongest. */
UENUM(BlueprintType)
enum class EMKHCrowdControl : uint8
{
	/** No crowd control. */
	None UMETA(DisplayName = "None"),

	/** Short interruption (State.Combat.CC.Staggered). */
	Stagger UMETA(DisplayName = "Stagger"),

	/** Long interruption (State.Combat.CC.Stunned). */
	Stun UMETA(DisplayName = "Stun")
};

/**
 * Additive change of the four damage multipliers of a duel (UExecCalc_Damage: output 1 + Empower - Weaken, incoming
 * 1 + Exposed - Reinforced) that a hit's status effects cause. Also used as the room left before each cap.
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHDamageModifierShift
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Change of the bot's output multiplier (Empower +, Weaken -). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shift")
	float BotOutput = 0.f;

	/** Change of the bot's incoming multiplier (Exposed +, Reinforced -). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shift")
	float BotIncoming = 0.f;

	/** Change of the opponent's output multiplier (Empower +, Weaken -). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shift")
	float OpponentOutput = 0.f;

	/** Change of the opponent's incoming multiplier (Exposed +, Reinforced -). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shift")
	float OpponentIncoming = 0.f;
};

/** One strike of an attack candidate, from its tuning row (FAttackData) and its montage (FMKHAbilityTiming). */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAttackStrikeInput
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Damage multiplier of the strike on the weapon's base damage (FAttackData::DamagePercent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strike")
	float DamagePercent = 1.f;

	/** Stamina damage dealt to a blocking target before its weapon's block stability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strike")
	float StaminaDamage = 0.f;

	/** Seconds from the activation to this strike's hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strike", meta = (Units = "s"))
	float TimeToHit = 0.f;

	/** Change of the damage multipliers caused by the strike's status effects (on the target and on the attacker). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strike")
	FMKHDamageModifierShift ModifierShift;

	/** Crowd control the strike applies to the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strike")
	EMKHCrowdControl CrowdControl = EMKHCrowdControl::None;

	/** Seconds the crowd control lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strike", meta = (Units = "s"))
	float CrowdControlDuration = 0.f;
};

/** One attack the bot could choose, already resolved from the data (no world access needed to score it). */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAttackCandidateInput
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** AbilityTag of the attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	FGameplayTag AbilityTag;

	/** Strikes to score: the single strike a press performs for an attack choice, every strike for an opponent's danger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	TArray<FMKHAttackStrikeInput> Strikes;

	/** Distance up to which the attack takes its target (FMKHAbilityTuningRow::GetEffectiveReach). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate", meta = (Units = "cm"))
	float Reach = 0.f;

	/** Base damage of the weapon that grants the attack (UEquipmentDefinition::BaseDamage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	float WeaponBaseDamage = 0.f;

	/** Cooldown the attack starts when used; 0 for basic attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate", meta = (Units = "s"))
	float CooldownTime = 0.f;

	/** True when this is the attack the bot used last. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	bool bIsLastUsed = false;

	/** True when the attack belongs to the sheathed weapon (an instant swap comes first). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	bool bNeedsSwap = false;

	/** True for the weapon's light attack (input Input.Ability.Attacks.Basics.Light): it stays on one weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	bool bIsLight = false;
};

/** What the bot knows of the duel when it scores its attacks, gathered by the evaluators. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAttackScoringContext
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Distance between the bot and the opponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context", meta = (Units = "cm"))
	float Distance = 0.f;

	/** Speed at which the bot closes the distance before attacking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float ApproachSpeed = 400.f;

	/** The bot's output damage multiplier (1 + Empower - Weaken). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float BotOutputMultiplier = 1.f;

	/** The bot's incoming damage multiplier (1 + Exposed - Reinforced). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float BotIncomingMultiplier = 1.f;

	/** The opponent's output damage multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float OpponentOutputMultiplier = 1.f;

	/** The opponent's incoming damage multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float OpponentIncomingMultiplier = 1.f;

	/** Largest change each multiplier can still take in the bot's favour before its attribute cap (all >= 0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	FMKHDamageModifierShift ModifierRoom;

	/** The opponent's current Health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float OpponentHealth = 100.f;

	/** Share of the damage the opponent's armor removes, 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float OpponentArmorMitigation = 0.f;

	/** The opponent's current Stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float OpponentStamina = 100.f;

	/** Block stability of the opponent's weapon, 0-1: the share of stamina damage its block absorbs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float OpponentBlockStability = 0.f;

	/** True when the bot has seen the opponent blocking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	bool bOpponentBlocking = false;

	/** Crowd control the opponent suffers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	EMKHCrowdControl OpponentCrowdControl = EMKHCrowdControl::None;

	/** Seconds of crowd control left on the opponent, exact or estimated depending on the bot's reading. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context", meta = (Units = "s"))
	float OpponentCrowdControlTimeLeft = 0.f;

	/** How open the opponent is to a punish, 0-1 (Perception evaluator). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	float PunishWindowScore = 0.f;
};

/** Tuning of the attack considerations; set in the Parameter category of the Attack Options evaluator. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAttackScoringParams
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Distance beyond the reach over which the distance score falls to its minimum. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance", meta = (Units = "cm", ClampMin = 1.f))
	float ApproachFalloffDistance = 600.f;

	/** Distance score of an attack far out of reach (approaching is still possible). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float MinDistanceScore = 0.2f;

	/** Response curve of the distance score (X: linear score 0-1); empty means linear. */
	UPROPERTY(EditAnywhere, Category = "Distance")
	FRuntimeFloatCurve DistanceCurve;

	/** Damage score of the weakest attack (the strongest gets 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float MinDamageScore = 0.3f;

	/** Share of the health damage that goes through a block that does not break it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float BlockedDamageFactor = 0.f;

	/** Response curve of the damage score (X: linear score 0-1); empty means linear. */
	UPROPERTY(EditAnywhere, Category = "Damage")
	FRuntimeFloatCurve DamageCurve;

	/** Guard score of an attack that barely dents a block (one that breaks it gets 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Break", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float MinGuardScore = 0.3f;

	/** Guard score of an attack that takes almost all the stamina without breaking the block. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Break", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float MaxGuardScoreWithoutBreak = 0.8f;

	/** Status score of an attack without status effects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float BaseStatusScore = 0.5f;

	/** Status score added per unit of relative damage advantage gained (0.1 = 10% better advantage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect", meta = (ClampMin = 0.f))
	float StatusGainScale = 2.f;

	/** Status score added by a stun. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect", meta = (ClampMin = 0.f))
	float StunValue = 0.4f;

	/** Status score added by a stagger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect", meta = (ClampMin = 0.f))
	float StaggerValue = 0.25f;

	/** Response curve of the status score (X: linear score 0-1); empty means linear. */
	UPROPERTY(EditAnywhere, Category = "Status Effect")
	FRuntimeFloatCurve StatusCurve;

	/** Crowd control score of an attack that lands after the opponent's crowd control ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd Control", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float CrowdControlMissScore = 0.1f;

	/** Crowd control score of the worst fitting attack that still lands in time (the best fitting gets 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd Control", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float MinCrowdControlFitScore = 0.4f;

	/** Response curve of the crowd control score (X: linear score 0-1); empty means linear. */
	UPROPERTY(EditAnywhere, Category = "Crowd Control")
	FRuntimeFloatCurve CrowdControlCurve;

	/** Cooldown at which spending a skill costs the most. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown", meta = (Units = "s", ClampMin = 0.1f))
	float CooldownReferenceTime = 20.f;

	/** Largest score cut for spending a skill outside any punish window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float MaxCooldownCost = 0.6f;

	/** Variety score of the attack used last (the others get 1); light attacks are never penalised for repeating. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variety", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float RepeatScore = 0.7f;

	/** Variety score of the sheathed weapon's light attack: it wins only when clearly better than the one in hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variety", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float LightSwapScore = 0.8f;

	/** Seconds to the hit up to which an attack gets the full speed score. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (Units = "s", ClampMin = 0.f))
	float FastHitTime = 0.3f;

	/** Seconds to the hit from which an attack gets MinSpeedScore. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (Units = "s", ClampMin = 0.f))
	float SlowHitTime = 1.f;

	/** Speed score of the slowest attacks (the fastest get 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float MinSpeedScore = 0.6f;

	/** Response curve of the speed score (X: linear score 0-1); empty means linear. */
	UPROPERTY(EditAnywhere, Category = "Speed")
	FRuntimeFloatCurve SpeedCurve;
};

/** How dangerous the opponent's abilities are for the bot; set in the Parameter category of the Perception evaluator. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHDangerParams
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Danger of an ability that stuns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float StunDanger = 1.f;

	/** Danger of an ability that staggers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float StaggerDanger = 0.7f;

	/** Effective damage multiplier (DamagePercent x opponent output x bot incoming) from which a strike is dangerous. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger", meta = (ClampMin = 0.f))
	float HighDamageThreshold = 1.8f;

	/** Danger of a strike right at HighDamageThreshold; rises to 1 as it takes the bot's whole remaining health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float HighDamageDanger = 0.6f;
};

/** Per consideration scores of one candidate and its final score, for the decision log and the debug table. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAttackScoreBreakdown
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Distance consideration, 0-1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float Distance = 1.f;

	/** Expected damage consideration, 0-1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float Damage = 1.f;

	/** Guard break consideration, 0-1 (1 when the opponent does not block). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float GuardBreak = 1.f;

	/** Status effect consideration, 0-1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float StatusEffect = 1.f;

	/** Crowd control fit consideration, 0-1 (1 when the opponent is not crowd controlled). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float CrowdControlFit = 1.f;

	/** Cooldown economy consideration, 0-1 (1 for basic attacks). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float CooldownEconomy = 1.f;

	/** Variety consideration, 0-1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float Variety = 1.f;

	/** Speed consideration, 0-1 (1 when the opponent is crowd controlled: Crowd Control Fit decides there). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float Speed = 1.f;

	/** Health damage the scored strike is expected to deal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float ExpectedDamage = 0.f;

	/** True when the expected damage kills the opponent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	bool bLethal = false;

	/** True when the attack breaks the opponent's block. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	bool bBreaksGuard = false;

	/** Product of every consideration, 0-1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	float Total = 0.f;
};

/** One attack the bot can start now, scored: what the Commit Attack task picks from and the tree then executes. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAttackCandidate
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** AbilityTag of the attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	FGameplayTag AbilityTag;

	/** Input to press to start it (after the swap for the sheathed weapon). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	FGameplayTag InputTag;

	/** Distance the bot must be within before pressing InputTag: the attack's reach times the engage factor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate", meta = (Units = "cm"))
	float EngageDistance = 0.f;

	/** Distance up to which the attack takes its target (FMKHAbilityTuningRow::GetEffectiveReach). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate", meta = (Units = "cm"))
	float Reach = 0.f;

	/** True when the attack belongs to the sheathed weapon: press SwapInputTag first (instant swap). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	bool bNeedsSwap = false;

	/** Weapon quick slot input that swaps to the sheathed weapon; empty when no swap is needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	FGameplayTag SwapInputTag;

	/** Strongest crowd control the attack applies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	EMKHCrowdControl CrowdControl = EMKHCrowdControl::None;

	/** Seconds from the press to the first strike's hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate", meta = (Units = "s"))
	float TimeToHit = 0.f;

	/** Final score, 0-1 (product of the considerations). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	float Score = 0.f;

	/** Chance of being picked, s^k / sum(s^k) with the bot's expertise sharpness. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	float Probability = 0.f;

	/** Per consideration scores. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidate")
	FMKHAttackScoreBreakdown Breakdown;
};

/**
 * Pure scoring library of the duel bot's attack choice: every function reads only its arguments (no world, no
 * actors), so the whole choice is covered by the Automation Spec Makhia.AI.AttackScoring. The Attack Options
 * evaluator gathers the inputs, this library turns them into scores and choice probabilities.
 *
 * Full model: see Documentation/DuelBotAI.md (Attack choice).
 */
struct MAKHIA_API FMKHAttackScoring
{
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * How much more the bot's hits hurt than the opponent's: (BotOut x OppIn) / (OppOut x BotIn).
	 *
	 * @return 1 when even, above 1 when the bot has the advantage.
	 */
	static float ComputeDamageAdvantage(float BotOutput, float BotIncoming, float OpponentOutput, float OpponentIncoming);

	/**
	 * Scores every candidate (product of the considerations); the damage score is relative to the strongest one.
	 *
	 * @return One breakdown per candidate, in the same order.
	 */
	static TArray<FMKHAttackScoreBreakdown> ScoreCandidates(TConstArrayView<FMKHAttackCandidateInput> Candidates,
		const FMKHAttackScoringContext& Context, const FMKHAttackScoringParams& Params);

	/**
	 * Choice probabilities p_i = s_i^k / sum(s_j^k): k 0 is a uniform choice, a large k nears the best score.
	 *
	 * @param Scores        Final score of each candidate.
	 * @param Sharpness     Exponent k (FMKHDuelBotTuning::GetDecisionSharpness).
	 * @param bExactArgmax  True to give the best score probability 1 (Expertise 1).
	 * @return One probability per score, summing to 1 (empty when Scores is empty).
	 */
	static TArray<float> ComputeChoiceProbabilities(TConstArrayView<float> Scores, float Sharpness, bool bExactArgmax);

	/** @return Expected score of the choice, sum(p_i x s_i): the utility of attacking at all. */
	static float ComputeExpectedUtility(TConstArrayView<float> Scores, TConstArrayView<float> Probabilities);

	/**
	 * Draws a candidate with the given probabilities.
	 *
	 * @param Probabilities  Choice probabilities (ComputeChoiceProbabilities).
	 * @param RandomStream   Random source; a fixed seed gives a repeatable choice (tests).
	 * @return The chosen index, or INDEX_NONE when Probabilities is empty.
	 */
	static int32 PickIndex(TConstArrayView<float> Probabilities, FRandomStream& RandomStream);

	/** @return Score^Sharpness: the expertise sharpness applied to one utility score (0^0 counts as 1). */
	static float ApplySharpness(float Score, float Sharpness);

	/**
	 * How dangerous one of the opponent's abilities is for the bot, 0-1: a stun or a stagger, or a strike whose
	 * effective multiplier reaches HighDamageThreshold, more when it takes most of the bot's remaining health.
	 *
	 * @param Ability          The opponent's ability, resolved like a bot candidate (shifts and reach unused).
	 * @param OpponentOutput   The opponent's output multiplier.
	 * @param BotIncoming      The bot's incoming multiplier.
	 * @param BotArmor         Share of the damage the bot's armor removes, 0-1.
	 * @param BotHealth        The bot's current Health.
	 * @param Params           Danger levels.
	 * @return The danger, 0-1.
	 */
	static float ComputeAbilityDanger(const FMKHAttackCandidateInput& Ability, float OpponentOutput, float BotIncoming,
		float BotArmor, float BotHealth, const FMKHDangerParams& Params);

	// --- Considerations, one per function (public for the tests) ---

	/** @return Health damage of the whole attack, through armor and, unless it breaks it, the block. */
	static float ComputeExpectedDamage(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
		const FMKHAttackScoringParams& Params);

	/** @return True when the attack's stamina damage, after block stability, empties the opponent's stamina. */
	static bool BreaksGuard(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context);

	/** @return 1 within reach, falling to MinDistanceScore at ApproachFalloffDistance beyond it. */
	static float ScoreDistance(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
		const FMKHAttackScoringParams& Params);

	/** @return 1 for a lethal attack, else from MinDamageScore to 1 relative to the strongest candidate. */
	static float ScoreDamage(float ExpectedDamage, float MaxExpectedDamage, bool bLethal, const FMKHAttackScoringParams& Params);

	/** @return 1 without a block or when it breaks it, else from MinGuardScore by the share of stamina taken. */
	static float ScoreGuardBreak(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
		const FMKHAttackScoringParams& Params);

	/** @return BaseStatusScore plus the damage advantage the status effects gain (within the caps) and crowd control. */
	static float ScoreStatusEffects(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
		const FMKHAttackScoringParams& Params);

	/** @return 1 without crowd control; on a stun the attack that best fills the time left, on a stagger the fastest. */
	static float ScoreCrowdControlFit(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
		const FMKHAttackScoringParams& Params);

	/** @return 1 for basic attacks; a skill costs by its cooldown, discounted by the punish window and a lethal hit. */
	static float ScoreCooldownEconomy(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
		bool bLethal, const FMKHAttackScoringParams& Params);

	/**
	 * Variety and loyalty: a light attack is never penalised for repeating (it stays on the weapon in hand) and the
	 * sheathed weapon's light gets LightSwapScore; any other attack used last gets RepeatScore.
	 *
	 * @return The variety score, 0-1.
	 */
	static float ScoreVariety(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringParams& Params);

	/**
	 * In a neutral situation a faster attack is better: it is harder to answer. Reads only the strike's own time to hit
	 * (the Distance consideration prices the approach).
	 *
	 * @return From 1 at FastHitTime to MinSpeedScore at SlowHitTime; 1 when the opponent is crowd controlled.
	 */
	static float ScoreSpeed(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
		const FMKHAttackScoringParams& Params);

	/** @return The strongest crowd control of the candidate's strikes. */
	static EMKHCrowdControl GetStrongestCrowdControl(const FMKHAttackCandidateInput& Candidate);

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/** @return The curve's value at Score, or Score itself when the curve is empty. */
	static float ApplyCurve(const FRuntimeFloatCurve& Curve, float Score);

	/** @return The sum of the strikes' modifier shifts, each clamped to the room left before its cap. */
	static FMKHDamageModifierShift SumClampedShift(const FMKHAttackCandidateInput& Candidate, const FMKHDamageModifierShift& Room);

	/** @return Seconds until the first strike lands: approach beyond the reach, then its TimeToHit. */
	static float ComputeTimeToFirstHit(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context);
};
