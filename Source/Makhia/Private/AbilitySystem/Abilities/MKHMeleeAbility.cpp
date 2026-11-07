// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/MKHMeleeAbility.h"

#include "MKHLogChannels.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/MKHAbilityTuning.h"
#include "AbilitySystem/MKHAbilityTypes.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Tasks/MKHAbilityTask_ApplyDMT.h"
#include "Engine/World.h"
#include "Equipment/Weapon/MKHWeaponBase.h"
#include "Libraries/MKHAbilitySystemLibrary.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHMeleeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Super may have ended the ability already (commit failure, missing weapon on authority).
	if (!IsActive() || ActorInfo == nullptr)
	{
		return;
	}

	AcceptedMeleeHits.Reset();
	bHitScanActive = false;

	// Hit detection runs on the locally-controlled machine: the owning client for players
	// (the only machine whose animation notifies are reliable under latency) and the server
	// for AI. The listen-server host is both, and applies its own hits authoritatively.
	if (ActorInfo->IsLocallyControlled())
	{
		BindHitScanEvents();
		BindApplyDMTEvent();
	}

	// The server proxy of a remote player receives the client's hits as replicated target
	// data and bounds its own lifetime, since it never self-terminates from its lagging
	// montage timeline (see OnMontageFinished).
	if (ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled())
	{
		BindServerTargetDataDelegate();
		ScheduleServerLifetimeFailsafe();
	}
}

void UMKHMeleeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// EndAbility re-enters through montage interruption callbacks and replicated ends: run the
	// melee cleanup only while the end is still valid, and keep it idempotent.
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (bHitScanActive)
		{
			bHitScanActive = false;
			HitScanEnd();
		}

		RemoveServerTargetDataDelegate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMKHMeleeAbility::OnMontageStarted_Implementation()
{
	Super::OnMontageStarted_Implementation();

	TryActivateDMT();
}

void UMKHMeleeAbility::OnMontageFinished_Implementation()
{
	// The server proxy of a remote player must not end itself when its own montage finishes:
	// its timeline lags the owning client's by the activation latency, so ending here races
	// swings the client already started (truncating the montage before its notifies fire).
	// The instance ends through the client's replicated EndAbility, bounded by the failsafe.
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && !CurrentActorInfo->IsLocallyControlled())
	{
		return;
	}

	Super::OnMontageFinished_Implementation();
}

// ============================================================
// Hit Scan Window (locally-controlled machine)
// ============================================================

void UMKHMeleeAbility::BindHitScanEvents()
{
	UE_LOG(LogMKHAbility, Log, TEXT("Binding Hit Scan Events for Melee Ability: %s"), *GetNameSafe(this));
	UAbilityTask_WaitGameplayEvent* HitScanEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		MKHGameplayTags::Event::HitScanStart
	);

	if (IsValid(HitScanEvent))
	{
		HitScanEvent->EventReceived.AddDynamic(this, &UMKHMeleeAbility::OnHitScanStartReceived);
		HitScanEvent->ReadyForActivation();
	}

	HitScanEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		MKHGameplayTags::Event::HitScanEnd
	);

	if (IsValid(HitScanEvent))
	{
		HitScanEvent->EventReceived.AddDynamic(this, &UMKHMeleeAbility::OnHitScanEndReceived);
		HitScanEvent->ReadyForActivation();
	}
}

void UMKHMeleeAbility::OnHitScanStartReceived(FGameplayEventData Payload)
{
	UE_LOG(LogMKHAbility, Log, TEXT("Hit Scan Start received for Melee Ability: %s"), *GetNameSafe(this));
	bHitScanActive = true;
	HitScanStart();
}

void UMKHMeleeAbility::OnHitScanEndReceived(FGameplayEventData Payload)
{
	UE_LOG(LogMKHAbility, Log, TEXT("Hit Scan End received for Melee Ability: %s"), *GetNameSafe(this));
	bHitScanActive = false;
	HitScanEnd();
}

void UMKHMeleeAbility::HitScanStart_Implementation()
{
	AMKHWeaponBase* Weapon = ResolveOwningWeapon();
	if (!IsValid(Weapon))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("HitScanStart: no valid owning weapon for %s, scan aborted."), *GetNameSafe(this));
		return;
	}

	Weapon->OnMeleeHitDetected.AddUniqueDynamic(this, &UMKHMeleeAbility::HandleMeleeHitDetected);
	Weapon->StartMeleeScan(HitScanFrequency);
}

void UMKHMeleeAbility::HitScanEnd_Implementation()
{
	if (IsValid(OwningWeapon))
	{
		OwningWeapon->StopMeleeScan();
		OwningWeapon->OnMeleeHitDetected.RemoveDynamic(this, &UMKHMeleeAbility::HandleMeleeHitDetected);
	}
}

void UMKHMeleeAbility::HandleMeleeHitDetected(const FHitResult& HitResult)
{
	if (!IsActive() || !bHitScanActive)
	{
		return;
	}

	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		// Listen-server host or AI: this machine is the authority, apply directly.
		ProcessAuthoritativeMeleeHit(HitResult, GetComboHitCounter());
	}
	else
	{
		SendMeleeHitToServer(HitResult);
	}
}

AMKHWeaponBase* UMKHMeleeAbility::ResolveOwningWeapon()
{
	if (!IsValid(OwningWeapon))
	{
		InitOwningWeapon();
	}

	return OwningWeapon;
}

// ============================================================
// Replicated Target Data (client -> server)
// ============================================================

void UMKHMeleeAbility::SendMeleeHitToServer(const FHitResult& HitResult)
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	FGameplayAbilityTargetDataHandle TargetDataHandle;
	TargetDataHandle.Add(new FMKHGameplayAbilityTargetData_MeleeHit(HitResult, GetComboHitCounter()));

	// The scoped prediction window lets the server accept target data sent outside the
	// original activation window (each swing of the combo produces new sends).
	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent);
	AbilitySystemComponent->CallServerSetReplicatedTargetData(
		CurrentSpecHandle,
		CurrentActivationInfo.GetActivationPredictionKey(),
		TargetDataHandle,
		FGameplayTag(),
		AbilitySystemComponent->ScopedPredictionKey
	);
}

void UMKHMeleeAbility::BindServerTargetDataDelegate()
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
		.AddUObject(this, &UMKHMeleeAbility::OnServerMeleeTargetDataReceived);
}

void UMKHMeleeAbility::RemoveServerTargetDataDelegate()
{
	if (CurrentActorInfo == nullptr || !CurrentActorInfo->IsNetAuthority() || CurrentActorInfo->IsLocallyControlled())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey()).RemoveAll(this);
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
}

void UMKHMeleeAbility::OnServerMeleeTargetDataReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}

	for (int32 DataIndex = 0; DataIndex < DataHandle.Num(); ++DataIndex)
	{
		const FGameplayAbilityTargetData* TargetData = DataHandle.Get(DataIndex);
		if (TargetData == nullptr || TargetData->GetScriptStruct() != FMKHGameplayAbilityTargetData_MeleeHit::StaticStruct())
		{
			continue;
		}

		const FMKHGameplayAbilityTargetData_MeleeHit* MeleeHitData = static_cast<const FMKHGameplayAbilityTargetData_MeleeHit*>(TargetData);
		if (const FHitResult* HitResult = MeleeHitData->GetHitResult())
		{
			ProcessAuthoritativeMeleeHit(*HitResult, MeleeHitData->ComboIndex);
		}
	}
}

// ============================================================
// Authoritative Hit Processing
// ============================================================

void UMKHMeleeAbility::ProcessAuthoritativeMeleeHit(const FHitResult& HitResult, int32 ComboIndex)
{
	AActor* TargetActor = HitResult.GetActor();
	if (!IsMeleeHitPlausible(TargetActor, ComboIndex))
	{
		UE_LOG(LogMKHAbility, Verbose, TEXT("Melee hit on %s rejected by server validation (combo %d)."), *GetNameSafe(TargetActor), ComboIndex);
		return;
	}

	const UWorld* World = GetWorld();
	AcceptedMeleeHits.Add(FObjectKey(TargetActor), { ComboIndex, IsValid(World) ? World->GetTimeSeconds() : 0.0 });

	// Price the strike with the combo step the detecting machine actually performed, even
	// when this instance's own combo bookkeeping lags behind under latency.
	SyncAuthoritativeComboCounter(ComboIndex);

	FDamageEffectInfo DamageEffectInfo;
	CaptureDamageEffectInfo(TargetActor, DamageEffectInfo);
	if (!IsValid(DamageEffectInfo.TargetASC))
	{
		return;
	}

	UMKHAbilitySystemLibrary::ApplyDamageEffect(DamageEffectInfo);
}

bool UMKHMeleeAbility::IsMeleeHitPlausible(const AActor* TargetActor, int32 ComboIndex) const
{
	if (!IsActive() || !IsValid(TargetActor))
	{
		return false;
	}

	if (UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor) == nullptr)
	{
		return false;
	}

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return false;
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - AvatarActor->GetActorLocation();
	if (ToTarget.SizeSquared() > FMath::Square(GetHitValidationMaxRange()))
	{
		return false;
	}

	const FVector ToTarget2D = ToTarget.GetSafeNormal2D();
	if (!ToTarget2D.IsNearlyZero())
	{
		const float CosineToTarget = FVector::DotProduct(AvatarActor->GetActorForwardVector().GetSafeNormal2D(), ToTarget2D);
		if (CosineToTarget < FMath::Cos(FMath::DegreesToRadians(GetHitValidationMaxConeAngleDegrees())))
		{
			return false;
		}
	}

	if (const FAcceptedMeleeHit* PreviousHit = AcceptedMeleeHits.Find(FObjectKey(TargetActor)))
	{
		// One accepted hit per target per combo strike.
		if (PreviousHit->ComboIndex == ComboIndex)
		{
			return false;
		}

		// Throttle duplicated/forged sends that claim a new combo strike too quickly.
		const UWorld* World = GetWorld();
		if (IsValid(World) && World->GetTimeSeconds() - PreviousHit->TimeSeconds < MeleeRehitCooldown)
		{
			return false;
		}
	}

	return true;
}

// ============================================================
// Server Lifetime Failsafe
// ============================================================

void UMKHMeleeAbility::ScheduleServerLifetimeFailsafe()
{
	UAbilityTask_WaitDelay* FailsafeTask = UAbilityTask_WaitDelay::WaitDelay(this, ServerLifetimeFailsafeDuration);
	if (IsValid(FailsafeTask))
	{
		FailsafeTask->OnFinish.AddDynamic(this, &UMKHMeleeAbility::OnServerLifetimeFailsafeElapsed);
		FailsafeTask->ReadyForActivation();
	}
}

void UMKHMeleeAbility::OnServerLifetimeFailsafeElapsed()
{
	UE_LOG(LogMKHAbility, Warning, TEXT("Melee ability %s force-ended by server lifetime failsafe: no replicated end received from the owning client."), *GetNameSafe(this));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ============================================================
// Directional Magnetic Targeting (DMT) Logic
// ============================================================

void UMKHMeleeAbility::OnComboTriggered_Implementation(int32 HitCounter)
{
	TryActivateDMT();
}

void UMKHMeleeAbility::TryActivateDMT()
{
	// DMT values are balancing data: they live in the ability tuning row (Documentation/AbilityTuning.md).
	const FMKHAbilityTuningRow& Tuning = GetAbilityTuning();
	if (!Tuning.bEnableDMT)
	{
		return;
	}

	UMKHAbilityTask_ApplyDMT* DMTTask = UMKHAbilityTask_ApplyDMT::ApplyDMT(
		this,
		Tuning.DMTRadius,
		Tuning.DMTMaxAngle,
		Tuning.AttackRange
	);

	if (IsValid(DMTTask))
	{
		DMTTask->OnDMTFinished.AddDynamic(this, &UMKHMeleeAbility::OnDMTFinished);
		DMTTask->ReadyForActivation();
	}
}

void UMKHMeleeAbility::OnDMTFinished()
{
	// Cleanup or further sequence triggers if needed
}

void UMKHMeleeAbility::BindApplyDMTEvent()
{
	UAbilityTask_WaitGameplayEvent* ApplyDMTEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		MKHGameplayTags::Event::ApplyDMT
	);

	if (IsValid(ApplyDMTEvent))
	{
		ApplyDMTEvent->EventReceived.AddDynamic(this, &UMKHMeleeAbility::OnApplyDMTReceived);
		ApplyDMTEvent->ReadyForActivation();
	}
}

void UMKHMeleeAbility::OnApplyDMTReceived(FGameplayEventData Payload)
{
	TryActivateDMT();
}
