// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/MKHAOEAbility.h"

#include "MKHLogChannels.h"
#include "AbilitySystem/MKHAbilityTuning.h"
#include "Engine/EngineTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/ObjectKey.h"

// ============================================================
// Lifecycle
// ============================================================

UMKHAOEAbility::UMKHAOEAbility()
{
	AOEObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
}

// ============================================================
// Overrides
// ============================================================

void UMKHAOEAbility::HitScanStart_Implementation()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("HitScanStart: no valid avatar actor for %s, AOE scan aborted."), *GetNameSafe(this));
		return;
	}

	TArray<FHitResult> HitResults;
	if (PerformAOESphereTrace(AvatarActor, HitResults))
	{
		ForwardUniqueActorHits(HitResults);
	}
}

void UMKHAOEAbility::HitScanEnd_Implementation()
{
	// Intentionally empty: the AOE trace happens once at HitScanStart, no weapon scan runs.
}

// ============================================================
// Protected / Internal Logic
// ============================================================

float UMKHAOEAbility::GetHitValidationMaxRange() const
{
	return GetAbilityTuning().AOERadius + AOEHitValidationSlack;
}

bool UMKHAOEAbility::PerformAOESphereTrace(AActor* AvatarActor, TArray<FHitResult>& OutHits) const
{
	const FVector Center = AvatarActor->GetActorLocation();

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AvatarActor);

	// A minimal vertical offset keeps the sweep non-degenerate while the trace stays
	// effectively a sphere overlap centered on the player.
	return UKismetSystemLibrary::SphereTraceMultiForObjects(
		AvatarActor,
		Center,
		Center + FVector(0.f, 0.f, 1.f),
		GetAbilityTuning().AOERadius,
		AOEObjectTypes,
		false,
		ActorsToIgnore,
		bDebugDrawAOE ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		OutHits,
		true
	);
}

void UMKHAOEAbility::ForwardUniqueActorHits(const TArray<FHitResult>& Hits)
{
	TSet<FObjectKey> ForwardedActors;
	for (const FHitResult& Hit : Hits)
	{
		const AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor))
		{
			continue;
		}

		bool bAlreadyForwarded = false;
		ForwardedActors.Add(FObjectKey(HitActor), &bAlreadyForwarded);
		if (bAlreadyForwarded)
		{
			continue;
		}

		UE_LOG(LogMKHAbility, Log, TEXT("AOE ability %s detected hit on %s."), *GetNameSafe(this), *GetNameSafe(HitActor));
		HandleMeleeHitDetected(Hit);
	}
}
