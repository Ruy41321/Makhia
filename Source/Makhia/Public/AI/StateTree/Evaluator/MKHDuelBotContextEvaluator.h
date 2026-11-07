// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "Inventory/MKHItemEntry.h"
#include "MKHDuelBotContextEvaluator.generated.h"

class AAIController;
class ACharacterBase;
struct FGameplayTag;
struct FStateTreeExecutionContext;

/** How far the bot is from its opponent, from the closest band to the farthest. */
UENUM(BlueprintType)
enum class EMKHDuelDistanceBand : uint8
{
	/** Within attack reach. */
	Combat UMETA(DisplayName = "Combat"),

	/** Out of reach, close enough to step in. */
	Safe UMETA(DisplayName = "Safe"),

	/** Too far to fight, the bot has to approach. */
	Far UMETA(DisplayName = "Far")
};

/**
 * Instance data of FMKHDuelBotContextEvaluator: the bot's controller (bound from the StateTree context) and the duel
 * context (who fights whom, how far, whether the fight can go on), exposed as outputs for conditions and tasks.
 */
USTRUCT()
struct FMKHDuelBotContextEvaluatorInstanceData
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
		
	/** Up to this distance the bot is in the Combat band, beyond it is in the Safe band. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Units = "cm", ClampMin = 0.f))
	float CombatBandMaxDistance = 250.f;

	/** Up to this distance the bot is in the Safe band, beyond it in the Far band. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Units = "cm", ClampMin = 0.f))
	float SafeBandMaxDistance = 600.f;

	/** Extra distance needed to leave a band outwards, so the band does not flicker at a boundary. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Units = "cm", ClampMin = 0.f))
	float BandHysteresis = 50.f;
	
	// ============================================================
	// Output
	// ============================================================	
	
	/** Behavior parameters of the bot's difficulty profile, copied every tick; tasks bind to its fields. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHDuelBotTuning Tuning;

	/** Character the bot controls, null until the controller has one. */
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<ACharacterBase> BotCharacter = nullptr;

	/** Character the bot fights, null until the controller has found one. */
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<ACharacterBase> Opponent = nullptr;

	/** Distance between the bot and the opponent in cm; 0 while there is no opponent. */
	UPROPERTY(EditAnywhere, Category = "Output")
	float Distance = 0.f;

	/** Band of Distance, with hysteresis; Far while there is no opponent. */
	UPROPERTY(EditAnywhere, Category = "Output")
	EMKHDuelDistanceBand DistanceBand = EMKHDuelDistanceBand::Far;

	/** Where an approach should stop: inside the Combat band by BandHysteresis (bind Move To's acceptance radius). */
	UPROPERTY(EditAnywhere, Category = "Output")
	float ApproachStopDistance = 0.f;

	/** True while the duel runs, there is an opponent and neither side is dead. */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bCanFight = false;

	/** True while the bot is alive and its opponent is dead: from the round's decisive death to the round reset. */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bWonRound = false;

	/** True when the bot won the round and its Tuning.TauntChance roll, made once per won round, passed. */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bShouldTaunt = false;

	/** The bot's current weapon MKHItemEntry (contains the effect package). */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHItemEntry CurrentWeaponEntry = FMKHItemEntry();
	
	/** The bot's sheathed weapon MKHItemEntry (contains the effect package). */
	UPROPERTY(EditAnywhere, Category = "Output")
	FMKHItemEntry SheathedWeaponEntry = FMKHItemEntry();
	
};

/**
 * StateTree evaluator of the duel bot: every tick it reads the bot and its opponent from AMKHDuelBotController and
 * publishes the tuning of the bot's profile, the item entries of its two weapons (in hand and sheathed, unchanged),
 * the distance and its band, whether the fight can go on and whether the bot won the round (and taunts), so every
 * state reads the same context through bindings. Place it first: the other evaluators may bind to its outputs.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Duel Bot Context", Category = "MKH"))
struct MAKHIA_API FMKHDuelBotContextEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	/** Type of the per-tree data this evaluator reads and writes. */
	using FInstanceDataType = FMKHDuelBotContextEvaluatorInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Refreshes the bot, the opponent, the tuning of the bot's profile, the bot's two weapon entries, the distance,
	 * its band, bCanFight, bWonRound and bShouldTaunt.
	 *
	 * @param Context    Execution context of the running StateTree, gives access to the instance data.
	 * @param DeltaTime  Seconds since the last tick.
	 */
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Finds the weapon a duel character holds, through its controller's IEquipmentInterface (player and bot alike).
	 *
	 * @param Character  Character to read.
	 * @return The equipped weapon's item entry, valid until the equipment changes; null when unarmed.
	 */
	static const FMKHItemEntry* FindCurrentWeapon(const ACharacterBase& Character);

	/**
	 * Finds the weapon a duel character would swap to: the first weapon quick slot holding another weapon than the
	 * one in hand (AMKHDuelBot::GetOtherWeaponQuickSlot's rule), read through its controller's item interfaces. The
	 * inventory only hands out copies, so the entry is copied only when the sheathed weapon changes (by ItemID).
	 *
	 * @param Character      Character to read.
	 * @param InOutWeapon    The sheathed weapon known so far; updated when it changed, emptied when there is none.
	 */
	static void UpdateSheathedWeapon(const ACharacterBase& Character, FMKHItemEntry& InOutWeapon);

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/** Resets every output to its "no opponent" value; the weapon entries are reset only when there is no bot. */
	static void ResetOutputs(FMKHDuelBotContextEvaluatorInstanceData& Data);

	/**
	 * Copies an item entry into an output only when it is another item (by ItemID), so a held weapon is not copied
	 * every tick: an item's rolls never change after its generation.
	 *
	 * @param Source  Entry to publish, null for none.
	 * @param Target  Output to update; set to an empty entry when Source is null.
	 */
	static void CopyEntryIfChanged(const FMKHItemEntry* Source, FMKHItemEntry& Target);

	/**
	 * Outputs the distance, its band, the approach stop distance, bCanFight and bWonRound.
	 *
	 * @param Context       Execution context of the running StateTree, gives the world and its game state.
	 * @param BotCharacter  The bot.
	 * @param Opponent      The bot's opponent.
	 * @param PreviousBand  Band of the previous tick, for the hysteresis.
	 * @param Data          Instance data to update.
	 */
	static void UpdateCombatMetrics(const FStateTreeExecutionContext& Context, const ACharacterBase& BotCharacter,
		const ACharacterBase& Opponent, EMKHDuelDistanceBand PreviousBand, FMKHDuelBotContextEvaluatorInstanceData& Data);

	/**
	 * Rolls Tuning.TauntChance once, when bWonRound turns true, and keeps the result until the round is reset: a roll
	 * every tick would turn any chance into a certainty.
	 *
	 * @param bWonLastTick      bWonRound of the previous tick.
	 * @param bTauntedLastTick  bShouldTaunt of the previous tick.
	 * @param Data              Instance data: reads bWonRound and Tuning, writes bShouldTaunt.
	 */
	static void UpdateTaunt(bool bWonLastTick, bool bTauntedLastTick, FMKHDuelBotContextEvaluatorInstanceData& Data);

	/**
	 * Sends Event.AI.DistanceBand.<Band> the tick DistanceBand changes, so transitions can react to a band change once
	 * (on the exact band tag, or on the parent Event.AI.DistanceBand for any change) instead of testing it every tick.
	 *
	 * @param Context       Execution context of the running StateTree, receives the event.
	 * @param PreviousBand  Band of the previous tick.
	 * @param Data          Instance data: the new band and the distance, for the log.
	 */
	static void SendDistanceBandEventOnChange(FStateTreeExecutionContext& Context, EMKHDuelDistanceBand PreviousBand,
		const FMKHDuelBotContextEvaluatorInstanceData& Data);

	/**
	 * @param Band  A distance band.
	 * @return The StateTree event tag sent when the bot enters Band.
	 */
	static FGameplayTag GetDistanceBandEventTag(EMKHDuelDistanceBand Band);

	/**
	 * Picks the band of a distance. Leaving a band outwards needs the boundary plus BandHysteresis, entering a band
	 * inwards only the boundary, so a distance hovering on a boundary keeps the current band.
	 *
	 * @param Distance     Distance between the bot and its opponent, in cm.
	 * @param CurrentBand  Band the bot is in before this update.
	 * @param Data         Instance data: the band thresholds.
	 * @return The band of Distance.
	 */
	static EMKHDuelDistanceBand ComputeDistanceBand(float Distance, EMKHDuelDistanceBand CurrentBand,
		const FMKHDuelBotContextEvaluatorInstanceData& Data);
	
};
