// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/Pickup/MKHPickupAnimationTypes.h"
#include "MKHPickupAnimationComponent.generated.h"

class APawn;
class USceneComponent;

/** Fired when an animation reaches its end on this machine (not when it is skipped or replaced). */
DECLARE_MULTICAST_DELEGATE_OneParam(FMKHPickupAnimationFinishedSignature, EMKHPickupAnimationType /*FinishedType*/);

/**
 * Plays the procedural animations of a world pickup (drop, rise from the ground, collection) by moving,
 * spinning and scaling one visual component around the pivot of the displayed mesh. Purely cosmetic and local:
 * AMKHPickupItem starts it on every machine from its replicated FMKHPickupAnimationState, the actor itself never
 * moves. Ticks only while an animation plays and never on a dedicated server.
 * Every duration and shape parameter is tunable in the Blueprint class defaults of the pickup.
 *
 * Full model: see Documentation/PickupSystemArchitecture.md ("Animations").
 */
UCLASS(ClassGroup = (Makhia), meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHPickupAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Sets up a post-physics tick, disabled until an animation starts. */
	UMKHPickupAnimationComponent();

	/** Advances the playing animation, then applies its pose or finishes it. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Starts an animation from its first frame, replacing the one playing. Does nothing on a dedicated server.
	 * A zero-length animation is skipped to its end (see SkipToEnd).
	 *
	 * @param Type               Animation to play; None is ignored.
	 * @param InAnimatedComponent Component moved by the animation; its rest pose is the identity relative transform.
	 * @param InPivotComponent   Direct child of InAnimatedComponent (the mesh) whose location is the animated point.
	 * @param Pawn               Dropping pawn (Drop) or collecting pawn (Collect); may be nullptr.
	 * @param FallbackOrigin     Used when Pawn is nullptr: drop start point, or center of the collector.
	 */
	void Play(EMKHPickupAnimationType Type, USceneComponent* InAnimatedComponent, const USceneComponent* InPivotComponent,
		const APawn* Pawn, const FVector& FallbackOrigin);

	/**
	 * Shows the final state of an animation without playing it: the rest pose, hidden after a collection.
	 * Used when the animation is already over by the time this machine learns about it.
	 *
	 * @param Type               Animation to skip.
	 * @param InAnimatedComponent Component moved by the animation.
	 */
	void SkipToEnd(EMKHPickupAnimationType Type, USceneComponent* InAnimatedComponent);

	/** Whether an animation is playing on this machine. */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup|Animation")
	bool IsPlaying() const { return ActiveType != EMKHPickupAnimationType::None; }

	/** Gets the animation playing on this machine, None when idle. */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup|Animation")
	EMKHPickupAnimationType GetPlayingAnimation() const { return ActiveType; }

	/**
	 * Gets the total length of an animation with the current parameters.
	 *
	 * @param Type Animation to measure.
	 * @return Length in seconds; 0 for None.
	 */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup|Animation")
	float GetAnimationDuration(EMKHPickupAnimationType Type) const;

	/** Gets how long the server keeps a collected pickup alive: the collection animation plus CollectDestroyMargin. */
	float GetCollectDestroyDelay() const;

	/** Gets how late an animation may start on a machine before it is skipped instead (see LateStartTolerance). */
	float GetLateStartTolerance() const { return LateStartTolerance; }

	/**
	 * Computes where a drop starts: the pawn's belly (capsule center) moved by the drop origin offsets.
	 *
	 * @param Pawn The dropping pawn.
	 * @return World location of the drop start.
	 */
	FVector ComputeDropOrigin(const APawn& Pawn) const;

	/** Broadcast when an animation reaches its end on this machine. */
	FMKHPickupAnimationFinishedSignature OnAnimationFinished;

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Pose of the animated point at one instant, in world space. */
	struct FPose
	{
		/** World location of the pivot (mesh). */
		FVector Location = FVector::ZeroVector;

		/** Scale applied around the pivot. */
		FVector Scale = FVector::OneVector;

		/** Spin around the vertical axis, in degrees. */
		float SpinDegrees = 0.f;
	};

	/** Evaluates the playing animation at Time. */
	FPose ComputePose(float Time) const;

	/** Drop: flight arc from StartLocation to the rest location, then the bounces. */
	FPose ComputeDropPose(float Time) const;

	/**
	 * Drop, after the flight: damped bounces whose durations follow the square root of their height.
	 *
	 * @param BounceTime Time elapsed since the landing.
	 */
	FPose ComputeDropBouncePose(float BounceTime) const;

	/** Rise: from RiseDepth below the rest location up to it, without overshoot. */
	FPose ComputeRisePose(float Time) const;

	/** Collect: up to CollectHeadLocation, hold, then down to CollectBellyLocation while shrinking. */
	FPose ComputeCollectPose(float Time) const;

	/**
	 * Squash & stretch shortly after a contact with the ground (a sine bump over LandingSquashDuration).
	 *
	 * @param TimeSinceContact Time elapsed since the contact.
	 * @param Amount           Maximum vertical squash (0.3 = 30% flatter).
	 */
	FVector ComputeSquashScale(float TimeSinceContact, float Amount) const;

	/** Refreshes the collection targets from the collecting pawn, which keeps moving; keeps the last ones without it. */
	void UpdateCollectTargets();

	/**
	 * Sets the collection targets from a pawn's capsule.
	 *
	 * @param PawnCenter     Capsule center (the belly).
	 * @param PawnHalfHeight Capsule half height (the top of the head is above the center by this much).
	 */
	void SetCollectTargets(const FVector& PawnCenter, float PawnHalfHeight);

	/** Gets the world location of the pivot when the animated component rests at its identity transform. */
	FVector GetRestPivotLocation() const;

	/** Gets the world transform of the animated component's parent (identity without one). */
	FTransform GetParentTransform() const;

	/** Moves the animated component so the pivot reaches the pose, scaled and spun around the pivot itself. */
	void ApplyPose(const FPose& Pose) const;

	/** Applies the final state, stops ticking and broadcasts OnAnimationFinished. */
	void FinishAnimation();

	/** Clears the playback state and stops ticking. */
	void ResetPlayback();

	// ============================================================
	// Properties
	// ============================================================

	// --- Drop ---

	/** Flight time from the dropping pawn to the landing point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "s"))
	float DropFlightDuration = 0.3f;

	/** Time spent bouncing after the landing (ignored when DropBounceCount is 0). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "s"))
	float DropBounceDuration = 0.2f;

	/** Height of the flight arc above the straight line from the start point to the landing point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "cm"))
	float DropArcHeight = 50.f;

	/** How far in front of the pawn's belly (capsule center) the drop starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float DropOriginForwardOffset = 30.f;

	/** How far above the pawn's belly (capsule center) the drop starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float DropOriginHeightOffset = 0.f;

	/** Scale of the item when it leaves the pawn; it grows to full size during the flight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float DropStartScale = 0.5f;

	/** Spin around the vertical axis during the flight, in degrees (ends facing its resting direction). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true"))
	float DropSpinDegrees = 360.f;

	/** Number of bounces after the landing; 0 lands without bouncing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "5"))
	int32 DropBounceCount = 2;

	/** Height of the first bounce. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "cm"))
	float DropBounceHeight = 20.f;

	/** Height of each bounce relative to the previous one (0.35 = 35%). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "1"))
	float DropBounceDamping = 0.35f;

	/** Vertical squash at the landing (0.25 = 25% flatter), reduced by DropBounceDamping at each following contact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "0.9"))
	float LandingSquash = 0.25f;

	/** How long a squash lasts after each contact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "s"))
	float LandingSquashDuration = 0.07f;

	// --- Rise from ground ---

	/** Time to emerge from the ground (item spawn points). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Rise From Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "s"))
	float RiseDuration = 0.5f;

	/** How far below its resting place the item starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Rise From Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "cm"))
	float RiseDepth = 80.f;

	/** Scale of the item when it starts rising; it grows to full size on the way up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Rise From Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float RiseStartScale = 0.3f;

	/** Spin around the vertical axis while rising, in degrees (ends facing its resting direction). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Rise From Ground", meta = (AllowPrivateAccess = "true"))
	float RiseSpinDegrees = 180.f;

	// --- Collect ---

	/** Time to fly from the resting place to above the collector's head. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Collect", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "s"))
	float CollectRiseDuration = 0.25f;

	/** Pause above the collector's head. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Collect", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "s"))
	float CollectHoldDuration = 0.05f;

	/** Time to come down into the collector. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Collect", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "s"))
	float CollectDescendDuration = 0.2f;

	/** Height of the item above the top of the collector's capsule at the peak. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Collect", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float CollectAboveHeadHeight = 40.f;

	/** Where the item disappears, relative to the collector's belly (capsule center). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Collect", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float CollectTargetHeightOffset = 0.f;

	/** Scale of the item when it disappears into the collector. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Collect", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float CollectEndScale = 0.1f;

	/** Spin around the vertical axis during the whole collection, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Collect", meta = (AllowPrivateAccess = "true"))
	float CollectSpinDegrees = 360.f;

	/** Extra time the server keeps a collected pickup alive, so clients finish the animation before it is destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Collect", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "s"))
	float CollectDestroyMargin = 0.25f;

	// --- Network ---

	/**
	 * How late, past its end, an animation may still be started on a machine that learns about it late
	 * (network delay); beyond that it is skipped to its final state instead of replayed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup Animation|Network", meta = (AllowPrivateAccess = "true", ClampMin = "0", Units = "s"))
	float LateStartTolerance = 1.f;

	// --- Playback state (local) ---

	/** Component moved by the playing animation. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> AnimatedComponent;

	/** Direct child of AnimatedComponent whose location is the animated point (the mesh). */
	UPROPERTY(Transient)
	TObjectPtr<const USceneComponent> PivotComponent;

	/** Collecting pawn followed by the collection animation. */
	TWeakObjectPtr<const APawn> TargetPawn;

	/** Animation playing on this machine. */
	EMKHPickupAnimationType ActiveType = EMKHPickupAnimationType::None;

	/** Time elapsed since the playing animation started. */
	float Elapsed = 0.f;

	/** World location of the pivot at the first frame (drop start, or where a collection starts). */
	FVector StartLocation = FVector::ZeroVector;

	/** Collection peak above the collector's head, updated while the collector is valid. */
	FVector CollectHeadLocation = FVector::ZeroVector;

	/** Collection end point in the collector's belly, updated while the collector is valid. */
	FVector CollectBellyLocation = FVector::ZeroVector;
};
