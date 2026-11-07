// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/MKHDifficultyAdjustment.h"

float FMKHDifficultyAdjustment::ComputeRoundScore(const float DamageDealt, const float DamageTaken)
{
	const float Dealt = FMath::Max(DamageDealt, 0.f);
	const float Taken = FMath::Max(DamageTaken, 0.f);
	const float Total = Dealt + Taken;
	return Total > 0.f ? (Dealt - Taken) / Total : 0.f;
}

int32 FMKHDifficultyAdjustment::ComputeNextLevel(const int32 CurrentLevel, const float RoundScore, const FMKHDifficultyAdjustmentSettings& Settings)
{
	const int32 LastLevel = FMath::Max(Settings.ProfileOrder.Num() - 1, 0);

	if (CurrentLevel >= LastLevel)
	{
		return LastLevel;
	}

	int32 Step = 0;
	if (RoundScore > Settings.RaiseThreshold)
	{
		Step = 1;
	}
	else if (RoundScore < -Settings.LowerThreshold)
	{
		Step = -1;
	}

	return FMath::Clamp(CurrentLevel + Step, 0, LastLevel);
}
