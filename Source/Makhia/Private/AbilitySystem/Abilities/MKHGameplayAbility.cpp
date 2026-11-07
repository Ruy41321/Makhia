// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/MKHGameplayAbility.h"

#include "MKHLogChannels.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"

UMKHGameplayAbility::UMKHGameplayAbility()
{
	SetAssetTags(FGameplayTagContainer(MKHGameplayTags::Ability::All));
	CancelAbilitiesWithTag.AddTag(MKHGameplayTags::Ability::All);
	ActivationBlockedTags.AddTag(MKHGameplayTags::State::Movement::Jumping);
	ActivationBlockedTags.AddTag(MKHGameplayTags::State::Movement::Falling);
	ActivationBlockedTags.AddTag(MKHGameplayTags::State::Movement::Dead);
}

void UMKHGameplayAbility::OnAbilityActivatedAgain_Implementation(float TimeWaited)
{
	BindInputPressEvent();
}

void UMKHGameplayAbility::OnAbilityReleased_Implementation(float TimeWaited)
{
	BindInputReleaseEvent();
}

void UMKHGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	BindInputPressEvent();
	BindInputReleaseEvent();

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bTryActivateQueuedAbilityCalled = false; // Reset the flag on activation
	
	// The movement state machine runs independently on every machine and gates
	// transitions (e.g. Attacking -> Blocking) on the priority tag, so the tag must
	// exist on the server ASC too — for remote pawns the server is not locally
	// controlled, and without it the server would allow transitions the owning
	// client denies (cancelling the running ability as a side effect).
	if (PriorityTag.IsValid() && (IsLocallyControlled() || HasAuthority(&ActivationInfo)))
	{
		GetAbilitySystemComponentFromActorInfo()->AddLooseGameplayTag(PriorityTag);
		BindMakeCancellable();
	}
	
	if (IsValid(MontageToPlay))
	{
		if (!PlayAnimation())
			return EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		OnMontageStarted();
	}
}

void UMKHGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Only force-stop the montage when the ability was cancelled (e.g. a stun interrupted
	// by an attack). On a normal end the montage task already stops itself locally
	// (bStopWhenAbilityEnds), and forcing a stop here would race with a queued ability's
	// montage that may have already started on a remote client.
	if (bWasCancelled && IsValid(MontageToPlay) && ActorInfo != nullptr)
	{
		if (ACharacterBase* Character = Cast<ACharacterBase>(ActorInfo->AvatarActor))
		{
			Character->Multicast_StopMontage(MontageToPlay, 0.1f);
		}
	}
	
	// The priority tag is managed on both the predicting client and the server (see
	// ActivateAbility). RemovePriorityTag also mirrors the client removal to the server,
	// ordered BEFORE the queued activation request fired below.
	if (IsLocallyControlled() || HasAuthority(&ActivationInfo))
	{
		RemovePriorityTag();
	}

	if (IsLocallyControlled())
	{
		TryActivateQueuedAbility();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMKHGameplayAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	OnAbilityRemoved();
	
	Super::OnRemoveAbility(ActorInfo, Spec);
}

bool UMKHGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// First check the base class logic (ActivationBlockedTags, SourceRequiredTags, etc.)
	if (!Super::DoesAbilitySatisfyTagRequirements(AbilitySystemComponent, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Verify priority tags if assigned.
	if (PriorityTag.IsValid())
	{
		if (PriorityTag == MKHGameplayTags::Ability::Priority1)
		{
			// Priority1 is the highest priority. Deny if the same priority is already active.
			if (AbilitySystemComponent.HasMatchingGameplayTag(MKHGameplayTags::Ability::Priority1))
			{
				return false;
			}
		}
		else if (PriorityTag == MKHGameplayTags::Ability::Priority2)
		{
			// Priority2 is lower prioritize. Deny if same or higher (Priority1) is already active.
			if (AbilitySystemComponent.HasMatchingGameplayTag(MKHGameplayTags::Ability::Priority1) ||
				AbilitySystemComponent.HasMatchingGameplayTag(MKHGameplayTags::Ability::Priority2))
			{
				return false;
			}
		}
	}
	
	return true;
}

void UMKHGameplayAbility::SetCharacterOrientation(bool bFollowCamera)
{
	// Get the actor safely, using IsValid
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (IsValid(AvatarActor))
	{
		if (ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor))
		{
			Character->bUseControllerRotationYaw = bFollowCamera;
		}
	}
}

void UMKHGameplayAbility::BindInputPressEvent()
{
	UAbilityTask_WaitInputPress* InputPressEvent = UAbilityTask_WaitInputPress::WaitInputPress(this);
	
	if (IsValid(InputPressEvent))
	{
		InputPressEvent->OnPress.AddDynamic(this, &UMKHGameplayAbility::OnAbilityActivatedAgain);
		InputPressEvent->ReadyForActivation();
	}
}

void UMKHGameplayAbility::BindInputReleaseEvent()
{
	UAbilityTask_WaitInputRelease* InputReleaseEvent = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	
	if (IsValid(InputReleaseEvent))
	{
		InputReleaseEvent->OnRelease.AddDynamic(this, &UMKHGameplayAbility::OnAbilityReleased);
		InputReleaseEvent->ReadyForActivation();
	}
}

void UMKHGameplayAbility::BindMakeCancellable()
{
	UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, MKHGameplayTags::Event::MakeAbilityCancellable, nullptr, true, true);
	
	if (IsValid(WaitEventTask))
	{
		WaitEventTask->EventReceived.AddDynamic(this, &UMKHGameplayAbility::OnMakeAbilityCancellable);
		WaitEventTask->ReadyForActivation();
	}
}

void UMKHGameplayAbility::OnMakeAbilityCancellable(FGameplayEventData Payload)
{
	RemovePriorityTag();

	// The input queue only exists on the owning client; the server just clears the tag.
	if (IsLocallyControlled())
	{
		TryActivateQueuedAbility();
	}
}

void UMKHGameplayAbility::RemovePriorityTag()
{
	if (!PriorityTag.IsValid())
		return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
		return;

	// Guarded: on the server this can be reached twice (the owning client's mirror RPC
	// plus the server's own MakeCancellable notify or EndAbility) and the loose tag
	// count must never underflow.
	if (ASC->HasMatchingGameplayTag(PriorityTag))
	{
		ASC->RemoveLooseGameplayTag(PriorityTag, 1, EGameplayTagReplicationState::None);

		// The owning client is the authority on the cancellable window: mirror the removal
		// to the server BEFORE any queued activation request is sent. Both RPCs travel on
		// the ASC's channel, so the reliable ordering guarantees the server clears the tag
		// before validating that activation — its own montage runs one latency behind and
		// would otherwise still hold the tag, rejecting the predicted activation.
		if (!K2_HasAuthority())
		{
			if (UMKHAbilitySystemComponent* MKHAsc = Cast<UMKHAbilitySystemComponent>(ASC))
			{
				MKHAsc->Server_RemovePriorityTag(PriorityTag);
			}
		}
	}
}

void UMKHGameplayAbility::OnMontageStarted_Implementation()
{
	// Does Nothing; To be Overriden
}

void UMKHGameplayAbility::TryActivateQueuedAbility()
{
	if (bTryActivateQueuedAbilityCalled)
		return;
	
	bTryActivateQueuedAbilityCalled = true;
	UAbilitySystemComponent* Asc = GetAbilitySystemComponentFromActorInfo();
	
	if (IsValid(Asc))
		Asc->HandleGameplayEvent(MKHGameplayTags::Event::ActivateQueuedAbility, nullptr);
}

void UMKHGameplayAbility::OnMontageFinished_Implementation()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool UMKHGameplayAbility::PlayAnimation()
{
	if (!IsValid(MontageToPlay))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("No Valid Montage To Play !"));
		return false;
	}
	
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		MontageToPlay,
		1.0f,
		NAME_None,
		true,
		1.0f,
		0.0f,
		true
	);
	
	if (!MontageTask)
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("Failed to create MontageTask!"));
		return false;
	}
	
	MontageTask->OnCompleted.AddDynamic(this, &UMKHGameplayAbility::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UMKHGameplayAbility::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &UMKHGameplayAbility::OnMontageFinished);
 
	MontageTask->ReadyForActivation();
	return true;
}
