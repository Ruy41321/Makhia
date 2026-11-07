// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/MKHLoggedFloatInputConsideration.h"

#include "AI/MKHAttackScoring.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

// ============================================================
// Protected / Internal Logic
// ============================================================

float FMKHLoggedFloatInputConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const float NormalizedInput = FMath::Clamp(Data.Interval.GetRangePct(Data.Input), 0.f, 1.f);
	const float CurveScore = EvaluateResponseCurve(NormalizedInput);
	const float Sharpness = Data.bApplyExpertise ? FMKHDuelBotTuning::ComputeDecisionSharpness(Data.Expertise) : 1.f;
	const float Score = FMKHAttackScoring::ApplySharpness(CurveScore, Sharpness);

	// Logged on the pawn when there is one, so the line sits on the bot's row of the timeline, with its position.
	const UObject* Owner = Context.GetOwner();
	const AController* Controller = Cast<AController>(Owner);
	const UObject* LogOwner = IsValid(Controller) && IsValid(Controller->GetPawn()) ? Controller->GetPawn() : Owner;
	UE_VLOG(LogOwner, LogMKHAI, Verbose, TEXT("Utility %s: input %.2f -> %.2f, curve %.2f, ^%.1f -> score %.2f"),
		Label.IsNone() ? TEXT("Float") : *Label.ToString(), Data.Input, NormalizedInput, CurveScore, Sharpness, Score);
	return Score;
}

float FMKHLoggedFloatInputConsideration::EvaluateResponseCurve(const float NormalizedInput) const
{
	const FRichCurve* Curve = ResponseCurve.GetRichCurveConst();
	if (Curve == nullptr || Curve->IsEmpty())
	{
		return NormalizedInput;
	}
	return Curve->Eval(NormalizedInput);
}
