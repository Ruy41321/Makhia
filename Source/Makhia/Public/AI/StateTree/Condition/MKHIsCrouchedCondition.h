// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "MKHIsCrouchedCondition.generated.h"

class AAIController;
struct FStateTreeExecutionContext;

/**
 * Instance data of FMKHIsCrouchedCondition: the bot's controller and the stance to check.
 */
USTRUCT()
struct FMKHIsCrouchedConditionInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** True to check that the pawn is crouched, false to check that it is standing. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bCrouched = true;
};

/**
 * StateTree condition of the duel bot: true when the controlled pawn's stance (UCrouchSystemComponent::IsCrouched,
 * the same state MKH Set Crouch reads) matches bCrouched. False without a pawn or a crouch component, whatever bCrouched.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Is Crouched", Category = "MKH"))
struct MAKHIA_API FMKHIsCrouchedCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	/** Type of the per-node data this condition reads. */
	using FInstanceDataType = FMKHIsCrouchedConditionInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Compares the controlled pawn's stance with bCrouched.
	 *
	 * @param Context  Execution context of the running StateTree, gives access to the instance data.
	 * @return True when the pawn is crouched and bCrouched is true, or standing and bCrouched is false.
	 */
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
