// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeAIComponent.h"
#include "MKHDuelBotStateTreeComponent.generated.h"

/**
 * StateTree component of the duel bot (AMKHDuelBotController): the engine's AI component plus an evaluators-only
 * tick, so that while the bot is paused (MKH.Bot.Pause) its evaluators keep reading the duel (scores, debug dumps)
 * while no task runs and no transition fires.
 *
 * Full model: see Documentation/DuelBotAI.md (Debugging).
 */
UCLASS(ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHDuelBotStateTreeComponent : public UStateTreeAIComponent
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

public:
	/**
	 * Ticks the global evaluators only (no tasks, no transitions). Does nothing while the tree is not running.
	 *
	 * @param DeltaTime  Seconds since the last tick.
	 */
	void TickEvaluatorsOnly(float DeltaTime);
};
