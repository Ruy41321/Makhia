// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "MKHCanActivateAbilityCondition.generated.h"

class AAIController;
struct FStateTreeExecutionContext;

/**
 * Instance data of FMKHCanActivateAbilityCondition: the bot's controller and the input tag of the ability to check.
 */
USTRUCT()
struct FMKHCanActivateAbilityConditionInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Input tag the ability is bound to, as for the player's key (e.g. Input.Ability.Attacks.Basics.Heavy). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "Input.Ability"))
	FGameplayTag InputTag;
};

/**
 * StateTree condition of the duel bot: true when the ability bound to InputTag could be activated now (granted,
 * off cooldown, cost affordable, not blocked), with the same rules as the player's key. Used as an enter condition,
 * it is a hard veto: utility selection then falls back to the next best state. For a quick slot input
 * (Input.Ability.QuickSlot.*) it asks AMKHDuelBot::CanUseQuickSlot: the slot must also hold an item (weapon not
 * already in hand, consumable off cooldown), since GA_QuickSlot alone cannot tell.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Can Activate Ability", Category = "MKH"))
struct MAKHIA_API FMKHCanActivateAbilityCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	/** Type of the per-node data this condition reads. */
	using FInstanceDataType = FMKHCanActivateAbilityConditionInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Checks whether the ability bound to InputTag can be activated on the bot's ability system.
	 *
	 * @param Context  Execution context of the running StateTree, gives access to the instance data.
	 * @return True when an ability is granted for the tag and CanActivateAbility passes.
	 */
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
