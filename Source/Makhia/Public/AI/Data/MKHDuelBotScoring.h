// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AI/MKHAttackScoring.h"
#include "AI/MKHStateScoring.h"
#include "MKHDuelBotScoring.generated.h"

/** What the Perception evaluator sees and how open each punish window is. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHPerceptionParams
{
	GENERATED_BODY()

	/** Seconds after the end of an opponent attack during which it still recovers (Recovery punish window). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Punish Windows", meta = (Units = "s", ClampMin = 0.01f))
	float RecoveryWindowDuration = 0.6f;

	/** Punish window score right at the end of an opponent attack, fading to 0 over RecoveryWindowDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Punish Windows", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float RecoveryWindowScore = 1.f;

	/** Punish window score while the opponent is out of stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Punish Windows", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float OutOfStaminaWindowScore = 1.f;

	/** Punish window score while the opponent cannot afford a dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Punish Windows", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float LastDodgeWindowScore = 0.6f;

	/** Punish window score while the opponent's dangerous abilities are in cooldown and none is available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Punish Windows", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float DangerousInCooldownWindowScore = 0.5f;

	/** Danger from which an opponent ability counts as dangerous (DangerousInCooldown window). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Punish Windows", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float DangerousAbilityThreshold = 0.7f;

	/** How dangerous the opponent's abilities are for the bot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger")
	FMKHDangerParams Danger;

	/** Stun length the bot assumes when its reading is below High (it does not see the time left). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd Control", meta = (Units = "s", ClampMin = 0.f))
	float TypicalStunDuration = 2.f;

	/** Stagger length the bot assumes when its reading is below High. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd Control", meta = (Units = "s", ClampMin = 0.f))
	float TypicalStaggerDuration = 0.8f;
};

/**
 * Generic scoring parameters of the duel bot (one row of DT_DuelBotScoring), shared by the profiles that reference
 * the table: attack considerations, perception and punish windows, danger and state scores. The difficulty profile
 * (UMKHDuelBotProfile) keeps only the personality; a table per difficulty is possible but not needed today.
 *
 * Full model: see Documentation/DuelBotAI.md (Tuning).
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHDuelBotScoringRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Considerations of the attack choice (Attack Options evaluator). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Choice")
	FMKHAttackScoringParams AttackScoring;

	/** Share of an attack's reach the bot closes in to before attacking (below 1: inside the reach, not at its edge). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Choice", meta = (ClampMin = 0.1f, ClampMax = 1.f))
	float EngageDistanceFactor = 0.8f;

	/** Perception, punish windows and danger (Perception evaluator). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception")
	FMKHPerceptionParams Perception;

	/** State scores (Decisions evaluator). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "States")
	FMKHStateScoringParams States;
};
