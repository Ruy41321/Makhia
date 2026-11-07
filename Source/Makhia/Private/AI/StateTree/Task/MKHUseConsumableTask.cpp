// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHUseConsumableTask.h"

#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AI/MKHDuelBot.h"
#include "AIController.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHUseConsumableTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime = 0.f;
	Data.ActiveSpecHandle = FGameplayAbilitySpecHandle();
	Data.QuickSlotTag = FGameplayTag();

	const AMKHDuelBot* Bot = GetControlledBot(Data.AIController);
	UMKHAbilitySystemComponent* AbilitySystem = GetBotAbilitySystem(Data.AIController);
	if (!Data.ItemTag.IsValid() || !IsValid(Bot) || !IsValid(AbilitySystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHUseConsumableTask: missing item tag, bot or ability system on %s."), *GetNameSafe(Bot));
		return EStateTreeRunStatus::Failed;
	}

	// GA_QuickSlot checks the cooldown only after activating: CanUseQuickSlot tells beforehand, with the player's rules.
	Data.QuickSlotTag = Bot->FindConsumableQuickSlot(Data.ItemTag);
	if (!Data.QuickSlotTag.IsValid() || !Bot->CanUseQuickSlot(Data.QuickSlotTag))
	{
		UE_LOG(LogMKHAI, Log, TEXT("MKHUseConsumableTask: %s cannot use %s now (%s)."), *Bot->GetName(), *Data.ItemTag.ToString(),
			Data.QuickSlotTag.IsValid() ? TEXT("cooldown or blocked") : TEXT("not quick slotted"));
		return EStateTreeRunStatus::Failed;
	}

	// The player's key: a tap on the quick slot input.
	AbilitySystem->AbilityInputPressed(Data.QuickSlotTag, false);
	AbilitySystem->AbilityInputReleased(Data.QuickSlotTag);
	Data.ActiveSpecHandle = FindActiveSpec(*AbilitySystem, Data.QuickSlotTag);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMKHUseConsumableTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime += DeltaTime;

	const UMKHAbilitySystemComponent* AbilitySystem = GetBotAbilitySystem(Data.AIController);
	if (!IsValid(AbilitySystem))
	{
		return EStateTreeRunStatus::Failed;
	}

	const FGameplayAbilitySpecHandle ActiveHandle = FindActiveSpec(*AbilitySystem, Data.QuickSlotTag);
	if (!Data.ActiveSpecHandle.IsValid())
	{
		// Not started yet: it may still come out of the ability queue.
		Data.ActiveSpecHandle = ActiveHandle;
		if (ActiveHandle.IsValid() || Data.ElapsedTime < Data.StartTimeout)
		{
			return EStateTreeRunStatus::Running;
		}

		UE_LOG(LogMKHAI, Log, TEXT("MKHUseConsumableTask: GA_QuickSlot did not start for %s."), *Data.ItemTag.ToString());
		return EStateTreeRunStatus::Failed;
	}

	if (!ActiveHandle.IsValid())
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return Data.ElapsedTime < Data.MaxDuration ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void FMKHUseConsumableTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UMKHAbilitySystemComponent* AbilitySystem = GetBotAbilitySystem(Data.AIController);
	if (!IsValid(AbilitySystem) || !Data.ActiveSpecHandle.IsValid())
	{
		return;
	}

	// Cleanup only: a use interrupted by death or a higher priority state must not keep running.
	const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Data.ActiveSpecHandle);
	if (Spec != nullptr && Spec->IsActive())
	{
		AbilitySystem->CancelAbilityHandle(Data.ActiveSpecHandle);
	}
	Data.ActiveSpecHandle = FGameplayAbilitySpecHandle();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

AMKHDuelBot* FMKHUseConsumableTask::GetControlledBot(const AAIController* AIController)
{
	return IsValid(AIController) ? Cast<AMKHDuelBot>(AIController->GetPawn()) : nullptr;
}

UMKHAbilitySystemComponent* FMKHUseConsumableTask::GetBotAbilitySystem(const AAIController* AIController)
{
	APawn* Pawn = IsValid(AIController) ? AIController->GetPawn() : nullptr;
	return Cast<UMKHAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn));
}

FGameplayAbilitySpecHandle FMKHUseConsumableTask::FindActiveSpec(const UMKHAbilitySystemComponent& AbilitySystem, const FGameplayTag& QuickSlotTag)
{
	const FGameplayAbilitySpec* Spec = AbilitySystem.FindAbilitySpecForInputTag(QuickSlotTag);
	return Spec != nullptr && Spec->IsActive() ? Spec->Handle : FGameplayAbilitySpecHandle();
}
