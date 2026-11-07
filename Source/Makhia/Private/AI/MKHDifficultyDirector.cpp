// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/MKHDifficultyDirector.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHDifficultyDirector::BeginDestroy()
{
	UnbindHits();

	Super::BeginDestroy();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHDifficultyDirector::StartTracking(const ACharacterBase* Player, const ACharacterBase* Bot, const FMKHDifficultyAdjustmentSettings& InSettings)
{
	Settings = InSettings;
	UnbindHits();

	BindHits(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ACharacterBase*>(Player)), false);
	BindHits(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ACharacterBase*>(Bot)), true);

	ResetMatch();
}

void UMKHDifficultyDirector::ResetMatch()
{
	CurrentLevel = FMath::Clamp(Settings.StartingLevel, 0, FMath::Max(Settings.ProfileOrder.Num() - 1, 0));
	DamageTaken = 0.f;
	DamageDealt = 0.f;
}

FName UMKHDifficultyDirector::EvaluateRound()
{
	const float RoundScore = FMKHDifficultyAdjustment::ComputeRoundScore(DamageDealt, DamageTaken);
	const int32 NextLevel = FMKHDifficultyAdjustment::ComputeNextLevel(CurrentLevel, RoundScore, Settings);

	UE_LOG(LogMKHAI, Log, TEXT("MKHDifficultyDirector: damage dealt %.0f, taken %.0f, score %.2f: level %d -> %d."),
		DamageDealt, DamageTaken, RoundScore, CurrentLevel, NextLevel);

	CurrentLevel = NextLevel;
	DamageTaken = 0.f;
	DamageDealt = 0.f;
	return GetCurrentProfileId();
}

FName UMKHDifficultyDirector::GetCurrentProfileId() const
{
	return Settings.ProfileOrder.IsValidIndex(CurrentLevel) ? Settings.ProfileOrder[CurrentLevel] : NAME_None;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHDifficultyDirector::HandleHitReceived(const FGameplayEventData* Payload, const bool bIsBot)
{
	if (Payload != nullptr)
	{
		(bIsBot ? DamageDealt : DamageTaken) += Payload->EventMagnitude;
	}
}

void UMKHDifficultyDirector::BindHits(UAbilitySystemComponent* ASC, const bool bIsBot)
{
	if (!IsValid(ASC))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDifficultyDirector: a participant has no ability system, its damage is not measured."));
		return;
	}

	FDelegateHandle Handle = ASC->GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::HitReceived)
		.AddUObject(this, &UMKHDifficultyDirector::HandleHitReceived, bIsBot);

	(bIsBot ? BotASC : PlayerASC) = ASC;
	(bIsBot ? BotHitHandle : PlayerHitHandle) = Handle;
}

void UMKHDifficultyDirector::UnbindHits()
{
	if (UAbilitySystemComponent* ASC = PlayerASC.Get())
	{
		ASC->GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::HitReceived).Remove(PlayerHitHandle);
	}
	if (UAbilitySystemComponent* ASC = BotASC.Get())
	{
		ASC->GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::HitReceived).Remove(BotHitHandle);
	}
	PlayerASC.Reset();
	BotASC.Reset();
}
