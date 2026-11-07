// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHTurnRelativeToOpponentTask.generated.h"

class AAIController;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHTurnRelativeToOpponentTask: the bot's controller, the facing to reach relative to the opponent,
 * how fast the bot turns and how long it holds the facing.
 */
USTRUCT()
struct FMKHTurnRelativeToOpponentTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/**
	 * Facing to reach, in degrees off "looking at the opponent": 0 face to face, 180 back to the opponent (bait), 90 or
	 * -90 side on. Positive turns clockwise seen from above, so 90 leaves the opponent on the bot's left.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = -180.f, ClampMax = 180.f, Units = "deg"))
	float AngleFromOpponent = 180.f;

	/** Degrees per second the bot turns; 360 matches the RotationRate of BP_DuelBot. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 1.f, Units = "deg"))
	float TurnRate = 360.f;

	/** Degrees off the asked facing still counted as reached. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, ClampMax = 90.f, Units = "deg"))
	float AngleTolerance = 10.f;

	/** Seconds the facing is held once reached, then the task succeeds; 0 holds it until the state is left. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, Units = "s"))
	float HoldDuration = 1.5f;

	/** Seconds since the facing was first reached; negative while still turning. */
	float HeldTime = -1.f;
};

/**
 * StateTree task of the duel bot: stops the bot and turns its body to a facing relative to the opponent
 * (AngleFromOpponent: 0 face to face, 180 back turned to bait an attack, 90 side on), following the opponent while it
 * moves around. The control rotation (focus on the opponent, used by the abilities to aim) is not touched, so a
 * reaction state entered meanwhile still aims at the opponent. Running while turning and holding, Succeeded after
 * HoldDuration, Failed without bot or opponent.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Turn Relative To Opponent", Category = "MKH"))
struct MAKHIA_API FMKHTurnRelativeToOpponentTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHTurnRelativeToOpponentTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Stops the path following (moving would turn the body towards the movement) and starts turning.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running, or Failed without bot or opponent.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/**
	 * Turns the body towards the asked facing and counts the time it is held.
	 *
	 * @param Context    Execution context of the running StateTree.
	 * @param DeltaTime  Seconds since the last tick.
	 * @return Running while turning or holding, Succeeded after HoldDuration, Failed without bot or opponent.
	 */
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Gets the bot's pawn and its opponent.
	 *
	 * @param AIController  Controller of the bot, an AMKHDuelBotController.
	 * @param OutBot        The controlled pawn, or null.
	 * @param OutOpponent   The opponent, or null.
	 * @return True when both are valid.
	 */
	static bool GetBotAndOpponent(const AAIController* AIController, APawn*& OutBot, const AActor*& OutOpponent);

	/**
	 * Yaw of a body at an angle off "looking at the opponent", on the ground plane.
	 *
	 * @param BotLocation        Bot's location.
	 * @param OpponentLocation   Opponent's location.
	 * @param AngleFromOpponent  Degrees added to the yaw looking at the opponent.
	 * @param FallbackYaw        Yaw returned when the two locations overlap on the ground plane.
	 * @return World yaw in degrees.
	 */
	static float GetTargetYaw(const FVector& BotLocation, const FVector& OpponentLocation, float AngleFromOpponent, float FallbackYaw);
};
