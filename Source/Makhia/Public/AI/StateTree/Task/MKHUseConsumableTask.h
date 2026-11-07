// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "MKHUseConsumableTask.generated.h"

class AAIController;
class AMKHDuelBot;
class UMKHAbilitySystemComponent;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHUseConsumableTask: the bot's controller, the consumable to use and the runtime state of the
 * quick slot use.
 */
USTRUCT()
struct FMKHUseConsumableTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Consumable to use, children included (e.g. Item.Consumable.HealthPotion). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "Item.Consumable"))
	FGameplayTag ItemTag;

	/** Seconds to wait for GA_QuickSlot to start after the press before failing. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.f, Units = "s"))
	float StartTimeout = 0.5f;

	/** Seconds after which the task fails even if the use is still running (safety net). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.1f, Units = "s"))
	float MaxDuration = 5.f;

	/** Consumable quick slot (input tag) pressed in EnterState. */
	FGameplayTag QuickSlotTag;

	/** Seconds since the press. */
	float ElapsedTime = 0.f;

	/** Spec of GA_QuickSlot seen active after the press; invalid until it starts. */
	FGameplayAbilitySpecHandle ActiveSpecHandle;
};

/**
 * StateTree task of the duel bot: uses a consumable as the player does, by pressing the consumable quick slot that
 * holds ItemTag (GA_QuickSlot: animation, cooldown, quantity). Only quick slotted consumables can be used, as for the
 * player; the default loadout quick slots them. Running while GA_QuickSlot runs, Succeeded when it ends, Failed when no
 * quick slot holds the item, it is on cooldown or the use is blocked (AMKHDuelBot::CanUseQuickSlot), when nothing
 * starts within StartTimeout or after MaxDuration. Leaving the state early cancels a use still running.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Use Consumable", Category = "MKH"))
struct MAKHIA_API FMKHUseConsumableTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHUseConsumableTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Finds the quick slot holding ItemTag and presses it if the use is allowed now.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running while waiting for GA_QuickSlot, Failed when the item cannot be used now.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/**
	 * Follows GA_QuickSlot started by the press.
	 *
	 * @param Context    Execution context of the running StateTree.
	 * @param DeltaTime  Seconds since the last tick.
	 * @return Running while the use runs, Succeeded when it ended, Failed on timeout.
	 */
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Cancels GA_QuickSlot if the state is left while the use still runs.
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
	 * Gets the bot driven by a controller.
	 *
	 * @param AIController  Controller of the bot.
	 * @return The controlled AMKHDuelBot, or null.
	 */
	static AMKHDuelBot* GetControlledBot(const AAIController* AIController);

	/**
	 * Gets the ability system of the bot driven by a controller.
	 *
	 * @param AIController  Controller of the bot.
	 * @return The bot's UMKHAbilitySystemComponent, or null.
	 */
	static UMKHAbilitySystemComponent* GetBotAbilitySystem(const AAIController* AIController);

	/**
	 * Finds the active GA_QuickSlot spec a quick slot input drives.
	 *
	 * @param AbilitySystem  Bot's ability system.
	 * @param QuickSlotTag   Quick slot input pressed.
	 * @return Handle of the active spec, invalid when none is running.
	 */
	static FGameplayAbilitySpecHandle FindActiveSpec(const UMKHAbilitySystemComponent& AbilitySystem, const FGameplayTag& QuickSlotTag);
};
