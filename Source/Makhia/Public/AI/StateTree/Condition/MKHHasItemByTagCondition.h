// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "MKHHasItemByTagCondition.generated.h"

class AAIController;
struct FStateTreeExecutionContext;

/**
 * Instance data of FMKHHasItemByTagCondition: the bot's controller and the kind of item to look for.
 */
USTRUCT()
struct FMKHHasItemByTagConditionInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Item looked for, children included (Item.Consumable matches any consumable, e.g. Item.Consumable.HealthPotion). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "Item"))
	FGameplayTag ItemTag;
};

/**
 * StateTree condition of the duel bot: true when the inventory of the controlled pawn holds at least one item matching
 * ItemTag, quick slotted or not. It does not tell whether the item can be used now (cooldown, quick slot): MKH Use
 * Consumable checks that. False without a valid tag, pawn or inventory.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Has Item By Tag", Category = "MKH"))
struct MAKHIA_API FMKHHasItemByTagCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	/** Type of the per-node data this condition reads. */
	using FInstanceDataType = FMKHHasItemByTagConditionInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Looks for an inventory entry whose item matches ItemTag.
	 *
	 * @param Context  Execution context of the running StateTree, gives access to the instance data.
	 * @return True when the controlled pawn owns such an item.
	 */
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
