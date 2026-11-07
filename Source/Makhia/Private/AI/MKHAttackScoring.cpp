// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/MKHAttackScoring.h"

#include "Math/RandomStream.h"

// ============================================================
// Public Interface
// ============================================================

float FMKHAttackScoring::ComputeDamageAdvantage(const float BotOutput, const float BotIncoming, const float OpponentOutput,
	const float OpponentIncoming)
{
	// A zero denominator (opponent deals no damage) is a huge advantage, not a division by zero.
	const float Denominator = FMath::Max(OpponentOutput * BotIncoming, UE_KINDA_SMALL_NUMBER);
	return FMath::Max(BotOutput * OpponentIncoming, 0.f) / Denominator;
}

TArray<FMKHAttackScoreBreakdown> FMKHAttackScoring::ScoreCandidates(TConstArrayView<FMKHAttackCandidateInput> Candidates,
	const FMKHAttackScoringContext& Context, const FMKHAttackScoringParams& Params)
{
	TArray<FMKHAttackScoreBreakdown> Breakdowns;
	Breakdowns.SetNum(Candidates.Num());

	// First pass: the damage score is relative to the strongest candidate.
	float MaxExpectedDamage = 0.f;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		FMKHAttackScoreBreakdown& Breakdown = Breakdowns[Index];
		Breakdown.ExpectedDamage = ComputeExpectedDamage(Candidates[Index], Context, Params);
		Breakdown.bLethal = Context.OpponentHealth > 0.f && Breakdown.ExpectedDamage >= Context.OpponentHealth;
		Breakdown.bBreaksGuard = Context.bOpponentBlocking && BreaksGuard(Candidates[Index], Context);
		MaxExpectedDamage = FMath::Max(MaxExpectedDamage, Breakdown.ExpectedDamage);
	}

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FMKHAttackCandidateInput& Candidate = Candidates[Index];
		FMKHAttackScoreBreakdown& Breakdown = Breakdowns[Index];
		Breakdown.Distance = ScoreDistance(Candidate, Context, Params);
		Breakdown.Damage = ScoreDamage(Breakdown.ExpectedDamage, MaxExpectedDamage, Breakdown.bLethal, Params);
		Breakdown.GuardBreak = ScoreGuardBreak(Candidate, Context, Params);
		Breakdown.StatusEffect = ScoreStatusEffects(Candidate, Context, Params);
		Breakdown.CrowdControlFit = ScoreCrowdControlFit(Candidate, Context, Params);
		Breakdown.CooldownEconomy = ScoreCooldownEconomy(Candidate, Context, Breakdown.bLethal, Params);
		Breakdown.Variety = ScoreVariety(Candidate, Params);
		Breakdown.Speed = ScoreSpeed(Candidate, Context, Params);
		Breakdown.Total = Breakdown.Distance * Breakdown.Damage * Breakdown.GuardBreak * Breakdown.StatusEffect
			* Breakdown.CrowdControlFit * Breakdown.CooldownEconomy * Breakdown.Variety * Breakdown.Speed;
	}
	return Breakdowns;
}

TArray<float> FMKHAttackScoring::ComputeChoiceProbabilities(TConstArrayView<float> Scores, const float Sharpness,
	const bool bExactArgmax)
{
	TArray<float> Probabilities;
	Probabilities.Init(0.f, Scores.Num());
	if (Scores.IsEmpty())
	{
		return Probabilities;
	}

	if (bExactArgmax)
	{
		int32 BestIndex = 0;
		for (int32 Index = 1; Index < Scores.Num(); ++Index)
		{
			BestIndex = Scores[Index] > Scores[BestIndex] ? Index : BestIndex;
		}
		Probabilities[BestIndex] = 1.f;
		return Probabilities;
	}

	float Sum = 0.f;
	for (int32 Index = 0; Index < Scores.Num(); ++Index)
	{
		Probabilities[Index] = ApplySharpness(Scores[Index], Sharpness);
		Sum += Probabilities[Index];
	}

	// Every score 0 with k > 0: no preference left, fall back to a uniform choice.
	for (float& Probability : Probabilities)
	{
		Probability = Sum > 0.f ? Probability / Sum : 1.f / Scores.Num();
	}
	return Probabilities;
}

float FMKHAttackScoring::ComputeExpectedUtility(TConstArrayView<float> Scores, TConstArrayView<float> Probabilities)
{
	float Utility = 0.f;
	for (int32 Index = 0; Index < FMath::Min(Scores.Num(), Probabilities.Num()); ++Index)
	{
		Utility += Scores[Index] * Probabilities[Index];
	}
	return Utility;
}

int32 FMKHAttackScoring::PickIndex(TConstArrayView<float> Probabilities, FRandomStream& RandomStream)
{
	if (Probabilities.IsEmpty())
	{
		return INDEX_NONE;
	}

	const float Roll = RandomStream.FRand();
	float Cumulative = 0.f;
	for (int32 Index = 0; Index < Probabilities.Num(); ++Index)
	{
		Cumulative += Probabilities[Index];
		if (Roll < Cumulative)
		{
			return Index;
		}
	}

	// Rounding can leave the sum just under 1: the roll then belongs to the last candidate with a chance.
	for (int32 Index = Probabilities.Num() - 1; Index >= 0; --Index)
	{
		if (Probabilities[Index] > 0.f)
		{
			return Index;
		}
	}
	return Probabilities.Num() - 1;
}

float FMKHAttackScoring::ApplySharpness(const float Score, const float Sharpness)
{
	if (Sharpness <= 0.f)
	{
		return 1.f;
	}
	return FMath::Pow(FMath::Max(Score, 0.f), Sharpness);
}

float FMKHAttackScoring::ComputeAbilityDanger(const FMKHAttackCandidateInput& Ability, const float OpponentOutput,
	const float BotIncoming, const float BotArmor, const float BotHealth, const FMKHDangerParams& Params)
{
	float StatusDanger = 0.f;
	float MaxEffectiveMultiplier = 0.f;
	float TotalDamagePercent = 0.f;
	for (const FMKHAttackStrikeInput& Strike : Ability.Strikes)
	{
		StatusDanger = FMath::Max(StatusDanger, Strike.CrowdControl == EMKHCrowdControl::Stun ? Params.StunDanger
			: Strike.CrowdControl == EMKHCrowdControl::Stagger ? Params.StaggerDanger : 0.f);
		MaxEffectiveMultiplier = FMath::Max(MaxEffectiveMultiplier, Strike.DamagePercent * OpponentOutput * BotIncoming);
		TotalDamagePercent += Strike.DamagePercent;
	}

	// Below the threshold a strike is only as dangerous as it is strong; above it, by the health it takes.
	const float Threshold = FMath::Max(Params.HighDamageThreshold, UE_KINDA_SMALL_NUMBER);
	float DamageDanger = Params.HighDamageDanger * FMath::Clamp(MaxEffectiveMultiplier / Threshold, 0.f, 1.f);
	if (MaxEffectiveMultiplier >= Threshold && BotHealth > 0.f)
	{
		const float HealthDamage = Ability.WeaponBaseDamage * TotalDamagePercent * OpponentOutput * BotIncoming * (1.f - BotArmor);
		const float HealthShare = FMath::Clamp(HealthDamage / BotHealth, 0.f, 1.f);
		DamageDanger = FMath::Lerp(Params.HighDamageDanger, 1.f, HealthShare);
	}
	return FMath::Max(StatusDanger, DamageDanger);
}

float FMKHAttackScoring::ComputeExpectedDamage(const FMKHAttackCandidateInput& Candidate,
	const FMKHAttackScoringContext& Context, const FMKHAttackScoringParams& Params)
{
	float TotalDamagePercent = 0.f;
	for (const FMKHAttackStrikeInput& Strike : Candidate.Strikes)
	{
		TotalDamagePercent += Strike.DamagePercent;
	}

	// Same chain as UExecCalc_Damage, without the critical roll: base x multiplier x modifiers x armor.
	const float Damage = Candidate.WeaponBaseDamage * TotalDamagePercent * Context.BotOutputMultiplier
		* Context.OpponentIncomingMultiplier * (1.f - FMath::Clamp(Context.OpponentArmorMitigation, 0.f, 1.f));
	const bool bBlocked = Context.bOpponentBlocking && !BreaksGuard(Candidate, Context);
	return FMath::Max(bBlocked ? Damage * Params.BlockedDamageFactor : Damage, 0.f);
}

bool FMKHAttackScoring::BreaksGuard(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context)
{
	float StaminaDamage = 0.f;
	for (const FMKHAttackStrikeInput& Strike : Candidate.Strikes)
	{
		StaminaDamage += Strike.StaminaDamage;
	}

	// UExecCalc_Damage::RefineStaminaDamage: the defender's weapon absorbs its block stability share.
	StaminaDamage *= 1.f - FMath::Clamp(Context.OpponentBlockStability, 0.f, 1.f);
	return StaminaDamage > 0.f && StaminaDamage >= Context.OpponentStamina;
}

float FMKHAttackScoring::ScoreDistance(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
	const FMKHAttackScoringParams& Params)
{
	const float OutOfReach = FMath::Max(Context.Distance - Candidate.Reach, 0.f);
	const float Linear = 1.f - FMath::Clamp(OutOfReach / FMath::Max(Params.ApproachFalloffDistance, 1.f), 0.f, 1.f);
	return FMath::Lerp(Params.MinDistanceScore, 1.f, ApplyCurve(Params.DistanceCurve, Linear));
}

float FMKHAttackScoring::ScoreDamage(const float ExpectedDamage, const float MaxExpectedDamage, const bool bLethal,
	const FMKHAttackScoringParams& Params)
{
	if (bLethal)
	{
		return 1.f;
	}
	const float Linear = MaxExpectedDamage > 0.f ? FMath::Clamp(ExpectedDamage / MaxExpectedDamage, 0.f, 1.f) : 1.f;
	return FMath::Lerp(Params.MinDamageScore, 1.f, ApplyCurve(Params.DamageCurve, Linear));
}

float FMKHAttackScoring::ScoreGuardBreak(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
	const FMKHAttackScoringParams& Params)
{
	if (!Context.bOpponentBlocking || BreaksGuard(Candidate, Context))
	{
		return 1.f;
	}

	float StaminaDamage = 0.f;
	for (const FMKHAttackStrikeInput& Strike : Candidate.Strikes)
	{
		StaminaDamage += Strike.StaminaDamage;
	}
	StaminaDamage *= 1.f - FMath::Clamp(Context.OpponentBlockStability, 0.f, 1.f);
	const float StaminaShare = Context.OpponentStamina > 0.f ? FMath::Clamp(StaminaDamage / Context.OpponentStamina, 0.f, 1.f) : 1.f;
	return FMath::Lerp(Params.MinGuardScore, Params.MaxGuardScoreWithoutBreak, StaminaShare);
}

float FMKHAttackScoring::ScoreStatusEffects(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
	const FMKHAttackScoringParams& Params)
{
	// Status effects always count (they add up), weighed by how much they move the damage advantage.
	const FMKHDamageModifierShift Shift = SumClampedShift(Candidate, Context.ModifierRoom);
	const float AdvantageBefore = ComputeDamageAdvantage(Context.BotOutputMultiplier, Context.BotIncomingMultiplier,
		Context.OpponentOutputMultiplier, Context.OpponentIncomingMultiplier);
	const float AdvantageAfter = ComputeDamageAdvantage(Context.BotOutputMultiplier + Shift.BotOutput,
		Context.BotIncomingMultiplier + Shift.BotIncoming, Context.OpponentOutputMultiplier + Shift.OpponentOutput,
		Context.OpponentIncomingMultiplier + Shift.OpponentIncoming);
	const float Gain = AdvantageBefore > 0.f ? AdvantageAfter / AdvantageBefore - 1.f : 0.f;

	const EMKHCrowdControl CrowdControl = GetStrongestCrowdControl(Candidate);
	const float CrowdControlValue = CrowdControl == EMKHCrowdControl::Stun ? Params.StunValue
		: CrowdControl == EMKHCrowdControl::Stagger ? Params.StaggerValue : 0.f;

	const float Linear = FMath::Clamp(Params.BaseStatusScore + Params.StatusGainScale * Gain + CrowdControlValue, 0.f, 1.f);
	return ApplyCurve(Params.StatusCurve, Linear);
}

float FMKHAttackScoring::ScoreCrowdControlFit(const FMKHAttackCandidateInput& Candidate,
	const FMKHAttackScoringContext& Context, const FMKHAttackScoringParams& Params)
{
	if (Context.OpponentCrowdControl == EMKHCrowdControl::None)
	{
		return 1.f;
	}

	const float TimeLeft = FMath::Max(Context.OpponentCrowdControlTimeLeft, 0.f);
	const float TimeToHit = ComputeTimeToFirstHit(Candidate, Context);
	if (TimeLeft <= 0.f || TimeToHit > TimeLeft)
	{
		return Params.CrowdControlMissScore;
	}

	// Stun: the slowest (heaviest) attack that still lands; stagger: the fastest one.
	const float UsedShare = TimeToHit / TimeLeft;
	const float Fit = Context.OpponentCrowdControl == EMKHCrowdControl::Stun ? UsedShare : 1.f - UsedShare;
	return FMath::Lerp(Params.MinCrowdControlFitScore, 1.f, ApplyCurve(Params.CrowdControlCurve, Fit));
}

float FMKHAttackScoring::ScoreCooldownEconomy(const FMKHAttackCandidateInput& Candidate,
	const FMKHAttackScoringContext& Context, const bool bLethal, const FMKHAttackScoringParams& Params)
{
	if (Candidate.CooldownTime <= 0.f)
	{
		return 1.f;
	}

	const float Cost = Params.MaxCooldownCost * FMath::Clamp(Candidate.CooldownTime / Params.CooldownReferenceTime, 0.f, 1.f);
	const float Discount = bLethal ? 1.f : FMath::Clamp(Context.PunishWindowScore, 0.f, 1.f);
	return 1.f - Cost * (1.f - Discount);
}

float FMKHAttackScoring::ScoreVariety(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringParams& Params)
{
	if (Candidate.bIsLight)
	{
		return Candidate.bNeedsSwap ? Params.LightSwapScore : 1.f;
	}
	return Candidate.bIsLastUsed ? Params.RepeatScore : 1.f;
}

float FMKHAttackScoring::ScoreSpeed(const FMKHAttackCandidateInput& Candidate, const FMKHAttackScoringContext& Context,
	const FMKHAttackScoringParams& Params)
{
	if (Context.OpponentCrowdControl != EMKHCrowdControl::None || Candidate.Strikes.IsEmpty())
	{
		return 1.f;
	}

	const float SlowRange = FMath::Max(Params.SlowHitTime - Params.FastHitTime, UE_KINDA_SMALL_NUMBER);
	const float Linear = 1.f - FMath::Clamp((Candidate.Strikes[0].TimeToHit - Params.FastHitTime) / SlowRange, 0.f, 1.f);
	return FMath::Lerp(Params.MinSpeedScore, 1.f, ApplyCurve(Params.SpeedCurve, Linear));
}

// ============================================================
// Protected / Internal Logic
// ============================================================

float FMKHAttackScoring::ApplyCurve(const FRuntimeFloatCurve& Curve, const float Score)
{
	const FRichCurve* RichCurve = Curve.GetRichCurveConst();
	if (RichCurve == nullptr || RichCurve->IsEmpty())
	{
		return Score;
	}
	return FMath::Clamp(RichCurve->Eval(Score), 0.f, 1.f);
}

FMKHDamageModifierShift FMKHAttackScoring::SumClampedShift(const FMKHAttackCandidateInput& Candidate,
	const FMKHDamageModifierShift& Room)
{
	FMKHDamageModifierShift Sum;
	for (const FMKHAttackStrikeInput& Strike : Candidate.Strikes)
	{
		Sum.BotOutput += Strike.ModifierShift.BotOutput;
		Sum.BotIncoming += Strike.ModifierShift.BotIncoming;
		Sum.OpponentOutput += Strike.ModifierShift.OpponentOutput;
		Sum.OpponentIncoming += Strike.ModifierShift.OpponentIncoming;
	}

	// Favourable shifts stop at the attribute caps; unfavourable ones are kept whole.
	Sum.BotOutput = FMath::Min(Sum.BotOutput, FMath::Max(Room.BotOutput, 0.f));
	Sum.BotIncoming = FMath::Max(Sum.BotIncoming, -FMath::Max(Room.BotIncoming, 0.f));
	Sum.OpponentOutput = FMath::Max(Sum.OpponentOutput, -FMath::Max(Room.OpponentOutput, 0.f));
	Sum.OpponentIncoming = FMath::Min(Sum.OpponentIncoming, FMath::Max(Room.OpponentIncoming, 0.f));
	return Sum;
}

EMKHCrowdControl FMKHAttackScoring::GetStrongestCrowdControl(const FMKHAttackCandidateInput& Candidate)
{
	EMKHCrowdControl Strongest = EMKHCrowdControl::None;
	for (const FMKHAttackStrikeInput& Strike : Candidate.Strikes)
	{
		Strongest = FMath::Max(Strongest, Strike.CrowdControl);
	}
	return Strongest;
}

float FMKHAttackScoring::ComputeTimeToFirstHit(const FMKHAttackCandidateInput& Candidate,
	const FMKHAttackScoringContext& Context)
{
	const float ApproachDistance = FMath::Max(Context.Distance - Candidate.Reach, 0.f);
	const float ApproachTime = ApproachDistance / FMath::Max(Context.ApproachSpeed, 1.f);
	const float StrikeTime = Candidate.Strikes.IsEmpty() ? 0.f : Candidate.Strikes[0].TimeToHit;
	return ApproachTime + StrikeTime;
}
