// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHDuelBotContextEvaluator.h"
#include "MKHDuelBotSelfCombatEvaluator.h"
#include "MKHDuelBotVitalsEvaluator.h"
#include "StateTreeEvaluatorBase.h"
#include "AI/MKHAttackScoring.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "AI/Data/MKHDuelBotScoring.h"
#include "MKHDuelBotPerceptionEvaluator.generated.h"

class ACharacterBase;
class AAIController;
class UAbilitySystemComponent;
class UMKHDataSubsystem;
struct FEquipmentAbilityDefinition;
struct FStateTreeExecutionContext;

/** Why the opponent is open to a punish right now; several can be true at once (PunishWindowScore is the best). */
USTRUCT()
struct FMKHPunishWindows
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return True when any window differs from Other's. */
	bool operator!=(const FMKHPunishWindows& Other) const
	{
		return bRecovery != Other.bRecovery || bOutOfStamina != Other.bOutOfStamina || bLastDodge != Other.bLastDodge
			|| bDangerousInCooldown != Other.bDangerousInCooldown;
	}

	// ============================================================
	// Properties
	// ============================================================

	/** The opponent's attack has just ended and it is still recovering (any reading level). */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bRecovery = false;

	/** The opponent is out of stamina, State.General.Stamina.Out (any reading level). */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bOutOfStamina = false;

	/** The opponent has not enough stamina left for a dodge (reading Medium and High). */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bLastDodge = false;

	/** The opponent's dangerous abilities are in cooldown (reading High). */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bDangerousInCooldown = false;
};

/**
 * Instance data of FMKHDuelBotPerceptionEvaluator: the bot's controller (bound from the StateTree context) and the
 * perception (what the bot sees the opponent do) outputs read by conditions, considerations and tasks.
 */
USTRUCT()
struct FMKHDuelBotPerceptionEvaluatorInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Context
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
	
	// ============================================================
	// Parameter
	// ============================================================
	
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (Units = "s", ClampMin = 0.f))
	float MaxBaitTime = 1.0f;

	// ===========================================================
	// Input
	// ===========================================================
	
	/** Behavior parameters used to make choices based on the bot's difficulty level. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHDuelBotTuning Tuning;
	
	/** Character the bot controls, null until the controller has one. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<ACharacterBase> BotCharacter = nullptr;

	/** Character the bot fights, null until the controller has found one. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<ACharacterBase> Opponent = nullptr;
	
	/** Distance band between the bot and the opponent; the bot reacts only to attacks in the Combat band. */
	UPROPERTY(EditAnywhere, Category = "Input")
	EMKHDuelDistanceBand DistanceBand = EMKHDuelDistanceBand::Far;

	/** The bot's attributes; bind to the Vitals evaluator's BotAttributes. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHCharacterAttributesData BotAttributes;

	/** The opponent's attributes; bind to the Vitals evaluator's OpponentAttributes. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMKHCharacterAttributesData OpponentAttributes;

	// ============================================================
	// Output
	// ============================================================
	
	/** True once the reaction delay has passed on an opponent attack in range that the bot decided to react to. */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bShouldReact = false;
		
	/** True while the opponent has an attack running (Ability.Attacking on its ability system). */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bIsOpponentAttacking = false;
	
	/** Tag of the opponent's attack ability that is running, or empty if none. Retrieved only if Tuning.OpponentReadingFactor == Medium */
	UPROPERTY(EditAnywhere, Category = "Output")
	FGameplayTag OpponentAttackAbilityTag = FGameplayTag();
	
	/** Seconds since the opponent's running attack started; 0 while it is not attacking. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float SecondsSinceOpponentAttackStarted = 0.f;
	
	/** Seconds since the opponent's last attack ended (punish window); very large while it attacks or before any attack. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float SecondsSinceOpponentAttackEnded = 0.f;
	
	/** Opponent's Available and Unavailable abilities */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHAbilitiesStatus OpponentAbilitiesStatus;

	/** True once the opponent has been blocking (State.Movement.Blocking) for the bot's reaction delay. */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bIsOpponentBlocking = false;

	/** Crowd control the opponent suffers (seen at any reading level). */
	UPROPERTY(EditAnywhere, Category = "Output")
	EMKHCrowdControl OpponentCrowdControl = EMKHCrowdControl::None;

	/** Seconds of crowd control left on the opponent: exact with reading High, else estimated from the typical length. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float OpponentCrowdControlTimeLeft = 0.f;

	/** True when OpponentCrowdControlTimeLeft is exact (reading High). */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bKnowsCrowdControlTimeLeft = false;

	/** Danger of the opponent's recognised attack, 0-1 (reading Medium and High, after the reaction delay). */
	UPROPERTY(EditAnywhere, Category = "Output")
	float OpponentAttackDanger = 0.f;

	/** Stamina damage of the opponent's recognised attack before block stability (0 while none is recognised). */
	UPROPERTY(EditAnywhere, Category = "Output")
	float OpponentAttackStaminaDamage = 0.f;

	/**
	 * Highest danger of the opponent's abilities ready to use, as the bot's reading knows them: High from the
	 * cooldowns of the weapon in hand, Medium the danger of the last attack it recognised, Low 0 (Bait score).
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	float OpponentReadyDanger = 0.f;

	/** How open the opponent is to a punish, 0-1: the best of the open windows. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float PunishWindowScore = 0.f;

	/** Which punish windows are open. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHPunishWindows PunishWindows;
			
	// ============================================================
	// Internal State
	// ============================================================
	
	/** World time the opponent's current (or last) attack started. */
	float OpponentAttackStartTime = -1000.f;

	/** World time the opponent's last attack ended; far in the past so a round does not start in a punish window. */
	float OpponentAttackEndTime = -1000.f;
	
	/** Outcome of the ReactionChance roll made when the opponent's current attack started. */
	bool bReactionRollPassed = false;

	/** True once Event.AI.ReactToAttack was sent for the opponent's current attack; reset when an attack starts. */
	bool bReactionEventSent = false;

	/** Reaction delay of the opponent's current attack, rolled in Tuning.ReactionTimeRange when the attack starts. */
	float CurrentReactionTime = 0.f;

	/** Opponent's sheathed weapon, kept between ticks so it is copied from the inventory only when it changes. */
	FMKHItemEntry OpponentSheathedWeapon;

	/** True while the opponent blocks, before the reaction delay is applied. */
	bool bOpponentBlockSeen = false;

	/** World time the opponent's current block started. */
	float OpponentBlockStartTime = 0.f;

	/** Reaction delay of the opponent's current block, rolled in Tuning.ReactionTimeRange when it starts. */
	float CurrentBlockReactionTime = 0.f;

	/** World time the opponent's current crowd control started. */
	float OpponentCrowdControlStartTime = 0.f;

	/** Danger of the last opponent attack the bot recognised (reading Medium's memory of the ready danger). */
	float LastSeenAttackDanger = 0.f;

	/** Scoring parameters of this tick (AMKHDuelBotController::GetScoring), refreshed at the start of Tick. */
	FMKHPerceptionParams Params;
			
};

/**
 * StateTree evaluator of the duel bot: every tick it publishes the perception (what the bot sees the opponent do) outputs, so every state reads them
 * through bindings. Parameters: the Perception field of AMKHDuelBotController::GetScoring (DT_DuelBotScoring).
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Duel Bot Perception", Category = "MKH"))
struct MAKHIA_API FMKHDuelBotPerceptionEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	/** Type of the per-tree data this evaluator reads and writes. */
	using FInstanceDataType = FMKHDuelBotPerceptionEvaluatorInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Refreshes what the bot perceives of the opponent's actions.
	 *
	 * @param Context    Execution context of the running StateTree, gives access to the instance data.
	 * @param DeltaTime  Seconds since the last tick.
	 */
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/** Resets every output to its "no opponent" value. */
	static void ResetOutputs(FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Tracks the start and end of the opponent's attacks, rolls the reaction chance and delay once per attack and outputs
	 * the timings.
	 *
	 * @param OpponentAbilitySystem  Opponent's ability system (on its PlayerState for a player).
	 * @param Now                    Current world time.
	 * @param Data                   Instance data to update.
	 */
	static void UpdateAttackState(const UAbilitySystemComponent& OpponentAbilitySystem, float Now,
		FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Outputs the running attack's AbilityTag once the reaction delay has passed (a human recognises an attack by its
	 * animation, not at its first frame), when Tuning.OpponentReadingFactor is Medium or High.
	 *
	 * @param OpponentAbilitySystem  Opponent's ability system.
	 * @param Data                   Instance data to update.
	 */
	static void UpdateAttackAbilityTag(const UAbilitySystemComponent& OpponentAbilitySystem,
		FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Finds the AbilityTag of the opponent's running attack: the active UMKHDamageAbility spec granted by its weapon,
	 * matched to the weapon's ability definition by class and input tag (as UMKHAbilitySystemComponent grants it).
	 *
	 * @param OpponentAbilitySystem  Opponent's ability system.
	 * @param Weapon                 Opponent's weapon in hand.
	 * @return The running attack's AbilityTag, or an empty tag when none is found.
	 */
	static FGameplayTag FindRunningAttackTag(const UAbilitySystemComponent& OpponentAbilitySystem, const FMKHItemEntry& Weapon);

	/**
	 * Outputs bShouldReact: the duel runs, both sides are alive, the opponent attacks in the Combat distance band,
	 * the reaction roll passed and the reaction delay has passed.
	 *
	 * @param Context  Execution context of the running StateTree, gives the world and its game state.
	 * @param Data     Instance data to update; BotCharacter and Opponent must be valid.
	 */
	static void UpdateShouldReact(const FStateTreeExecutionContext& Context, FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Sends Event.AI.ReactToAttack the first tick bShouldReact is true, so the bot reacts at most once per attack.
	 *
	 * @param Context  Execution context of the running StateTree, receives the event.
	 * @param Data     Instance data to update.
	 */
	static void SendReactionEventOnce(FStateTreeExecutionContext& Context, FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Outputs the opponent's weapon abilities sorted by cooldown when Tuning.OpponentReadingFactor is High, else empty
	 * lists. OtherWeaponAvailableAbilitiesTags stays empty for the opponent.
	 *
	 * @param OpponentAbilitySystem  Opponent's ability system.
	 * @param Data                   Instance data to update.
	 */
	static void UpdateOpponentAbilitiesStatus(const UAbilitySystemComponent& OpponentAbilitySystem,
		FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Outputs bIsOpponentBlocking once the opponent has held its block for a reaction delay (rolled once per block).
	 *
	 * @param OpponentAbilitySystem  Opponent's ability system.
	 * @param Now                    Current world time.
	 * @param Data                   Instance data to update.
	 */
	static void UpdateOpponentBlocking(const UAbilitySystemComponent& OpponentAbilitySystem, float Now,
		FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Outputs the opponent's crowd control and its time left: the exact effect time with reading High, else the
	 * typical length minus the time seen.
	 *
	 * @param OpponentAbilitySystem  Opponent's ability system.
	 * @param Now                    Current world time.
	 * @param Data                   Instance data to update.
	 */
	static void UpdateOpponentCrowdControl(const UAbilitySystemComponent& OpponentAbilitySystem, float Now,
		FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Outputs the highest danger of the opponent's ready abilities, by reading level (see OpponentReadyDanger).
	 *
	 * @param OpponentAbilitySystem  Opponent's ability system (cooldown tags).
	 * @param DataSubsystem          Data subsystem.
	 * @param Data                   Instance data to update.
	 */
	static void UpdateOpponentReadyDanger(const UAbilitySystemComponent& OpponentAbilitySystem, const UMKHDataSubsystem& DataSubsystem,
		FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Outputs the danger and the stamina damage of the opponent's recognised attack (0 while none is recognised).
	 *
	 * @param DataSubsystem  Data subsystem (tuning, perk and weapon data).
	 * @param Data           Instance data to update.
	 */
	static void UpdateOpponentAttackDanger(const UMKHDataSubsystem& DataSubsystem, FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Hands the controller the next strike of the recognised attack, for the counter choice of this tick.
	 *
	 * @param OpponentAttack  The recognised attack with all its strikes, null when none is recognised.
	 * @param Data            Instance data (controller, time since the attack started).
	 */
	static void PublishIncomingStrike(const FMKHAttackCandidateInput* OpponentAttack, const FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Opens the punish windows the bot's reading allows and outputs PunishWindowScore, the best of them.
	 *
	 * @param OpponentAbilitySystem  Opponent's ability system.
	 * @param DataSubsystem          Data subsystem (for the danger of the opponent's abilities).
	 * @param Data                   Instance data to update.
	 */
	static void UpdatePunishWindows(const UAbilitySystemComponent& OpponentAbilitySystem, const UMKHDataSubsystem& DataSubsystem,
		FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * DangerousInCooldown window: at least one dangerous ability of the opponent's weapons is in cooldown; the score
	 * falls with the danger of the abilities it can still use.
	 *
	 * @param OpponentAbilitySystem  Opponent's ability system (cooldown tags).
	 * @param DataSubsystem          Data subsystem.
	 * @param Data                   Instance data (weapons, attributes, parameters).
	 * @return The window score, 0 when closed.
	 */
	static float ComputeDangerousInCooldownScore(const UAbilitySystemComponent& OpponentAbilitySystem,
		const UMKHDataSubsystem& DataSubsystem, const FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/**
	 * Danger of one of the opponent's abilities for the bot (FMKHAttackScoring::ComputeAbilityDanger).
	 *
	 * @param DataSubsystem     Data subsystem.
	 * @param Ability           The ability's definition.
	 * @param WeaponBaseDamage  Base damage of the weapon granting it.
	 * @param Data              Instance data (attributes, danger parameters).
	 * @return The danger, 0-1; 0 for a non damage ability.
	 */
	static float ComputeOpponentAbilityDanger(const UMKHDataSubsystem& DataSubsystem, const FEquipmentAbilityDefinition& Ability,
		float WeaponBaseDamage, const FMKHDuelBotPerceptionEvaluatorInstanceData& Data);

	/** Value of the "seconds since" outputs when the event did not happen recently, far beyond any curve range. */
	static constexpr float NoRecentEventSeconds = 1000.f;
	
	static void CheckBaitAwareness(FStateTreeExecutionContext& Context, const float DeltaTime, FMKHDuelBotPerceptionEvaluatorInstanceData& Data);
};
