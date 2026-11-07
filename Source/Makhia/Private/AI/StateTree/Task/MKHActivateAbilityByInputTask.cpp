// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Task/MKHActivateAbilityByInputTask.h"

#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

// ============================================================
// Public Interface
// ============================================================

EStateTreeRunStatus FMKHActivateAbilityByInputTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime = 0.f;
	Data.ActiveSpecHandle = FGameplayAbilitySpecHandle();

	Data.PressedInputTag = Data.InputTag;
	Data.PressedSwapInputTag = Data.SwapInputTag;
	Data.bSwapPending = false;

	UMKHAbilitySystemComponent* AbilitySystem = GetPawnAbilitySystem(Data.AIController);
	if (!Data.PressedInputTag.IsValid() || !IsValid(AbilitySystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHActivateAbilityByInputTask: missing input tag or ability system on %s."),
			*GetNameSafe(IsValid(Data.AIController) ? Data.AIController->GetPawn() : nullptr));
		return EStateTreeRunStatus::Failed;
	}

	Data.bAttackEndPending = ShouldWaitForAttackEnd(*AbilitySystem, Data);
	if (!Data.bAttackEndPending)
	{
		StartInputs(Context, *AbilitySystem);
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMKHActivateAbilityByInputTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime += DeltaTime;

	UMKHAbilitySystemComponent* AbilitySystem = GetPawnAbilitySystem(Data.AIController);
	if (!IsValid(AbilitySystem))
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.bAttackEndPending)
	{
		if (ShouldWaitForAttackEnd(*AbilitySystem, Data) && Data.ElapsedTime < Data.StartTimeout)
		{
			return EStateTreeRunStatus::Running;
		}
		Data.bAttackEndPending = false;
		Data.ElapsedTime = 0.f;
		StartInputs(Context, *AbilitySystem);
		return EStateTreeRunStatus::Running;
	}
	if (Data.bSwapPending)
	{
		return TickPendingSwap(Context, *AbilitySystem);
	}

	if (Data.bInputHeld && Data.ElapsedTime >= Data.HoldDuration)
	{
		ReleaseHeldInput(*AbilitySystem, Data);
	}

	const FGameplayAbilitySpecHandle ActiveHandle = FindActiveSpecForInputTag(*AbilitySystem, Data.PressedInputTag);
	if (!Data.ActiveSpecHandle.IsValid())
	{
		// Not started yet: it may still come out of the ability queue.
		Data.ActiveSpecHandle = ActiveHandle;
		if (ActiveHandle.IsValid() || Data.ElapsedTime < Data.StartTimeout)
		{
			return EStateTreeRunStatus::Running;
		}

		UE_LOG(LogMKHAI, Log, TEXT("MKHActivateAbilityByInputTask: no ability started for %s (granted: %s)."),
			*Data.PressedInputTag.ToString(), AbilitySystem->FindAbilitySpecForInputTag(Data.PressedInputTag) != nullptr ? TEXT("yes") : TEXT("no"));
		return EStateTreeRunStatus::Failed;
	}

	if (!ActiveHandle.IsValid())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (Data.bComboWindowOpen)
	{
		// One strike per decision: the tree re-scores; without a new press in the window the strike ends the attack.
		UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Combo window open (%s): back to Combat."), *Data.PressedInputTag.ToString());
		Data.ActiveSpecHandle = FGameplayAbilitySpecHandle();
		return EStateTreeRunStatus::Succeeded;
	}
	return Data.ElapsedTime < Data.MaxDuration ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void FMKHActivateAbilityByInputTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UnbindComboWindowEvents(Data);

	UMKHAbilitySystemComponent* AbilitySystem = GetPawnAbilitySystem(Data.AIController);
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	// A key still held when the state is left (death, reaction) is released, as the player would lift the finger.
	if (Data.bInputHeld)
	{
		ReleaseHeldInput(*AbilitySystem, Data);
	}
	if (!Data.ActiveSpecHandle.IsValid())
	{
		return;
	}

	// A strike holding its priority tag can only be interrupted as the player's: see DuelBotAI.md, Reactions and priority.
	const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Data.ActiveSpecHandle);
	if (Spec != nullptr && Spec->IsActive() && CanCancelOnExit(*AbilitySystem, Data))
	{
		AbilitySystem->CancelAbilityHandle(Data.ActiveSpecHandle);
	}
	Data.ActiveSpecHandle = FGameplayAbilitySpecHandle();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

UMKHAbilitySystemComponent* FMKHActivateAbilityByInputTask::GetPawnAbilitySystem(const AAIController* AIController)
{
	APawn* Pawn = IsValid(AIController) ? AIController->GetPawn() : nullptr;
	return Cast<UMKHAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn));
}

void FMKHActivateAbilityByInputTask::ReleaseHeldInput(UMKHAbilitySystemComponent& AbilitySystem, FMKHActivateAbilityByInputTaskInstanceData& Data)
{
	AbilitySystem.AbilityInputReleased(Data.PressedInputTag);
	Data.bInputHeld = false;
}

void FMKHActivateAbilityByInputTask::PressInput(FStateTreeExecutionContext& Context, UMKHAbilitySystemComponent& AbilitySystem) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	// Combo windows are sent by the montage's anim notifies; abilities without combos never send them.
	BindComboWindowEvents(Context, AbilitySystem);

	// The player's key: press, then release at once (tap) or after HoldDuration in Tick, through the same queueing rules.
	AbilitySystem.AbilityInputPressed(Data.PressedInputTag, false);
	Data.bInputHeld = true;
	if (Data.HoldDuration <= 0.f)
	{
		ReleaseHeldInput(AbilitySystem, Data);
	}
	Data.ActiveSpecHandle = FindActiveSpecForInputTag(AbilitySystem, Data.PressedInputTag);
}

EStateTreeRunStatus FMKHActivateAbilityByInputTask::TickPendingSwap(FStateTreeExecutionContext& Context, UMKHAbilitySystemComponent& AbilitySystem) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	const bool bSwapRunning = AbilitySystem.HasMatchingGameplayTag(MKHGameplayTags::State::QuickSlotUse);
	if (bSwapRunning && Data.ElapsedTime < Data.StartTimeout)
	{
		return EStateTreeRunStatus::Running;
	}

	// The swap grants the weapon's abilities: the attack input now reaches the new weapon, as a second key press would.
	Data.bSwapPending = false;
	Data.ElapsedTime = 0.f;
	PressInput(Context, AbilitySystem);
	return EStateTreeRunStatus::Running;
}

FGameplayAbilitySpecHandle FMKHActivateAbilityByInputTask::FindActiveSpecForInputTag(const UMKHAbilitySystemComponent& AbilitySystem, const FGameplayTag& InputTag)
{
	// Quick slot inputs resolve to GA_QuickSlot's spec, as when the player presses them.
	const FGameplayAbilitySpec* Spec = AbilitySystem.FindAbilitySpecForInputTag(InputTag);
	return Spec != nullptr && Spec->IsActive() ? Spec->Handle : FGameplayAbilitySpecHandle();
}

void FMKHActivateAbilityByInputTask::BindComboWindowEvents(FStateTreeExecutionContext& Context, UMKHAbilitySystemComponent& AbilitySystem) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UnbindComboWindowEvents(Data);

	// The instance data may move in memory: the callbacks reach it through a struct ref, never a raw pointer.
	const TStateTreeInstanceDataStructRef<FInstanceDataType> DataRef = Context.GetInstanceDataStructRef(*this);
	Data.ComboEventsAbilitySystem = &AbilitySystem;
	Data.ComboStartHandle = AbilitySystem.GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::ContinueComboStart)
		.AddLambda([DataRef](const FGameplayEventData*) { HandleComboWindowEvent(DataRef, true); });
	Data.ComboEndHandle = AbilitySystem.GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::ContinueComboEnd)
		.AddLambda([DataRef](const FGameplayEventData*) { HandleComboWindowEvent(DataRef, false); });
}

void FMKHActivateAbilityByInputTask::UnbindComboWindowEvents(FMKHActivateAbilityByInputTaskInstanceData& Data)
{
	if (UMKHAbilitySystemComponent* AbilitySystem = Data.ComboEventsAbilitySystem.Get(); IsValid(AbilitySystem))
	{
		if (FGameplayEventMulticastDelegate* StartDelegate = AbilitySystem->GenericGameplayEventCallbacks.Find(MKHGameplayTags::Event::ContinueComboStart))
		{
			StartDelegate->Remove(Data.ComboStartHandle);
		}
		if (FGameplayEventMulticastDelegate* EndDelegate = AbilitySystem->GenericGameplayEventCallbacks.Find(MKHGameplayTags::Event::ContinueComboEnd))
		{
			EndDelegate->Remove(Data.ComboEndHandle);
		}
	}

	Data.ComboEventsAbilitySystem.Reset();
	Data.ComboStartHandle.Reset();
	Data.ComboEndHandle.Reset();
	Data.bComboWindowOpen = false;
}

void FMKHActivateAbilityByInputTask::HandleComboWindowEvent(const TStateTreeInstanceDataStructRef<FMKHActivateAbilityByInputTaskInstanceData>& DataRef, const bool bOpened)
{
	FInstanceDataType* Data = DataRef.GetPtr();
	if (Data == nullptr)
	{
		return;
	}

	Data->bComboWindowOpen = bOpened;
}

bool FMKHActivateAbilityByInputTask::ShouldWaitForAttackEnd(const UMKHAbilitySystemComponent& AbilitySystem, const FMKHActivateAbilityByInputTaskInstanceData& Data)
{
	using namespace MKHGameplayTags;
	const bool bIsAttackInput = Data.PressedInputTag.MatchesTag(Input::Attacks) && !Data.PressedInputTag.MatchesTagExact(Input::Block);
	const bool bIsSwap = Data.PressedSwapInputTag.IsValid() || Data.PressedInputTag.MatchesTag(Input::WeaponQuickSlotCategory);
	if ((!bIsAttackInput && !bIsSwap) || !AbilitySystem.HasMatchingGameplayTag(Ability::Attacking)
		|| FindActiveSpecForInputTag(AbilitySystem, Data.PressedInputTag).IsValid())
	{
		return false;
	}
	if (bIsSwap)
	{
		return true;
	}

	// Same check as a key press: what the priority rules allow interrupts the running attack at once.
	const FGameplayAbilitySpec* Spec = AbilitySystem.FindAbilitySpecForInputTag(Data.PressedInputTag);
	return Spec == nullptr || !IsValid(Spec->Ability) || !Spec->Ability->CanActivateAbility(Spec->Handle, AbilitySystem.AbilityActorInfo.Get());
}

bool FMKHActivateAbilityByInputTask::CanCancelOnExit(const UMKHAbilitySystemComponent& AbilitySystem, const FMKHActivateAbilityByInputTaskInstanceData& Data)
{
	using namespace MKHGameplayTags;
	return AbilitySystem.HasMatchingGameplayTag(State::Movement::Dead) || Data.ElapsedTime >= Data.MaxDuration
		|| (!AbilitySystem.HasMatchingGameplayTag(Ability::Priority1) && !AbilitySystem.HasMatchingGameplayTag(Ability::Priority2));
}

void FMKHActivateAbilityByInputTask::StartInputs(FStateTreeExecutionContext& Context, UMKHAbilitySystemComponent& AbilitySystem) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	// Swap first (instant): the attack input is pressed in Tick once GA_QuickSlot is done.
	if (Data.PressedSwapInputTag.IsValid())
	{
		AbilitySystem.AbilityInputPressed(Data.PressedSwapInputTag, false);
		AbilitySystem.AbilityInputReleased(Data.PressedSwapInputTag);
		Data.bSwapPending = true;
		return;
	}
	PressInput(Context, AbilitySystem);
}
