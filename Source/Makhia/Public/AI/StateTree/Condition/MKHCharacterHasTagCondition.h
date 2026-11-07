// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "MKHCharacterHasTagCondition.generated.h"

class ACharacterBase;
struct FStateTreeExecutionContext;

/**
 * Instance data of FMKHCharacterHasTagCondition: the character to check and the gameplay tag to look for.
 */
USTRUCT()
struct FMKHCharacterHasTagConditionInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Character whose ability system is checked (e.g. the bot itself, or the opponent bound from an evaluator). */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<ACharacterBase> Character = nullptr;

	/** Gameplay tag looked for among the tags owned by the character's ability system. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag Tag;

	/** True to match only Tag itself; false also matches its children (State.Movement matches State.Movement.Dead). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bExactMatch = false;

	/** True to pass when the character does NOT own Tag. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/**
 * StateTree condition of the duel bot: true when the ability system of Character owns Tag, exactly or as a parent of
 * an owned tag (bExactMatch); bInvert turns it into "does not own Tag". False without a valid tag, character or ability
 * system, whatever bInvert.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Character Has Tag", Category = "MKH"))
struct MAKHIA_API FMKHCharacterHasTagCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	/** Type of the per-node data this condition reads. */
	using FInstanceDataType = FMKHCharacterHasTagConditionInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Looks for Tag among the tags owned by the character's ability system.
	 *
	 * @param Context  Execution context of the running StateTree, gives access to the instance data.
	 * @return True when the tag is owned (exactly, or through a child tag when bExactMatch is false), or when it is not
	 *         owned and bInvert is set.
	 */
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
