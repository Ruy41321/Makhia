// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHSetCrouchTask.generated.h"

class AAIController;
class UCrouchSystemComponent;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHSetCrouchTask: the bot's controller and whether it must end up crouched or standing.
 */
USTRUCT()
struct FMKHSetCrouchTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** True to crouch, false to stand up. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bCrouch = true;
};

/**
 * StateTree task of the duel bot: crouches or stands up once, through UCrouchSystemComponent::CrouchPressed, the
 * entry point of the player's crouch key (same gates: crowd control, dodge, movement state, headroom to stand up), then
 * finishes at once. The key is pressed only when the bot is not already in the asked stance. Succeeded when the bot is
 * in the asked stance, Failed when a gate refused it or the bot has no crouch component. Standing up without headroom
 * steps the bot to the nearest free spot (UCrouchSystemComponent::TryStandUpWithNudge). Leaving the state does not
 * restore the previous stance.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Set Crouch", Category = "MKH"))
struct MAKHIA_API FMKHSetCrouchTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHSetCrouchTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Presses crouch if the bot is not already in the asked stance.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Succeeded when the bot is in the asked stance, Failed otherwise.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

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
};
