// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/CharacterBase.h"

#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/CharacterClassInfo.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Libraries/MKHAbilitySystemLibrary.h"
#include "MKHCollisionChannels.h"
#include "MKHLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Character/Components/BasicMovement/BasicMovementComponent.h"
#include "Character/Components/Blocking/BlockingSystemComponent.h"
#include "Character/Components/CharacterMovement/MKHCharacterMovementComponent.h"
#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/Components/Dodge/DodgeSystemComponent.h"
#include "Audio/MKHSoundEventComponent.h"
#include "Character/Components/Footprints/MKHFootprintComponent.h"
#include "Character/Components/Jump/JumpSystemComponent.h"
#include "Character/Components/Sprint/SprintSystemComponent.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"

// ============================================================
// Lifecycle
// ============================================================

ACharacterBase::ACharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMKHCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	// Subobject names are shared by every subclass: Blueprint children keep their component overrides by these names.
	DodgeSystem = CreateDefaultSubobject<UDodgeSystemComponent>(TEXT("Dodge System Component"));
	CrouchSystem = CreateDefaultSubobject<UCrouchSystemComponent>(TEXT("Crouch System Component"));
	BlockingSystem = CreateDefaultSubobject<UBlockingSystemComponent>(TEXT("Blocking System Component"));
	BasicMovementSystem = CreateDefaultSubobject<UBasicMovementComponent>(TEXT("Basic Movement Component"));
	JumpSystem = CreateDefaultSubobject<UJumpSystemComponent>(TEXT("Jump System Component"));
	SprintSystem = CreateDefaultSubobject<USprintSystemComponent>(TEXT("Sprint System Component"));
	MovementStateMachine = CreateDefaultSubobject<UMovementStateMachine>(TEXT("Movement State Machine"));
	FootprintComponent = CreateDefaultSubobject<UMKHFootprintComponent>(TEXT("Footprint Component"));
	SoundEventComponent = CreateDefaultSubobject<UMKHSoundEventComponent>(TEXT("Sound Event Component"));

	// The footprint decals are stamped under the feet: they must not project onto the character.
	GetMesh()->SetReceivesDecals(false);
}

void ACharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACharacterBase, bIsHoldingWeapon);
}

void ACharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	IgnoreCameraCollision();
}

void ACharacterBase::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (IsValid(JumpSystem))
	{
		JumpSystem->OnLanded(Hit);
	}
}

// ============================================================
// Public Interface (Ability System)
// ============================================================

UAbilitySystemComponent* ACharacterBase::GetAbilitySystemComponent() const
{
	return MKHAbilitySystemComponent;
}

// ============================================================
// Public Interface (Movement)
// ============================================================

void ACharacterBase::SetMaxWalkSpeed(float NewSpeed)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!IsValid(Movement))
	{
		return;
	}

	Movement->MaxWalkSpeed = NewSpeed;
	Movement->MaxWalkSpeedCrouched = NewSpeed;
	UE_LOG(LogMKHMovement, Verbose, TEXT("CharacterBase: %s MaxWalkSpeed set to %f"), *GetName(), NewSpeed);
}

void ACharacterBase::IgnoreCameraCollision() const
{
	// A boom hitting a moving character (e.g. crouch spam over a corpse) snaps in and out: see CharacterSystemArchitecture.md, Camera Setup.
	// The player's boom probes the custom CameraObstacle channel, not ECC_Camera: ignore both.
	for (UPrimitiveComponent* Component : { static_cast<UPrimitiveComponent*>(GetCapsuleComponent()), static_cast<UPrimitiveComponent*>(GetMesh()) })
	{
		if (IsValid(Component))
		{
			Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			Component->SetCollisionResponseToChannel(ECC_MKHCameraObstacle, ECR_Ignore);
		}
	}
}

EMovementStateValue ACharacterBase::GetCurrentMovementState() const
{
	return IsValid(MovementStateMachine) ? MovementStateMachine->GetCurrentState() : EMovementStateValue::None;
}

EMovementStateValue ACharacterBase::GetPreviousMovementState() const
{
	return IsValid(MovementStateMachine) ? MovementStateMachine->GetPreviousState() : EMovementStateValue::None;
}

bool ACharacterBase::TransitionToMovementState(EMovementStateValue NewState, bool bForceTransition)
{
	return IsValid(MovementStateMachine) && MovementStateMachine->TransitionToState(NewState, bForceTransition);
}

void ACharacterBase::Multicast_TransitionToMovementState_Implementation(EMovementStateValue NewState, bool bForceTransition)
{
	TransitionToMovementState(NewState, bForceTransition);
}

FString ACharacterBase::GetCurrentMovementStateAsString() const
{
	return UMovementStateTypes::MovementStateToString(GetCurrentMovementState());
}

void ACharacterBase::UnsubscribeFromMovementStateChanges(UObject* Subscriber)
{
	if (IsValid(MovementStateMachine))
	{
		MovementStateMachine->UnsubscribeFromStateChanges(Subscriber);
	}
}

// ============================================================
// Public Interface (Animation)
// ============================================================

void ACharacterBase::Multicast_StopMontage_Implementation(UAnimMontage* MontageToStop, const float InBlendOutTime)
{
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		// Stop only the requested montage: a late-arriving stop for a cancelled ability
		// must never kill the montage of a different ability that has since started.
		AnimInstance->Montage_Stop(InBlendOutTime, MontageToStop);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void ACharacterBase::InitAbilityActorInfo()
{
	// Each subclass decides who owns the ability system component (player state or the actor itself).
}

void ACharacterBase::BindCallbacksToDependencies()
{
	if (!IsValid(MKHAbilitySystemComponent) || !IsValid(MKHAttributeSet))
	{
		return;
	}

	// Rebind-safe: actor info re-initialization (e.g. the controller replicating after
	// the player state) runs this again, and stale bindings must not accumulate double
	// broadcasts. Weak lambdas make RemoveAll(this) effective and stop callbacks from
	// outliving this character (for players the delegates live on the PlayerState-owned ASC).
	MKHAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetHealthAttribute()).RemoveAll(this);
	MKHAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetHealthAttribute()).AddWeakLambda(this,
		[this](const FOnAttributeChangeData& Data)
		{
			OnHealthChanged.Broadcast(Data.OldValue, Data.NewValue, MKHAttributeSet->GetMaxHealth());
		});

	BindStaminaCallbacks();
	BindCrowdControlCallbacks();

	// Server only (the component checks): event-triggered sound effects.
	if (IsValid(SoundEventComponent))
	{
		SoundEventComponent->BindToAbilitySystem(MKHAbilitySystemComponent);
	}
}

void ACharacterBase::InitClassDefaults()
{
	if (!CharacterTag.IsValid())
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("CharacterBase: CharacterTag is not set for %s"), *GetNameSafe(this));
		return;
	}

	UCharacterClassInfo* ClassInfo = UMKHAbilitySystemLibrary::GetCharacterClassDefaultInfo(this);
	if (!IsValid(ClassInfo))
	{
		return;
	}

	const FCharacterClassDefaultInfo* SelectedClass = ClassInfo->ClassDefaultInfoMap.Find(CharacterTag);
	if (SelectedClass == nullptr)
	{
		return;
	}

	if (!IsValid(MKHAbilitySystemComponent))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("CharacterBase: MKHAbilitySystemComponent is not valid for %s"), *GetNameSafe(this));
		return;
	}

	MKHAbilitySystemComponent->AddCharacterAbilities(SelectedClass->StartingAbilities);
	MKHAbilitySystemComponent->AddCharacterPassiveAbilities(SelectedClass->StartingPassives);
	MKHAbilitySystemComponent->InitializeDefaultAttributes(SelectedClass->DefaultAttributes);
}

void ACharacterBase::HandleStaminaChanged(float OldStamina, float CurrentStamina, float MaxStamina)
{
	if (!IsValid(MKHAbilitySystemComponent))
	{
		return;
	}

	const bool bIsOutOfStamina = MKHAbilitySystemComponent->HasMatchingGameplayTag(MKHGameplayTags::State::OutOfStamina);

	if (CurrentStamina <= 0.f && !bIsOutOfStamina)
	{
		MKHAbilitySystemComponent->AddLooseGameplayTag(MKHGameplayTags::State::OutOfStamina);
	}
	else if (CurrentStamina == MaxStamina && bIsOutOfStamina)
	{
		MKHAbilitySystemComponent->RemoveLooseGameplayTag(MKHGameplayTags::State::OutOfStamina);
	}

	UpdateStaminaRegen(OldStamina, CurrentStamina, MaxStamina);
}

void ACharacterBase::BroadcastInitialValues()
{
	if (!IsValid(MKHAttributeSet))
	{
		return;
	}

	OnHealthChanged.Broadcast(MKHAttributeSet->GetHealth(), MKHAttributeSet->GetHealth(), MKHAttributeSet->GetMaxHealth());
	OnStaminaChanged.Broadcast(MKHAttributeSet->GetStamina(), MKHAttributeSet->GetStamina(), MKHAttributeSet->GetMaxStamina());
}

void ACharacterBase::HandleCrowdControlTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// Only react when the crowd-control effect becomes active; ignore its removal.
	if (NewCount <= 0)
	{
		return;
	}

	// Force the character out of crouch (respecting headroom).
	if (IsValid(CrouchSystem))
	{
		CrouchSystem->ForceUncrouch();
	}

	// Interrupt any in-progress jump so upward movement stops immediately.
	if (IsValid(JumpSystem))
	{
		JumpSystem->InterruptJump();
	}
}

void ACharacterBase::BindStaminaCallbacks()
{
	// Rebind-safe, same rationale as BindCallbacksToDependencies.
	MKHAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetStaminaAttribute()).RemoveAll(this);
	MKHAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetStaminaAttribute()).AddWeakLambda(this,
		[this](const FOnAttributeChangeData& Data)
		{
			HandleStaminaChanged(Data.OldValue, Data.NewValue, MKHAttributeSet->GetMaxStamina());
			OnStaminaChanged.Broadcast(Data.OldValue, Data.NewValue, MKHAttributeSet->GetMaxStamina());
		});
}

void ACharacterBase::UpdateStaminaRegen(const float OldStamina, const float CurrentStamina, const float MaxStamina)
{
	// Effects are applied by the server only: a client's copy would be overwritten by the replicated one.
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(MKHAbilitySystemComponent) || !MKHAbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	// Any spend pauses the regeneration and restarts the delay, so it resumes only after a pause in spending.
	if (CurrentStamina < OldStamina)
	{
		RemoveStaminaRegen();
		World->GetTimerManager().SetTimer(StaminaRegenTimer, this, &ACharacterBase::ApplyStaminaRegen, FMath::Max(StaminaRegenDelay, UE_KINDA_SMALL_NUMBER));
	}
	else if (CurrentStamina >= MaxStamina)
	{
		RemoveStaminaRegen();
	}
}

void ACharacterBase::ApplyStaminaRegen()
{
	// The block holds the regeneration: BP_BlockingSystemComp applies it again when the block ends.
	if (!IsValid(MKHAbilitySystemComponent) || GetCurrentMovementState() == EMovementStateValue::Blocking)
	{
		return;
	}
	if (!IsValid(StaminaRegenEffectClass))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("CharacterBase: StaminaRegenEffectClass is not set on %s, stamina will not regenerate."), *GetNameSafe(this));
		return;
	}

	const FGameplayEffectContextHandle EffectContext = MKHAbilitySystemComponent->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = MKHAbilitySystemComponent->MakeOutgoingSpec(StaminaRegenEffectClass, 1.f, EffectContext);
	if (Spec.IsValid())
	{
		MKHAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void ACharacterBase::RemoveStaminaRegen()
{
	if (IsValid(MKHAbilitySystemComponent) && MKHAbilitySystemComponent->HasMatchingGameplayTag(MKHGameplayTags::State::StaminaRecovery))
	{
		MKHAbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(MKHGameplayTags::State::StaminaRecovery.GetTag().GetSingleTagContainer());
	}
}

void ACharacterBase::BindCrowdControlCallbacks()
{
	// Crowd control (Stunned / Staggered) cancels ongoing movement actions when applied.
	const FGameplayTag CrowdControlTags[] = { MKHGameplayTags::State::Stunned, MKHGameplayTags::State::Staggered };
	for (const FGameplayTag& CrowdControlTag : CrowdControlTags)
	{
		FOnGameplayEffectTagCountChanged& TagEvent =
			MKHAbilitySystemComponent->RegisterGameplayTagEvent(CrowdControlTag, EGameplayTagEventType::NewOrRemoved);
		TagEvent.RemoveAll(this);
		TagEvent.AddUObject(this, &ACharacterBase::HandleCrowdControlTagChanged);
	}
}
