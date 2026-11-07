// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/MKHStateScoring.h"

// ============================================================
// Lifecycle
// ============================================================

FMKHUtilityCurve::FMKHUtilityCurve(const float MinInput, const float MaxInput, const std::initializer_list<FVector2f> Keys)
	: InputRange(MinInput, MaxInput)
{
	FRichCurve* RichCurve = Curve.GetRichCurve();
	for (const FVector2f& Key : Keys)
	{
		const FKeyHandle Handle = RichCurve->AddKey(Key.X, Key.Y);
		RichCurve->SetKeyInterpMode(Handle, RCIM_Linear);
	}
}

// ============================================================
// Public Interface
// ============================================================

float FMKHUtilityCurve::Evaluate(const float Input) const
{
	const float Normalized = InputRange.Max > InputRange.Min ? FMath::Clamp(InputRange.GetRangePct(Input), 0.f, 1.f) : 0.f;
	const FRichCurve* RichCurve = Curve.GetRichCurveConst();
	const float Score = RichCurve != nullptr && !RichCurve->IsEmpty() ? RichCurve->Eval(Normalized) : Normalized;
	return FMath::Clamp(Score, 0.f, 1.f);
}

FMKHStateScores FMKHStateScoring::ScoreStates(const FMKHStateScoringContext& Context, TConstArrayView<FMKHAttackCandidate> Candidates,
	const FMKHStateScoringParams& Params)
{
	FMKHStateScores Scores;
	Scores.Attack = ScoreAttack(Context, Params.Attack);
	Scores.Bait = ScoreBait(Context, Params.Bait, Scores.BaitChase, Scores.BaitDanger);
	Scores.PlayPassive = ScorePlayPassive(Context, Params.PlayPassive);
	Scores.Block = ScoreBlock(Context, Params.React);
	Scores.Dodge = ScoreDodge(Context, Params.React);
	Scores.Counter = ScoreCounter(Context, Candidates, Params.React, Scores.CounterIndex);

	// React is an argmax: the bait roll decides here, the attack wins whenever one is possible.
	if (Context.bBaitWantsCounter && Scores.CounterIndex != INDEX_NONE)
	{
		Scores.Block = 0.f;
		Scores.Dodge = 0.f;
	}
	return Scores;
}

float FMKHStateScoring::ScoreAttack(const FMKHStateScoringContext& Context, const FMKHAttackStateParams& Params)
{
	return Params.Utility.Evaluate(Context.AttackUtility) * Params.DamageAdvantage.Evaluate(Context.DamageAdvantage)
		* Params.PunishWindow.Evaluate(Context.PunishWindowScore) * Params.Aggression.Evaluate(Context.Aggression)
		* Params.HealthGap.Evaluate(Context.BotHealthRatio - Context.OpponentHealthRatio);
}

float FMKHStateScoring::ScoreBait(const FMKHStateScoringContext& Context, const FMKHBaitStateParams& Params, float& OutChase,
	float& OutDanger)
{
	// Chase: the bot keeps failing to reach an opponent that keeps stepping away.
	OutChase = Params.ChaseTime.Evaluate(Context.UnansweredAttackSeconds) * FMath::Clamp(Context.RetreatShare, 0.f, 1.f);
	OutDanger = Params.Danger.Evaluate(Context.OpponentReadyDanger);
	return FMath::Clamp(Context.BaitWilling, 0.f, 1.f) * Params.Health.Evaluate(Context.BotHealthRatio) * FMath::Max(OutChase, OutDanger);
}

float FMKHStateScoring::ScorePlayPassive(const FMKHStateScoringContext& Context, const FMKHPlayPassiveStateParams& Params)
{
	const float Need = FMath::Max(Params.Health.Evaluate(Context.BotHealthRatio), Params.LowStamina.Evaluate(Context.BotStaminaRatio));
	return Need * Params.DamageDisadvantage.Evaluate(Context.DamageAdvantage) * Params.FreeWindow.Evaluate(Context.OpponentCrowdControlTimeLeft);
}

float FMKHStateScoring::ComputeBlockStaminaDamage(const FMKHStateScoringContext& Context, const FMKHReactStateParams& Params)
{
	const float StaminaDamage = Context.bKnowsIncomingAttack ? Context.IncomingStaminaDamage : Params.AverageIncomingStaminaDamage;
	return FMath::Max(StaminaDamage, 0.f) * (1.f - FMath::Clamp(Context.BotBlockStability, 0.f, 1.f));
}

float FMKHStateScoring::ScoreBlock(const FMKHStateScoringContext& Context, const FMKHReactStateParams& Params)
{
	const float StaminaDamage = ComputeBlockStaminaDamage(Context, Params);
	if (Context.BotStamina <= 0.f || StaminaDamage >= Context.BotStamina)
	{
		// The block would break: a veto, not a low score.
		return 0.f;
	}
	return Params.BlockStaminaShare.Evaluate(StaminaDamage / Context.BotStamina);
}

float FMKHStateScoring::ScoreDodge(const FMKHStateScoringContext& Context, const FMKHReactStateParams& Params)
{
	if (!Context.bCanAffordDodge)
	{
		return 0.f;
	}
	return Params.DodgeHealthGap.Evaluate(Context.OpponentHealthRatio - Context.BotHealthRatio);
}

float FMKHStateScoring::ScoreCounterOption(const FMKHAttackCandidate& Candidate, const FMKHStateScoringContext& Context,
	const FMKHReactStateParams& Params)
{
	// A counter answers now: only attacks that already reach, no approach.
	if (Candidate.Reach < Context.Distance || IsInterruptedBeforeHit(Candidate, Context))
	{
		return 0.f;
	}

	const float CrowdControlValue = Candidate.CrowdControl == EMKHCrowdControl::Stun ? Params.StunCounterValue
		: Candidate.CrowdControl == EMKHCrowdControl::Stagger ? Params.StaggerCounterValue : 0.f;
	const bool bCanTrade = Context.bBaitWantsCounter || Context.BotHealthRatio >= Params.PlainCounterHealthRatio
		|| Context.BotHealthRatio > Context.OpponentHealthRatio;
	const float PlainValue = bCanTrade ? Params.PlainCounterScale * Candidate.Breakdown.Damage * Candidate.Breakdown.StatusEffect : 0.f;

	return FMath::Max(CrowdControlValue, PlainValue) * Params.CounterHealth.Evaluate(Context.BotHealthRatio)
		* Params.CounterAggression.Evaluate(Context.Aggression);
}

float FMKHStateScoring::ScoreCounter(const FMKHStateScoringContext& Context, TConstArrayView<FMKHAttackCandidate> Candidates,
	const FMKHReactStateParams& Params, int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	float BestScore = 0.f;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const float Score = ScoreCounterOption(Candidates[Index], Context, Params);
		if (Score > BestScore)
		{
			BestScore = Score;
			OutIndex = Index;
		}
	}
	return BestScore;
}

bool FMKHStateScoring::IsInterruptedBeforeHit(const FMKHAttackCandidate& Candidate, const FMKHStateScoringContext& Context)
{
	return Context.IncomingCrowdControl != EMKHCrowdControl::None && Candidate.TimeToHit >= Context.IncomingSecondsToHit;
}
