// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Condition/MKHBotStateEnabledCondition.h"

#include "AI/Controller/MKHDuelBotController.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

bool FMKHBotStateEnabledCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController);
	return !IsValid(BotController) || BotController->IsStateEnabled(Data.StateTag);
}
