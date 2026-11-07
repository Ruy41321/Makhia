// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "StateTreeConsiderationBase.h"
#include "MKHLoggedFloatInputConsideration.generated.h"

struct FStateTreeExecutionContext;

/**
 * Instance data of FMKHLoggedFloatInputConsideration: the bound input value and the range that maps it to 0-1.
 */
USTRUCT()
struct FMKHLoggedFloatInputConsiderationInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Value to score, usually bound to an output of the MKH Duel Opponent evaluator (e.g. Distance). */
	UPROPERTY(EditAnywhere, Category = "Input")
	float Input = 0.f;

	/** Input range normalized to 0-1 before the curve: values outside are clamped to its ends. */
	UPROPERTY(EditAnywhere, Category = "Parameter", DisplayName = "Input Range")
	FFloatInterval Interval = FFloatInterval(0.f, 1.f);

	/** The bot's Expertise 0-1; bind it to Context Tuning.Expertise where bApplyExpertise is set (optional binding). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Expertise = 1.f;

	/**
	 * Raises the score to k = Expertise x MaxDecisionSharpness (FMKHDuelBotTuning::GetDecisionSharpness): set it on
	 * the considerations of the states of a weighted random choice, so the choice is as sharp as the bot's expertise.
	 * The states' Weight stays 1.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bApplyExpertise = false;
};

/**
 * Utility consideration of the duel bot: same scoring as the engine's Float Input consideration (input normalized
 * over a range, then shaped by a response curve), optionally sharpened by the bot's expertise (score^k), plus a
 * Visual Logger line per evaluation with its label, input and score, so the bot's choices can be read on the Visual
 * Logger timeline next to its position.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Logged Float Input", Category = "MKH"))
struct MAKHIA_API FMKHLoggedFloatInputConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	/** Type of the per-node data this consideration reads. */
	using FInstanceDataType = FMKHLoggedFloatInputConsiderationInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Normalizes the input over the range, applies the response curve and the expertise sharpness and logs the result
	 * to the Visual Logger.
	 *
	 * @param Context  Execution context of the running StateTree, gives access to the instance data and the owner.
	 * @return Score in 0-1 (the base class clamps it).
	 */
	virtual float GetScore(FStateTreeExecutionContext& Context) const override;

	/**
	 * Applies the response curve to a normalized input.
	 *
	 * @param NormalizedInput  Input mapped to 0-1.
	 * @return The curve's value, or the input itself when the curve is empty (linear response).
	 */
	float EvaluateResponseCurve(const float NormalizedInput) const;

	// ============================================================
	// Properties
	// ============================================================

public:
	/** Name shown in the Visual Logger line (e.g. "Approach.Distance"), to tell the considerations apart. */
	UPROPERTY(EditAnywhere, Category = "Default")
	FName Label;

	/** Response curve over the normalized input, X and Y in 0-1; empty means linear. */
	UPROPERTY(EditAnywhere, Category = "Default", DisplayName = "Curve")
	FRuntimeFloatCurve ResponseCurve;
};
