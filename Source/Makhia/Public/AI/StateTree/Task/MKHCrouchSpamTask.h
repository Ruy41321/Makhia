// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHCrouchSpamTask.generated.h"

class AAIController;
class UCrouchSystemComponent;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHCrouchSpamTask: the bot's controller, the press rhythm and the runtime timers.
 */
USTRUCT()
struct FMKHCrouchSpamTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Seconds between two crouch presses, rolled in this range before each press so the rhythm is not mechanical. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.05f, Units = "s"))
	FFloatInterval ToggleIntervalRange = FFloatInterval(0.2f, 0.35f);

	/** Seconds after which the bot stands up and the task succeeds; 0 spams until the state is left. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, Units = "s"))
	float MaxDuration = 4.f;

	/** Seconds since the state was entered. */
	float ElapsedTime = 0.f;

	/** Seconds left before the next press. */
	float TimeToNextPress = 0.f;
};

/**
 * StateTree task of the duel bot: taunts a defeated opponent by pressing crouch again and again, through
 * UCrouchSystemComponent::CrouchPressed, the entry point of the player's crouch key (same gates: crowd control, dodge,
 * movement state). Running while it spams, Succeeded after MaxDuration (standing), Failed without a crouch component.
 * Leaving the state always stands the bot up (ForceUncrouch, which respects the headroom).
 *
 * Tree layout: see Documentation/MatchFlowArchitecture.md (Bot AI).
 */
USTRUCT(meta = (DisplayName = "MKH Crouch Spam", Category = "MKH"))
struct MAKHIA_API FMKHCrouchSpamTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHCrouchSpamTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Presses crouch at once and rolls the delay of the next press.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running, or Failed when the bot has no crouch component.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/**
	 * Presses crouch again each time the rolled delay runs out, until MaxDuration.
	 *
	 * @param Context    Execution context of the running StateTree.
	 * @param DeltaTime  Seconds since the last tick.
	 * @return Running while spamming, Succeeded after MaxDuration, Failed without a crouch component.
	 */
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Stands the bot up, so it never stays crouched after the taunt (round reset, match restart).
	 *
	 * @param Context     Execution context of the running StateTree.
	 * @param Transition  Transition leaving the state (unused).
	 */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Gets the crouch component of the pawn driven by a controller.
	 *
	 * @param AIController  Controller of the bot.
	 * @return The pawn's UCrouchSystemComponent, or null.
	 */
	static UCrouchSystemComponent* GetPawnCrouchSystem(const AAIController* AIController);

	/**
	 * Presses the crouch key once, as the player would, and rolls the delay of the next press.
	 *
	 * @param CrouchSystem  Bot's crouch component.
	 * @param Data          Instance data: TimeToNextPress is rolled in ToggleIntervalRange.
	 */
	static void PressCrouch(UCrouchSystemComponent& CrouchSystem, FMKHCrouchSpamTaskInstanceData& Data);
};
