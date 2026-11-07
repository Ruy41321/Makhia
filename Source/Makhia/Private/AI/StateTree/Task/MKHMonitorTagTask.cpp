// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHMonitorTagTask.h"

#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHMonitorTagTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.TagChangedHandle.Reset();

	UAbilitySystemComponent* Asc = GetMonitoredAbilitySystem(Data);
	if (!IsValid(Asc))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHMonitorTagTask: no character or ability system to monitor (%s)."),
			*GetNameSafe(Data.CharToMonitor));
		return EStateTreeRunStatus::Failed;
	}
	if (!Data.TagToMonitor.IsValid())
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHMonitorTagTask: invalid tag on %s."), *GetNameSafe(Data.CharToMonitor));
		return EStateTreeRunStatus::Failed;
	}

	// The callback outlives this call: capture the weak context and a copy of the dispatcher, never Context or Data
	// (the context lives on the stack of this call, the instance data may move).
	Data.TagChangedHandle = Asc->RegisterGameplayTagEvent(Data.TagToMonitor, EGameplayTagEventType::NewOrRemoved).AddLambda(
		[WeakContext = Context.MakeWeakExecutionContext(), Dispatcher = Data.OnTagChangedDelegate](const FGameplayTag, int32)
		{
			WeakContext.BroadcastDelegate(Dispatcher);
		});

	return EStateTreeRunStatus::Running;
}

void FMKHMonitorTagTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	UAbilitySystemComponent* Asc = GetMonitoredAbilitySystem(Data);
	if (IsValid(Asc) && Data.TagChangedHandle.IsValid())
	{
		Asc->RegisterGameplayTagEvent(Data.TagToMonitor, EGameplayTagEventType::NewOrRemoved).Remove(Data.TagChangedHandle);
	}
	Data.TagChangedHandle.Reset();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

UAbilitySystemComponent* FMKHMonitorTagTask::GetMonitoredAbilitySystem(const FMKHMonitorTagTaskInstanceData& Data)
{
	return IsValid(Data.CharToMonitor) ? Data.CharToMonitor->GetAbilitySystemComponent() : nullptr;
}
