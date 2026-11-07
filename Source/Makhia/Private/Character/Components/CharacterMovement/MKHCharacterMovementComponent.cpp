// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/Components/CharacterMovement/MKHCharacterMovementComponent.h"

#include "GameFramework/Pawn.h"

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHCharacterMovementComponent::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
{
	if (IsPawnHit(Hit))
	{
		return false;
	}

	return Super::IsValidLandingSpot(CapsuleLocation, Hit);
}

void UMKHCharacterMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);

	if (IsFalling() && IsOnTopOfPawn(Hit))
	{
		PendingPushOffPawn = Hit.GetActor();
	}
}

void UMKHCharacterMovementComponent::PhysFalling(float DeltaTime, int32 Iterations)
{
	// The engine overwrites Velocity after an impact, so the push is applied before the next falling step.
	if (const AActor* OtherPawn = PendingPushOffPawn.Get())
	{
		ApplyPushOff(ComputePushOffDirection(*OtherPawn));
	}
	PendingPushOffPawn.Reset();

	Super::PhysFalling(DeltaTime, Iterations);
}

bool UMKHCharacterMovementComponent::IsPawnHit(const FHitResult& Hit)
{
	return Hit.bBlockingHit && IsValid(Cast<APawn>(Hit.GetActor()));
}

bool UMKHCharacterMovementComponent::IsOnTopOfPawn(const FHitResult& Hit) const
{
	return IsPawnHit(Hit) && Hit.GetActor() != GetOwner() && GetGravitySpaceZ(Hit.Normal) >= PawnTopMinNormalZ;
}

FVector UMKHCharacterMovementComponent::ComputePushOffDirection(const AActor& OtherPawn) const
{
	const FVector Away = ProjectToGravityFloor(UpdatedComponent->GetComponentLocation() - OtherPawn.GetActorLocation());
	if (!Away.IsNearlyZero(1.f))
	{
		return Away.GetSafeNormal();
	}

	return ProjectToGravityFloor(UpdatedComponent->GetForwardVector()).GetSafeNormal();
}

void UMKHCharacterMovementComponent::ApplyPushOff(const FVector& Direction)
{
	const FVector::FReal SpeedAlong = ProjectToGravityFloor(Velocity) | Direction;
	if (SpeedAlong < PawnPushOffSpeed)
	{
		Velocity += Direction * (PawnPushOffSpeed - SpeedAlong);
	}
}
