// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AbilitySystem/MKHAbilityTuning.h"

#include "Animation/AnimMontage.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Character/Animation/MKHAnimNotify_GameplayEvent.h"
#include "MKHLogChannels.h"
#include "Misc/DataValidation.h"

// ============================================================
// Public Interface
// ============================================================

void FMKHAbilityTuningRow::OnDataTableChanged(const UDataTable* InDataTable, const FName InRowName)
{
	// A new row usually gets its name first: take the tag from it so the designer sets it only once.
	if (!AbilityTag.IsValid())
	{
		AbilityTag = FGameplayTag::RequestGameplayTag(InRowName, false);
		return;
	}

	if (AbilityTag.GetTagName() != InRowName)
	{
		UE_LOG(LogMKHData, Warning, TEXT("%s: row '%s' has AbilityTag %s; lookups go by row name, rename the row to the tag."),
			*GetNameSafe(InDataTable), *InRowName.ToString(), *AbilityTag.ToString());
	}
}

#if WITH_EDITOR
EDataValidationResult FMKHAbilityTuningRow::IsDataValid(FDataValidationContext& Context) const
{
	// Editor-only messages: culture invariant, never gathered for translation.
	const auto Message = [this](const TCHAR* Text)
	{
		return FText::AsCultureInvariant(FString::Printf(TEXT("%s: %s"), *AbilityTag.ToString(), Text));
	};

	EDataValidationResult Result = EDataValidationResult::Valid;
	if (!AbilityTag.IsValid())
	{
		Context.AddError(Message(TEXT("AbilityTag is not set.")));
		Result = EDataValidationResult::Invalid;
	}
	if (Attacks.IsEmpty())
	{
		Context.AddError(Message(TEXT("Attacks is empty, every damage ability needs one entry per strike.")));
		Result = EDataValidationResult::Invalid;
	}

	// DMT stops the attacker at AttackRange from a target found within DMTRadius.
	const bool bUsesDMT = Archetype != EMKHAbilityArchetype::Projectile && bEnableDMT;
	if (bUsesDMT && AttackRange > DMTRadius)
	{
		Context.AddWarning(Message(TEXT("AttackRange is larger than DMTRadius, DMT never moves the attacker.")));
	}
	if (Archetype == EMKHAbilityArchetype::AOE && bUsesDMT && AttackRange > AOERadius)
	{
		Context.AddWarning(Message(TEXT("AttackRange is larger than AOERadius, DMT stops the attacker out of the AOE.")));
	}
	return Result;
}
#endif

float FMKHAbilityTuningRow::GetEffectiveReach() const
{
	switch (Archetype)
	{
	case EMKHAbilityArchetype::Projectile:
		return EffectiveRange;
	case EMKHAbilityArchetype::AOE:
		return bEnableDMT ? FMath::Max(DMTRadius, AOERadius) : AOERadius;
	default:
		return bEnableDMT ? DMTRadius : AttackRange;
	}
}

// ============================================================
// FMKHAbilityTiming
// ============================================================

FMKHAbilityTiming FMKHAbilityTiming::BuildFromMontage(const UAnimMontage& Montage)
{
	// A montage authored at another speed plays faster or slower than its timeline: rescale every time.
	const float RateScale = FMath::Max(Montage.RateScale, UE_KINDA_SMALL_NUMBER);

	FMKHAbilityTiming Timing;
	Timing.MontageDuration = Montage.GetPlayLength() / RateScale;
	for (const FAnimNotifyEvent& NotifyEvent : Montage.Notifies)
	{
		const UMKHAnimNotify_GameplayEvent* Notify = Cast<UMKHAnimNotify_GameplayEvent>(NotifyEvent.Notify);
		const bool bIsStrike = IsValid(Notify) && (Notify->GetEventTag() == MKHGameplayTags::Event::HitScanStart
			|| Notify->GetEventTag() == MKHGameplayTags::Event::SpawnProjectile);
		if (bIsStrike)
		{
			Timing.StrikeHitTimes.Add(NotifyEvent.GetTriggerTime() / RateScale);
		}
	}

	// Notifies are stored in authoring order, not by time.
	Timing.StrikeHitTimes.Sort();
	return Timing;
}

float FMKHAbilityTiming::GetTimeToHit(const int32 StrikeIndex) const
{
	if (StrikeHitTimes.IsEmpty())
	{
		return 0.f;
	}
	return StrikeHitTimes[FMath::Clamp(StrikeIndex, 0, StrikeHitTimes.Num() - 1)];
}
