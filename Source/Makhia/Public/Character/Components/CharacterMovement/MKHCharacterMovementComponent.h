// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MKHCharacterMovementComponent.generated.h"

/**
 * Character movement of every duel character (ACharacterBase): the engine movement plus the "no standing on heads"
 * rule. A falling character never lands on another pawn and, when it rests on top of one, is pushed off sideways at
 * once instead of sliding slowly down the other capsule. Runs the same way on server and owning client (prediction).
 *
 * Full model: see Documentation/CharacterSystemArchitecture.md ("Pawn stacking").
 */
UCLASS()
class MAKHIA_API UMKHCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Rejects another pawn as a landing spot, then defers to the engine checks.
	 *
	 * @param CapsuleLocation  Location of the capsule at the hit.
	 * @param Hit              Blocking hit of the falling move.
	 * @return                 True when the character may land there.
	 */
	virtual bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const override;

	/**
	 * Remembers a falling contact on top of another pawn so the next falling step pushes off it.
	 *
	 * @param Hit        Blocking hit of the move.
	 * @param TimeSlice  Time of the move slice.
	 * @param MoveDelta  Attempted move.
	 */
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

	/**
	 * Applies the pending push-off from a pawn, then runs the engine falling physics.
	 *
	 * @param DeltaTime   Time step.
	 * @param Iterations  Physics iteration count.
	 */
	virtual void PhysFalling(float DeltaTime, int32 Iterations) override;

private:
	/**
	 * @param Hit  Hit to test.
	 * @return     True when the hit is another pawn's collision.
	 */
	static bool IsPawnHit(const FHitResult& Hit);

	/**
	 * @param Hit  Hit of a falling move.
	 * @return     True when the hit is on the upper part of another pawn (a side contact returns false).
	 */
	bool IsOnTopOfPawn(const FHitResult& Hit) const;

	/**
	 * Horizontal direction away from the pawn below: from its centre to ours, or our facing when they are aligned.
	 *
	 * @param OtherPawn  Pawn the character rests on.
	 * @return           Unit horizontal direction.
	 */
	FVector ComputePushOffDirection(const AActor& OtherPawn) const;

	/**
	 * Raises the horizontal speed along Direction to PawnPushOffSpeed, keeping the vertical speed.
	 *
	 * @param Direction  Unit horizontal direction.
	 */
	void ApplyPushOff(const FVector& Direction);

	// ============================================================
	// Properties
	// ============================================================

public:
	/** Horizontal speed (cm/s) that pushes a character off another pawn's head. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Makhia|Pawn Stacking", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float PawnPushOffSpeed = 350.f;

	/** Minimum up component of the contact normal that counts as "on top of" a pawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Makhia|Pawn Stacking", meta = (ClampMin = "0", ClampMax = "1"))
	float PawnTopMinNormalZ = 0.3f;

private:
	/** Pawn touched from above during the last falling step; consumed by the next PhysFalling. */
	TWeakObjectPtr<const AActor> PendingPushOffPawn;
};
