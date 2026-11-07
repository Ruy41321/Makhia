// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/EnemyBase.h"

#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"

// ============================================================
// Lifecycle
// ============================================================

AEnemyBase::AEnemyBase()
{
	bReplicates = true;

	MKHAbilitySystemComponent = CreateDefaultSubobject<UMKHAbilitySystemComponent>(TEXT("MKHAbilitySystemComponent"));
	MKHAbilitySystemComponent->SetIsReplicated(true);
	MKHAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	MKHAttributeSet = CreateDefaultSubobject<UMKHAttributeSet>(TEXT("MKHAttributeSet"));
}

void AEnemyBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnemyBase, bInitAttributes);
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	BindCallbacksToDependencies();
	InitAbilityActorInfo();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AEnemyBase::InitAbilityActorInfo()
{
	if (!IsValid(MKHAbilitySystemComponent) || !IsValid(MKHAttributeSet))
	{
		return;
	}

	MKHAbilitySystemComponent->InitAbilityActorInfo(this, this);

	// As for the player: the state machine entered its first state during the components' BeginPlay, make sure its
	// tag is on the ability system now that the actor info exists (no-op when already there).
	if (IsValid(MovementStateMachine))
	{
		MovementStateMachine->SyncCurrentStateTagToASC();
	}

	if (HasAuthority())
	{
		InitClassDefaults();
		BroadcastInitialValues();
	}
}

void AEnemyBase::BindCallbacksToDependencies()
{
	Super::BindCallbacksToDependencies();

	if (IsValid(MKHAbilitySystemComponent) && HasAuthority())
	{
		MKHAbilitySystemComponent->OnAttributesGiven.AddLambda(
			[this]
			{
				bInitAttributes = true;
			});
	}
}

void AEnemyBase::OnRep_InitAttributes()
{
	BroadcastInitialValues();
}
