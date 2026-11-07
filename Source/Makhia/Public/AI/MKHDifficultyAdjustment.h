// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHDifficultyAdjustment.generated.h"

/** Rules of the Random Duel's difficulty adjustment. Full model: see Documentation/OfflineModes.md. */
USTRUCT(BlueprintType)
struct FMKHDifficultyAdjustmentSettings
{
	GENERATED_BODY()

	/** Difficulty profiles from the easiest to the hardest (UMKHDuelBotProfile::GetProfileId). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	TArray<FName> ProfileOrder = { TEXT("Easy"), TEXT("Medium"), TEXT("Hard") };

	/** Index in ProfileOrder every match starts from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0"))
	int32 StartingLevel = 1;

	/** Round score above which the next round is one level harder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RaiseThreshold = 0.35f;

	/** Round score below which (as a negative value) the next round is one level easier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowerThreshold = 0.35f;
};

/** Pure rules of the difficulty adjustment, covered by the Makhia.AI.DifficultyAdjustment spec. */
struct MAKHIA_API FMKHDifficultyAdjustment
{
	/**
	 * Scores how one-sided a round was: (dealt - taken) / (dealt + taken).
	 *
	 * @param DamageDealt  Damage the player dealt to the bot in the round.
	 * @param DamageTaken  Damage the player took from the bot in the round.
	 * @return From -1 (the player only took damage) to +1 (the player only dealt it); 0 when nobody was hit.
	 */
	static float ComputeRoundScore(float DamageDealt, float DamageTaken);

	/**
	 * Picks the difficulty of the next round: at most one level away from the current one. The hardest level is final:
	 * once reached it never lowers, so a player cannot throw a round to face an easier opponent in the next one.
	 *
	 * @param CurrentLevel  Index in Settings.ProfileOrder of the round just played.
	 * @param RoundScore    Score of that round (ComputeRoundScore).
	 * @param Settings      Thresholds and profile order.
	 * @return The index in Settings.ProfileOrder of the next round.
	 */
	static int32 ComputeNextLevel(int32 CurrentLevel, float RoundScore, const FMKHDifficultyAdjustmentSettings& Settings);
};
