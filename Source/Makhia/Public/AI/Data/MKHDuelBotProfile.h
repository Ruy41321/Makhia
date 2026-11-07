// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MKHDuelBotProfile.generated.h"

class UDataTable;
struct FMKHDuelBotScoringRow;
struct FMKHStartingItem;

UENUM(BlueprintType)
enum class EOpponentReadingFactor : uint8
{
	/** Choices based only on the opponent's state and attributes.. */
	Low UMETA(DisplayName = "Low"),

	/** choices based also on the opponent's current attack type / details. */
	Medium UMETA(DisplayName = "Medium"),

	/** Choices based also on the opponent's available & on CD attacks. */
	High UMETA(DisplayName = "High")
};

/**
 * Behaviour parameters of the duel bot for one difficulty level. Difficulty changes how the bot decides and reacts,
 * and its loadout (UMKHDuelBotProfile::BuildLoadout), never its base attributes. The StateTree reads it through the
 * MKH Duel Opponent evaluator's Tuning output.
 *
 * Profiles and their values: see Documentation/MatchFlowArchitecture.md (Bot AI).
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHDuelBotTuning
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

	/** Sharpness k reached at Expertise 1 (score^6: a 0.9 against a 0.6 is chosen ~92% of the time). */
	static constexpr float MaxDecisionSharpness = 6.f;

	/**
	 * Sharpness of the bot's choices: every utility score is raised to k = Expertise x MaxDecisionSharpness before the
	 * weighted random choice (FMKHAttackScoring::ApplySharpness). The only place where Expertise becomes k.
	 *
	 * @return k, 0 (uniform choice) to MaxDecisionSharpness.
	 */
	float GetDecisionSharpness() const { return ComputeDecisionSharpness(Expertise); }

	/**
	 * GetDecisionSharpness for a bound Expertise value (StateTree considerations receive the float, not the tuning).
	 *
	 * @param InExpertise  Expertise 0-1.
	 * @return k = InExpertise x MaxDecisionSharpness.
	 */
	static float ComputeDecisionSharpness(const float InExpertise) { return FMath::Clamp(InExpertise, 0.f, 1.f) * MaxDecisionSharpness; }

	/** @return True when the bot always takes the best attack (Expertise 1: exact argmax in the attack choice). */
	bool IsExactChoice() const { return Expertise >= 1.f; }

	// ============================================================
	// Properties
	// ============================================================

	/** 0 cautious - 1 aggressive: raises the Attack and counterattack scores (FMKHStateScoring). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Personality", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float Aggression = 0.6f;
	
	/** 0 not willing - 1 very willing: scales the Bait score (FMKHStateScoring). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Personality", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float BaitWilling = 0.5f;

	/** 0 never - 1 always: chance, rolled once per won round, that the bot taunts the defeated opponent (crouch spam). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Personality", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float TauntChance = 0.4f;

	/** Delay between the start of the opponent's attack and the bot's reaction, rolled in this range once per attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = 0.f, Units = "s"))
	FFloatInterval ReactionTimeRange = FFloatInterval(0.25f, 0.4f);

	/** Chance, rolled once per opponent attack, that the bot reacts to it at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float ReactionChance = 0.5f;
	
	/** ReactionTimeRange while the bait awareness is on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = 0.f, Units = "s"))
	FFloatInterval BaitReactionTimeRange = FFloatInterval(0.15f, 0.25f);

	/** ReactionChance while the bait awareness is on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float BaitReactionChance = 0.95f;
			
	/**
	 * How sharp the bot's choices are, only that (never how much it knows: see OpponentReadingFactor). Every choice is
	 * a weighted random one with probabilities p_i = s_i^k / sum(s_j^k), k = Expertise x MaxDecisionSharpness
	 * (GetDecisionSharpness). 0: uniform choice among the options; 0.5 (k 3): mostly the good ones, mistakes stay
	 * possible; 1: the attack choice always takes the best option (exact argmax), the states with considerations use
	 * k = 6. A better option always has a higher chance than a worse one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Expertise", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float Expertise = 1.f;

	/** Factor to adjust the bot's reading of the opponent's behavior. 
	 * Low: the bot bases his choices only based on the opponent's state and attributes.
	 * Medium: the bot bases his choices also on the opponent's current attack type / details.
	 * High: the bot bases his choices also on the opponent's available & on CD attacks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Expertise", meta = (ClampMin = 0.f, ClampMax = 1.f))
	EOpponentReadingFactor OpponentReadingFactor = EOpponentReadingFactor::High;
	
};

/**
 * Difficulty profile of the duel bot (e.g. DA_DuelBotProfile_Hard). AMKHGameMode picks one from its BotProfiles by
 * ProfileId (URL option ?BotProfile=) and hands it to AMKHDuelBotController::SetProfile.
 */
UCLASS(BlueprintType)
class MAKHIA_API UMKHDuelBotProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

public:
	/** @return Id matched against the ?BotProfile= URL option (e.g. "Hard"). */
	UFUNCTION(BlueprintPure, Category = "Duel|AI|Profile")
	FName GetProfileId() const { return ProfileId; }

	/** @return The behaviour parameters of this profile. */
	UFUNCTION(BlueprintPure, Category = "Duel|AI|Profile")
	const FMKHDuelBotTuning& GetTuning() const { return Tuning; }

	/**
	 * Finds the scoring row this profile plays with: ScoringRowName, or the table's first row when it is None.
	 *
	 * @return The row, or null without a table or with a missing row (logged).
	 */
	const FMKHDuelBotScoringRow* FindScoringRow() const;

	/** @return The scoring table (DT_DuelBotScoring), or null. */
	UDataTable* GetScoringTable() const { return ScoringTable; }

	/**
	 * Reads the items the bot plays with from LoadoutTable.
	 *
	 * @return The items in row order; empty without a table (logged): the bot then has no items.
	 */
	TArray<FMKHStartingItem> BuildLoadout() const;

	// ============================================================
	// Properties
	// ============================================================

private:
	/** Id matched against the ?BotProfile= URL option (case insensitive); unique among the game mode's BotProfiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duel|AI|Profile", meta = (AllowPrivateAccess = "true"))
	FName ProfileId;

	/** Behaviour parameters of this profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duel|AI|Profile", meta = (AllowPrivateAccess = "true"))
	FMKHDuelBotTuning Tuning;

	/** Generic scoring parameters (FMKHDuelBotScoringRow), shared by every profile today: DT_DuelBotScoring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duel|AI|Scoring", meta = (AllowPrivateAccess = "true",
		RequiredAssetDataTags = "RowStructure=/Script/Makhia.MKHDuelBotScoringRow"))
	TObjectPtr<UDataTable> ScoringTable;

	/** Row of ScoringTable to use; None picks the first row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duel|AI|Scoring", meta = (AllowPrivateAccess = "true"))
	FName ScoringRowName;

	/** Items the bot gets at spawn and at every rematch, one per row in loadout order (FMKHStartingLoadoutRow). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duel|AI|Loadout", meta = (AllowPrivateAccess = "true",
		RequiredAssetDataTags = "RowStructure=/Script/Makhia.MKHStartingLoadoutRow"))
	TObjectPtr<UDataTable> LoadoutTable;
};
