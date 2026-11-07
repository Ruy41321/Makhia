// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "MKHAbilityTask_ApplyDMT.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMKHDMTFinishedDelegate);

/**
 * Task to smoothly translate and rotate the character towards the best target in front of the camera.
 */
UCLASS()
class MAKHIA_API UMKHAbilityTask_ApplyDMT : public UAbilityTask
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	UMKHAbilityTask_ApplyDMT(const FObjectInitializer& ObjectInitializer);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Creates a task to apply Directional Magnetic Targeting, adjusting character position and rotation over time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UMKHAbilityTask_ApplyDMT* ApplyDMT(
		UGameplayAbility* OwningAbility,
		float Radius = 300.f,
		float MaxAngle = 35.f,
		float TargetStopDistance = 150.f);

	// ============================================================
	// Properties
	// ============================================================

	/** Triggered when the duration expires or task ends natively. */
	UPROPERTY(BlueprintAssignable)
	FMKHDMTFinishedDelegate OnDMTFinished;

protected:
	// ============================================================
	// Internal Logic
	// ============================================================

	virtual void OnDestroy(bool bInOwnerFinished) override;

	/** Updates character rotation to face the target. */
	void UpdateRotation(ACharacter* Character, AActor* Target, float DeltaTime) const;
	
	/** Updates character rotation towards the aiming direction. */
	void UpdateRotation(ACharacter* Character, float DeltaTime) const;

	/** Updates character position to move towards the target. */
	void UpdatePosition(ACharacter* Character, AActor* Target, float DeltaTime) const;

	/**
	 * Retrieves the aiming direction: the camera for players, the control rotation (focus) for AI, else the actor forward.
	 *
	 * @param OutForwardVector  Normalized aiming direction.
	 * @return False without a valid ability or pawn.
	 */
	bool GetTargetingForwardVector(FVector& OutForwardVector) const;

	/** Performs a sphere trace to find potential targets. */
	void PerformTargetingTrace(const FVector& TraceStart, TArray<FHitResult>& OutHits) const;

	/** Evaluates hit results and returns the best matching target. */
	AActor* SelectBestTargetFromHits(const TArray<FHitResult>& Hits, const FVector& TraceStart, const FVector& ForwardVector) const;

private:
	float DMTDuration = 0.15f;
	float DMTRadius = 300.f;
	float DMTMaxAngle = 35.f;
	float DMTAlignmentWeight = 0.3f;
	float DMTDistanceWeight = 0.7f;
	float DMTTargetStopDistance = 150.f;
	float DMTInterpSpeedPosition = 10.f;
	float DMTInterpSpeedRotation = 20.f;

	float TimeElapsed = 0.f;
	
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	/** Helper to find the optimal target. */
	bool FindBestTarget(AActor*& OutTarget) const;
};
