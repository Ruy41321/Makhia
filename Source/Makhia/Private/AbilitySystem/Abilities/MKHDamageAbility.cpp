// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/MKHDamageAbility.h"

#include "MKHLogChannels.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/MKHAbilityGrantPayload.h"
#include "AbilitySystem/MKHAbilityTuning.h"
#include "AbilitySystem/MKHAbilityTypes.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Equipment/Weapon/MKHWeaponBase.h"
#include "Equipment/EquipmentTypes.h"
#include "Character/CharacterBase.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/MKHMeleeAbility.h"
#include "Libraries/MKHAbilitySystemLibrary.h"
#include "Data/GenericClassReference.h"
#include "Data/MKHDataSubsystem.h"
#include "GameInstance/MKHGameInstance.h"
#include "Player/PlayerController/MKHPlayerController.h"

// ============================================================
// Lifecycle
// ============================================================

/** Fallback stamina damage dealt to a blocking target when an attack has no per-attack data. */
static constexpr float DefaultStaminaDamageOnBlock = 20.f;

/** Tuning used by an ability without a row: one neutral strike and the row's default reach. */
static const FMKHAbilityTuningRow DefaultAbilityTuning;

UMKHDamageAbility::UMKHDamageAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	SetAssetTags(FGameplayTagContainer(MKHGameplayTags::Ability::Type_Attacks));
	ActivationOwnedTags.Reset();
	ActivationOwnedTags.AddTag(MKHGameplayTags::Ability::Attacking);
	CancelAbilitiesWithTag.Reset();
	CancelAbilitiesWithTag.AddTag(MKHGameplayTags::Ability::Type_Attacks);
}

void UMKHDamageAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (!IsValid(InitOwningWeapon()))
		{
			UE_LOG(LogMKHAbility, Error, TEXT("Failed to initialize owning weapon for damage ability. Ending ability."));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UMKHDamageAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, 
	const FGameplayEventData* TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
	
	if (!Cast<UMKHMeleeAbility>(this))
	{
		FaceCharacterTowardsAttack();
	}
}

void UMKHDamageAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	// Balancing data on every machine (the AbilityTag replicates with the spec).
	ResolveAbilityTuning(ActorInfo, Spec);

	// This instance already starts from the CDO defaults (InstancedPerActor). The per-grant
	// payload set by GrantEquipmentAbility overrides them with this weapon's rolled data.
	if (const UMKHAbilityGrantPayload* Payload = GetGrantPayload(Spec))
	{
		SetIsSkillAbility(Payload->bIsSkillAbility);
		SetCooldownTags(Payload->CooldownTags);
	}

	AssignBPClasses(ActorInfo);
}

const UMKHAbilityGrantPayload* UMKHDamageAbility::GetGrantPayload(const FGameplayAbilitySpec& Spec)
{
	return Cast<UMKHAbilityGrantPayload>(Spec.SourceObject.Get());
}

void UMKHDamageAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// Skills use a custom cooldown logic based on set-by-caller magnitude
	if (!bIsSkillAbility)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}
	
	const float CooldownTime = GetAbilityTuning().CooldownTime;
	if (IsValid(CooldownEffect) && CooldownTime > 0.f)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownEffect, GetAbilityLevel());
		if (!SpecHandle.IsValid())
		{
			return;
		}

		// Setting CD time through set by caller magnitude
		SpecHandle.Data.Get()->SetSetByCallerMagnitude(MKHGameplayTags::Combat::Data_AbilityCooldownTime, CooldownTime);

		// Setting dynamically the tag of the cooldown to apply to the actor
		SpecHandle.Data.Get()->DynamicGrantedTags.Reset();
		for (const FGameplayTag& CooldownTag : CooldownTagContainer)
		{
			SpecHandle.Data.Get()->DynamicGrantedTags.AddTag(CooldownTag);
		}

		// Apply Custom Cooldown
		(void)ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
}

const FGameplayTagContainer* UMKHDamageAbility::GetCooldownTags() const
{
	return &CooldownTagContainer;
}

void UMKHDamageAbility::OnMontageStarted_Implementation()
{
	Super::OnMontageStarted_Implementation();

	// A new attack sequence starts: allow self-activation effects to apply again from step 0.
	LastSelfActivationComboIndex = INDEX_NONE;

	if (bIsComboAbility)
	{
		ComboHitCounter = 0;
		BindOnContinueComboEvents();
	}

	TryApplySelfActivationStatusEffects();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHDamageAbility::CaptureDamageEffectInfo(AActor* TargetActor, FDamageEffectInfo& OutInfo)
{
	AActor* AvatarActorFromInfo = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActorFromInfo))
	{
		return;
	}

	const TArray<FAttackData>& AttacksData = GetAttacks();
	const bool bHasAttackData = AttacksData.IsValidIndex(ComboHitCounter);

	OutInfo.AvatarActor = AvatarActorFromInfo;
	OutInfo.DamageMultiplier = GetAttackDamageMultiplier();
	OutInfo.StaminaDamage = bHasAttackData ? AttacksData[ComboHitCounter].StaminaDamageValue : DefaultStaminaDamageOnBlock;
	OutInfo.DamageEffect = DamageEffect;
	OutInfo.TargetStatusEffectsContainer = bHasAttackData ? AttacksData[ComboHitCounter].TargetStatusEffectsContainer : FGameplayTagContainer();
	OutInfo.SelfOnHitStatusEffectsContainer = bHasAttackData ? AttacksData[ComboHitCounter].SelfOnHitStatusEffectsContainer : FGameplayTagContainer();
	OutInfo.SourceASC = GetAbilitySystemComponentFromActorInfo();
	
	if (IsValid(TargetActor) && TargetActor->Implements<UAbilitySystemInterface>())
	{
		OutInfo.TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	}
}

void UMKHDamageAbility::SetCooldownTags(const FGameplayTagContainer& InCooldownTagContainer)
{
	CooldownTagContainer = InCooldownTagContainer;
}

const FMKHAbilityTuningRow& UMKHDamageAbility::GetAbilityTuning() const
{
	// Looked up on use, never cached: a table edited in the editor applies from the next strike.
	const UMKHDataSubsystem* Data = DataSubsystem.Get();
	const FMKHAbilityTuningRow* Tuning = IsValid(Data) ? Data->FindAbilityTuning(TuningTag) : nullptr;
	return Tuning != nullptr ? *Tuning : DefaultAbilityTuning;
}

void UMKHDamageAbility::SetIsSkillAbility(bool InIsSkillAbility)
{
	bIsSkillAbility = InIsSkillAbility;
}

void UMKHDamageAbility::OnAbilityActivatedAgain_Implementation(float TimeWaited)
{
	Super::OnAbilityActivatedAgain_Implementation(TimeWaited);
	
	if (bIsWithinComboWindow)
	{
		bContinueCombo = true;
	}
}

void UMKHDamageAbility::OnComboTriggered_Implementation(int32 HitCounter)
{
	FaceCharacterTowardsAttack();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

AMKHWeaponBase* UMKHDamageAbility::InitOwningWeapon()
{
	OwningWeapon = nullptr;

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return nullptr;
	}

	// The equipment instance managing the weapon (FRPGEquipmentEntry::Instance) is
	// server-only, so slot lookup only works on the authority. The weapon actor itself
	// replicates and arrives attached to the character mesh on every machine, so resolving
	// it from the avatar's attached actors works everywhere.
	TArray<AActor*> AttachedActors;
	AvatarActor->GetAttachedActors(AttachedActors);

	for (AActor* AttachedActor : AttachedActors)
	{
		if (AMKHWeaponBase* Weapon = Cast<AMKHWeaponBase>(AttachedActor))
		{
			OwningWeapon = Weapon;
			break;
		}
	}

	return OwningWeapon;
}

const TArray<FAttackData>& UMKHDamageAbility::GetAttacks() const
{
	return GetAbilityTuning().Attacks;
}

float UMKHDamageAbility::GetAttackDamageMultiplier() const
{
	const TArray<FAttackData>& AttacksData = GetAttacks();
	if (ComboHitCounter >= AttacksData.Num())
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("ComboHitCounter %d exceeds AttacksData array size %d. Using neutral multiplier."), ComboHitCounter, AttacksData.Num());
		return 1.f;
	}
	return AttacksData[ComboHitCounter].DamagePercent;
}

void UMKHDamageAbility::FaceCharacterTowardsAttack()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return;
	}

	ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor);
	if (!IsValid(Character))
	{
		return;
	}
	
	// For an AI the control rotation follows the controller's focus (SetFocus on the opponent).
	const FRotator ControlRotation = Character->GetControlRotation();
	const FRotator TargetRotation = FRotator(0.f, ControlRotation.Yaw, 0.f);
	Character->SetActorRotation(TargetRotation);
}

void UMKHDamageAbility::AssignBPClasses(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (ActorInfo == nullptr || !ActorInfo->AvatarActor.IsValid())
	{
		return;
	}

	const UWorld* World = ActorInfo->AvatarActor->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(World);
	if (!IsValid(GameInstance))
	{
		return;
	}

	UGenericClassReference* ClassDataAsset = GameInstance->GetGenericClassReference();
	if (!IsValid(ClassDataAsset))
	{
		return;
	}

	// Resolve the generic effect classes through their well-known named keys.
	CooldownEffect = UGenericClassReference::GetGameplayEffectByName(ClassDataAsset, UGenericClassReference::SkillCooldownEffectKey);
	DamageEffect = UGenericClassReference::GetGameplayEffectByName(ClassDataAsset, UGenericClassReference::DamageEffectKey);
}

void UMKHDamageAbility::ResolveAbilityTuning(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	TuningTag = FGameplayTag();
	const AActor* Owner = ActorInfo != nullptr ? ActorInfo->OwnerActor.Get() : nullptr;
	DataSubsystem = UMKHDataSubsystem::Get(Owner);
	if (!DataSubsystem.IsValid())
	{
		return;
	}

	// The spec carries the input tag and the AbilityTag: the tuning tag is the one with a row.
	for (const FGameplayTag& Tag : Spec.GetDynamicSpecSourceTags())
	{
		if (DataSubsystem->FindAbilityTuning(Tag) != nullptr)
		{
			TuningTag = Tag;
			return;
		}
	}

	UE_LOG(LogMKHAbility, Warning, TEXT("%s: no ability tuning row for spec tags [%s], using one neutral strike and the default reach."),
		*GetNameSafe(this), *Spec.GetDynamicSpecSourceTags().ToStringSimple());
}

void UMKHDamageAbility::BindOnContinueComboEvents()
{
	UAbilityTask_WaitGameplayEvent* WaitEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		MKHGameplayTags::Event::ContinueComboStart
	);
	
	if (IsValid(WaitEvent))
	{
		WaitEvent->EventReceived.AddDynamic(this, &UMKHDamageAbility::OnContinueComboStartReceived);
		WaitEvent->ReadyForActivation();
	}
	
	WaitEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		MKHGameplayTags::Event::ContinueComboEnd
	);
	
	if (IsValid(WaitEvent))
	{
		WaitEvent->EventReceived.AddDynamic(this, &UMKHDamageAbility::OnContinueComboEndReceived);
		WaitEvent->ReadyForActivation();
	}
}

void UMKHDamageAbility::OnContinueComboStartReceived(FGameplayEventData Payload)
{
	bIsWithinComboWindow = true;
	bContinueCombo = false;
}

void UMKHDamageAbility::OnContinueComboEndReceived(FGameplayEventData Payload)
{
	bIsWithinComboWindow = false;
	if (bContinueCombo)
	{
		++ComboHitCounter;

		// The next combo strike starts now: its self-activation effects must be active
		// before any damage spec of that strike is created.
		TryApplySelfActivationStatusEffects();

		OnComboTriggered(ComboHitCounter);
		return;
	}

	// Only the locally-controlled instance owns input timing and may end the chain naturally.
	// The server proxy of a remote player would race the client's own end under latency, so
	// it waits for the client's replicated EndAbility instead. bWasCancelled=false because
	// this is a completion, not a cancel: it must not force-stop a follow-up montage.
	if (IsLocallyControlled())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UMKHDamageAbility::SyncAuthoritativeComboCounter(int32 InComboIndex)
{
	const int32 MaxComboIndex = FMath::Max(GetAttacks().Num() - 1, 0);
	ComboHitCounter = FMath::Clamp(FMath::Max(ComboHitCounter, InComboIndex), 0, MaxComboIndex);

	// Under latency the server may learn about a combo strike only when its hit arrives.
	// Catch up the strike's self-activation effects here, before the damage spec is
	// created, so buffs like Empower are still captured by this hit's damage calculation.
	TryApplySelfActivationStatusEffects();
}

void UMKHDamageAbility::TryApplySelfActivationStatusEffects()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	// One application per combo strike, no matter how many call sites reach it first.
	const TArray<FAttackData>& AttacksData = GetAttacks();
	if (LastSelfActivationComboIndex == ComboHitCounter || !AttacksData.IsValidIndex(ComboHitCounter))
	{
		return;
	}
	LastSelfActivationComboIndex = ComboHitCounter;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		return;
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());

	UMKHAbilitySystemLibrary::ApplyStatusEffects(ASC, ASC, AttacksData[ComboHitCounter].SelfActivationStatusEffectsContainer, ContextHandle);
}

