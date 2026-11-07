// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/MKHMeleeAbility.h"
#include "MKHAOEAbility.generated.h"

/**
 * Melee-derived ability that damages every valid target around the player at once.
 *
 * Instead of driving the weapon's native melee scan, the HitScanStart notify performs a single
 * multi sphere trace for objects centered on the avatar with the tuning row's AOERadius. Every unique
 * actor detected is routed into the standard melee hit pipeline inherited from
 * UMKHMeleeAbility: applied directly on the authority (listen host, AI) or shipped to the
 * server as replicated target data and validated there before applying damage.
 *
 * Server hit validation is kept coherent with the AOE shape: the plausibility range derives
 * from AOERadius (plus a configurable slack) and the swing-cone check is disabled, since the
 * area of effect covers all 360 degrees around the player.
 */
UCLASS()
class MAKHIA_API UMKHAOEAbility : public UMKHMeleeAbility
{
	GENERATED_BODY()

public:
	// ==========================================
	// Lifecycle
	// ==========================================

	/**
	 * Sets default values for this ability's properties (default traced object types).
	 */
	UMKHAOEAbility();

protected:
	// ==========================================
	// Overrides
	// ==========================================

	/**
	 * Performs the AOE hit detection: a single multi sphere trace for objects centered on the
	 * avatar with the tuning row's AOERadius, replacingthe weapon's native melee scan. Every unique
	 * actor detected is forwarded into the inherited melee hit pipeline (direct authoritative
	 * processing or replicated target data to the server).
	 */
	virtual void HitScanStart_Implementation() override;

	/**
	 * No-op: the AOE trace is instantaneous, so there is no weapon scan window to close.
	 */
	virtual void HitScanEnd_Implementation() override;

	/**
	 * Server hit validation range for the AOE: the trace radius plus a slack absorbing
	 * client/server position divergence and target capsule extents.
	 *
	 * @return The maximum accepted hit distance.
	 */
	virtual float GetHitValidationMaxRange() const override;

	/**
	 * Disables the swing-cone validation check: the AOE hits all around the player.
	 *
	 * @return 180 degrees, which accepts targets in any direction.
	 */
	virtual float GetHitValidationMaxConeAngleDegrees() const override { return 180.f; }

	// ==========================================
	// Protected / Internal Logic
	// ==========================================

	/**
	 * Runs the multi sphere trace for objects centered on the avatar.
	 *
	 * @param AvatarActor  The avatar the trace is centered on (also ignored by the trace).
	 * @param OutHits      Every hit detected inside the AOE sphere.
	 * @return True when the trace detected at least one hit.
	 */
	bool PerformAOESphereTrace(AActor* AvatarActor, TArray<FHitResult>& OutHits) const;

	/**
	 * Forwards the detected hits into the inherited melee hit pipeline, at most once per
	 * unique actor (a sphere trace can report multiple components of the same target).
	 *
	 * @param Hits  The hits detected by the AOE sphere trace.
	 */
	void ForwardUniqueActorHits(const TArray<FHitResult>& Hits);

private:
	// ==========================================
	// Properties
	// ==========================================

	/** Object types detected by the AOE sphere trace. Defaults to Pawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "AOE Ability | Core Settings")
	TArray<TEnumAsByte<EObjectTypeQuery>> AOEObjectTypes;

	/** Extra distance added to the tuning row's AOERadius by server hit validation, absorbing client/server position divergence under latency and target capsule extents. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", ClampMin = "0"), Category = "AOE Ability | Hit Validation")
	float AOEHitValidationSlack = 150.f;

	/** If true, draws the AOE sphere trace for debugging purposes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "AOE Ability | Debug")
	bool bDebugDrawAOE = false;
};
