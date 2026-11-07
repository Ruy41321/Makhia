// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/Pickup/MKHPickupInteractionComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Inventory/Pickup/MKHPickupItem.h"
#include "Player/PlayerController/MKHPlayerController.h"
#include "TimerManager.h"

namespace
{
	/** One candidate inside the cone, as ranked by PickBestCandidate. */
	struct FPickupFocusScore
	{
		AMKHPickupItem* Pickup = nullptr;
		double RankedAngle = 0.0;
		double DistSqToPawn = 0.0;
	};

	/** Angle in degrees between a unit view direction and the direction to a point; negative when the point is at the view origin. */
	double ComputeViewAngleDegrees(const FVector& ViewLocation, const FVector& ViewForward, const FVector& Point)
	{
		const FVector ToPoint = (Point - ViewLocation).GetSafeNormal();
		if (ToPoint.IsZero())
		{
			return -1.0;
		}

		// Clamped: rounding can push the dot of two unit vectors past 1, where acos returns NaN.
		const double Cosine = FMath::Clamp(FVector::DotProduct(ViewForward, ToPoint), -1.0, 1.0);
		return FMath::RadiansToDegrees(FMath::Acos(Cosine));
	}
}

UMKHPickupInteractionComponent::UMKHPickupInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMKHPickupInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	const AMKHPlayerController* PC = Cast<AMKHPlayerController>(GetOwner());
	UWorld* World = GetWorld();
	if (IsValid(PC) && PC->IsLocalController() && IsValid(World))
	{
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UMKHPickupInteractionComponent::RefreshFocus, RefreshInterval, true);
	}
}

void UMKHPickupInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	// No state notification here: the pickups may already be tearing down with the world.
	FocusedPickup.Reset();
	TrackedCandidates.Reset();

	Super::EndPlay(EndPlayReason);
}

void UMKHPickupInteractionComponent::RefreshFocus()
{
	const AMKHPlayerController* PC = Cast<AMKHPlayerController>(GetOwner());
	const APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;
	if (!IsValid(Pawn) || PC->IsPawnDead() || PC->IsUIFocused())
	{
		ClearFocus();
		return;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const TArray<AMKHPickupItem*> Candidates = GatherCandidates(*Pawn);
	AMKHPickupItem* NewFocused = PickBestCandidate(Candidates, CameraLocation, CameraRotation.Vector(), Pawn->GetActorLocation());

	FocusedPickup = NewFocused;
	UpdateInteractionStates(Candidates, NewFocused);
}

TArray<AMKHPickupItem*> UMKHPickupInteractionComponent::GatherCandidates(const APawn& Pawn) const
{
	TArray<AMKHPickupItem*> Candidates;

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return Candidates;
	}

	const APlayerState* LocalPlayerState = Pawn.GetPlayerState();
	for (TActorIterator<AMKHPickupItem> It(World); It; ++It)
	{
		AMKHPickupItem* Pickup = *It;
		if (IsValid(Pickup) && Pickup->GetItem().IsValid() && Pickup->IsInteractable() && Pickup->CanBeCollectedBy(LocalPlayerState)
			&& Pickup->IsPawnInInteractionRange(Pawn))
		{
			Candidates.Add(Pickup);
		}
	}

	return Candidates;
}

AMKHPickupItem* UMKHPickupInteractionComponent::PickBestCandidate(const TArray<AMKHPickupItem*>& Candidates,
	const FVector& CameraLocation, const FVector& CameraForward, const FVector& PawnLocation) const
{
	TArray<FPickupFocusScore> Scores;
	double BestRankedAngle = TNumericLimits<double>::Max();
	const AMKHPickupItem* CurrentFocus = FocusedPickup.Get();

	for (AMKHPickupItem* Candidate : Candidates)
	{
		if (!IsValid(Candidate))
		{
			continue;
		}

		// The cone always uses the real angle; only the ranking favours the current focus.
		const double Angle = ComputeViewAngleDegrees(CameraLocation, CameraForward, Candidate->GetInteractionFocusPoint());
		if (Angle < 0.0 || Angle > MaxConeAngleDegrees)
		{
			continue;
		}

		const double RankedAngle = Candidate == CurrentFocus ? Angle - FocusStickinessDegrees : Angle;
		Scores.Add({ Candidate, RankedAngle, FVector::DistSquared(PawnLocation, Candidate->GetActorLocation()) });
		BestRankedAngle = FMath::Min(BestRankedAngle, RankedAngle);
	}

	AMKHPickupItem* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (const FPickupFocusScore& Score : Scores)
	{
		if (Score.RankedAngle <= BestRankedAngle + FocusTieToleranceDegrees && Score.DistSqToPawn < BestDistSq)
		{
			Best = Score.Pickup;
			BestDistSq = Score.DistSqToPawn;
		}
	}

	return Best;
}

void UMKHPickupInteractionComponent::UpdateInteractionStates(const TArray<AMKHPickupItem*>& Candidates, const AMKHPickupItem* NewFocused)
{
	for (const TWeakObjectPtr<AMKHPickupItem>& Tracked : TrackedCandidates)
	{
		AMKHPickupItem* TrackedPickup = Tracked.Get();
		if (IsValid(TrackedPickup) && !Candidates.Contains(TrackedPickup))
		{
			TrackedPickup->SetInteractionState(false, false);
		}
	}

	TrackedCandidates.Reset(Candidates.Num());
	AMKHPickupItem* FocusedCandidate = nullptr;

	for (AMKHPickupItem* Candidate : Candidates)
	{
		if (!IsValid(Candidate))
		{
			continue;
		}

		TrackedCandidates.Add(Candidate);
		if (Candidate == NewFocused)
		{
			FocusedCandidate = Candidate;
			continue;
		}
		Candidate->SetInteractionState(true, false);
	}

	// Last, so the previously focused pickup has already lost its focus.
	if (IsValid(FocusedCandidate))
	{
		FocusedCandidate->SetInteractionState(true, true);
	}
}

void UMKHPickupInteractionComponent::ClearFocus()
{
	FocusedPickup.Reset();

	for (const TWeakObjectPtr<AMKHPickupItem>& Tracked : TrackedCandidates)
	{
		AMKHPickupItem* TrackedPickup = Tracked.Get();
		if (IsValid(TrackedPickup))
		{
			TrackedPickup->SetInteractionState(false, false);
		}
	}

	TrackedCandidates.Reset();
}
