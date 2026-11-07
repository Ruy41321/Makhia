// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MKHPickupInteractionComponent.generated.h"

class AMKHPickupItem;
class APawn;

/**
 * Chooses which world pickup the local player is aiming at, so the pickup can show its detail widget and
 * the Interact input can collect it (AMKHPlayerController::Server_PickupItem). Owned by the player
 * controller and active only on the machine that controls it; nothing is focused while the pawn is dead
 * or a UI screen has focus.
 * Polls on a timer rather than Tick or overlap events: the candidates must be re-ranked whenever the
 * camera turns, even when nothing enters or leaves an interaction range.
 *
 * Full model: see Documentation/PickupSystemArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHPickupInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Disables ticking: the selection runs on a timer. */
	UMKHPickupInteractionComponent();

	/** Starts the selection timer when the owner is a locally controlled player controller. */
	virtual void BeginPlay() override;

	/** Stops the selection timer and forgets the tracked pickups. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Gets the pickup the local player is aiming at, or nullptr when none is within the cone. */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	AMKHPickupItem* GetFocusedPickup() const { return FocusedPickup.Get(); }

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Timer callback: collects the candidates, picks the focused one and updates their interaction state. */
	void RefreshFocus();

	/**
	 * Collects the pickups whose interaction range contains the pawn; pickups whose item has not
	 * replicated yet (nothing to show or collect), pickups still animating or being collected
	 * (AMKHPickupItem::IsInteractable) and pickups reserved to another player are skipped.
	 *
	 * @param Pawn The local pawn.
	 * @return The candidates, in no particular order.
	 */
	TArray<AMKHPickupItem*> GatherCandidates(const APawn& Pawn) const;

	/**
	 * Picks the pickup to focus among the candidates within MaxConeAngleDegrees of the view direction.
	 * Ranking uses the angle to each focus point, minus FocusStickinessDegrees for the pickup already
	 * focused; every candidate within FocusTieToleranceDegrees of the best ranked angle counts as a tie,
	 * and the tie goes to the one closest to the pawn.
	 *
	 * @param Candidates     Pickups within range.
	 * @param CameraLocation Location of the player's view.
	 * @param CameraForward  Unit forward direction of the player's view.
	 * @param PawnLocation   Location of the local pawn, used for the tie-break.
	 * @return The focused pickup, or nullptr when none is within the cone.
	 */
	AMKHPickupItem* PickBestCandidate(const TArray<AMKHPickupItem*>& Candidates, const FVector& CameraLocation,
		const FVector& CameraForward, const FVector& PawnLocation) const;

	/**
	 * Pushes the new state to the pickups: pickups no longer in range are cleared first, then the other
	 * candidates are unfocused, and the focused one is updated last.
	 *
	 * @param Candidates Pickups within range this refresh.
	 * @param NewFocused The candidate being aimed at, if any.
	 */
	void UpdateInteractionStates(const TArray<AMKHPickupItem*>& Candidates, const AMKHPickupItem* NewFocused);

	/** Clears the focus and the interaction state of every tracked pickup (e.g. while the player has no pawn). */
	void ClearFocus();

	// ============================================================
	// Properties
	// ============================================================

	/** Seconds between two selections. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Items|Pickup", meta = (ClampMin = "0.01", Units = "s", AllowPrivateAccess = "true"))
	float RefreshInterval = 0.1f;

	/** Maximum angle between the camera forward and the direction to a pickup for it to be focused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Items|Pickup", meta = (ClampMin = "0", ClampMax = "180", Units = "deg", AllowPrivateAccess = "true"))
	float MaxConeAngleDegrees = 30.f;

	/** Candidates whose angles differ by at most this much count as equally aimed at: the closest one wins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Items|Pickup", meta = (ClampMin = "0", Units = "deg", AllowPrivateAccess = "true"))
	float FocusTieToleranceDegrees = 2.f;

	/** Angle advantage given to the pickup already focused, so the focus does not flicker between nearly aligned pickups. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Items|Pickup", meta = (ClampMin = "0", Units = "deg", AllowPrivateAccess = "true"))
	float FocusStickinessDegrees = 3.f;

	/** Pickup currently focused; weak because the server may destroy it at any time. */
	TWeakObjectPtr<AMKHPickupItem> FocusedPickup;

	/** Candidates of the last refresh, so the ones that leave the range can have their state cleared. */
	TArray<TWeakObjectPtr<AMKHPickupItem>> TrackedCandidates;

	/** Drives RefreshFocus; only set on the locally controlled player controller. */
	FTimerHandle RefreshTimerHandle;
};
