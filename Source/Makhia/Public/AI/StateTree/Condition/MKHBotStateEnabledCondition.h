// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "MKHBotStateEnabledCondition.generated.h"

class AAIController;
struct FStateTreeExecutionContext;

/**
 * Instance data of FMKHBotStateEnabledCondition: the bot's controller and the state's tag.
 */
USTRUCT()
struct FMKHBotStateEnabledConditionInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** AI.State.* tag of the state this condition guards (e.g. AI.State.Attack). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "AI.State"))
	FGameplayTag StateTag;
};

/**
 * StateTree condition of the duel bot, an enter condition of every selectable state (Combat children, React and
 * its children): false while the state is disabled with MKH.Bot.State.Disable / Only, so a test can run with only the
 * states it needs. True without a duel bot controller (nothing can disable a state then).
 *
 * Full model: see Documentation/DuelBotAI.md (Debugging).
 */
USTRUCT(meta = (DisplayName = "MKH Bot State Enabled", Category = "MKH"))
struct MAKHIA_API FMKHBotStateEnabledCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	/** Type of the per-node data this condition reads. */
	using FInstanceDataType = FMKHBotStateEnabledConditionInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Asks the controller whether the state is enabled.
	 *
	 * @param Context  Execution context of the running StateTree, gives access to the instance data.
	 * @return False when StateTag is disabled for the session.
	 */
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
