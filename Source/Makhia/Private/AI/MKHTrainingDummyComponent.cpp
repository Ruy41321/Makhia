// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/MKHTrainingDummyComponent.h"

#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Character/CharacterBase.h"
#include "Character/Components/Blocking/BlockingSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MKHLogChannels.h"
#include "TimerManager.h"

// ============================================================
// Lifecycle
// ============================================================

UMKHTrainingDummyComponent::UMKHTrainingDummyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMKHTrainingDummyComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character);
	if (!IsValid(Character) || !IsValid(ASC))
	{
		UE_LOG(LogMKHAI, Error, TEXT("MKHTrainingDummyComponent: %s needs a character with an ability system."), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	OwnerCharacter = Character;
	OwnerASC = ASC;

	ASC->AddLooseGameplayTag(MKHGameplayTags::State::Unkillable);
	ASC->GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::GuardBreak)
		.AddUObject(this, &UMKHTrainingDummyComponent::HandleGuardBreak);
	ASC->GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::LethalHitPrevented)
		.AddUObject(this, &UMKHTrainingDummyComponent::HandleLethalHitPrevented);
}

void UMKHTrainingDummyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FocusPlayer();

	ACharacterBase* Character = OwnerCharacter.Get();
	if (Phase != EMKHTrainingDummyPhase::Guarding || !IsValid(Character)
		|| Character->GetCurrentMovementState() == EMovementStateValue::Blocking)
	{
		return;
	}

	if (UBlockingSystemComponent* Blocking = Character->FindComponentByClass<UBlockingSystemComponent>(); IsValid(Blocking))
	{
		Blocking->StartBlocking();
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHTrainingDummyComponent::HandleGuardBreak(const FGameplayEventData* Payload)
{
	Phase = EMKHTrainingDummyPhase::Open;

	const ACharacterBase* Character = OwnerCharacter.Get();
	UBlockingSystemComponent* Blocking = IsValid(Character) ? Character->FindComponentByClass<UBlockingSystemComponent>() : nullptr;
	if (IsValid(Blocking))
	{
		Blocking->StopBlocking();
	}
}

void UMKHTrainingDummyComponent::HandleLethalHitPrevented(const FGameplayEventData* Payload)
{
	if (bRecoveryPending)
	{
		return;
	}

	bRecoveryPending = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UMKHTrainingDummyComponent::Recover);
}

void UMKHTrainingDummyComponent::Recover()
{
	bRecoveryPending = false;

	UAbilitySystemComponent* ASC = OwnerASC.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	ASC->SetNumericAttributeBase(UMKHAttributeSet::GetHealthAttribute(), ASC->GetNumericAttribute(UMKHAttributeSet::GetMaxHealthAttribute()));
	ASC->SetNumericAttributeBase(UMKHAttributeSet::GetStaminaAttribute(), ASC->GetNumericAttribute(UMKHAttributeSet::GetMaxStaminaAttribute()));
	Phase = EMKHTrainingDummyPhase::Guarding;
}

void UMKHTrainingDummyComponent::FocusPlayer()
{
	const ACharacterBase* Character = OwnerCharacter.Get();
	AAIController* Controller = IsValid(Character) ? Character->GetController<AAIController>() : nullptr;
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (bPlayerFocused || !IsValid(Controller) || !IsValid(PlayerPawn))
	{
		return;
	}

	Controller->SetFocus(PlayerPawn);
	bPlayerFocused = true;
}
