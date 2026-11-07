// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MKHMonitorTagTask.generated.h"

class ACharacterBase;
class AAIController;
class UAbilitySystemComponent;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

/**
 * Instance data of FMKHMonitorTagTask: the character and tag to watch, the dispatcher fired on changes and the handle
 * of the ability system callback.
 */
USTRUCT()
struct FMKHMonitorTagTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Character whose ability system is monitored for the specified tag. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<ACharacterBase> CharToMonitor = nullptr;
	
	/** Tag to monitor on the specified ACharacterBase ability system. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag TagToMonitor;
		
	/** Broadcast when TagToMonitor is added to or removed from CharToMonitor; bind a transition (On Delegate) to it. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnTagChangedDelegate;

	/** Handle of the ability system callback, removed when the state is left. */
	FDelegateHandle TagChangedHandle;
};

/**
 * StateTree task of the duel bot: listens to TagToMonitor on CharToMonitor's ability system and broadcasts
 * OnTagChangedDelegate when the tag is added or removed (count 0 <-> >0). Never finishes on its own: Running until the
 * state is left, Failed without character, ability system or valid tag. Fires only on changes, not on the state of the
 * tag at entry. Leaving the state removes the callback.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Monitor Tag", Category = "MKH"))
struct MAKHIA_API FMKHMonitorTagTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHMonitorTagTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Registers the tag callback on the monitored ability system.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running, or Failed without character, ability system or valid tag.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/**
	 * Removes the tag callback, whatever the reason the state is left.
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
	 * @param Data  Instance data with the character to monitor.
	 * @return The ability system of CharToMonitor, or null.
	 */
	static UAbilitySystemComponent* GetMonitoredAbilitySystem(const FMKHMonitorTagTaskInstanceData& Data);
};
