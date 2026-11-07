// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "MKHPickupAnimationTypes.generated.h"

class APawn;

/** How a pickup appears in the world when it is spawned. */
UENUM(BlueprintType)
enum class EMKHPickupAppearance : uint8
{
	/** Shown at its resting place right away. */
	Instant,
	/** Thrown from the belly of the dropping pawn to its resting place, then bounces. Requires the pawn. */
	Drop,
	/** Emerges from the ground below its resting place, without bouncing (item spawn points). */
	RiseFromGround
};

/** Visual animation a pickup plays on every machine (see UMKHPickupAnimationComponent). */
UENUM(BlueprintType)
enum class EMKHPickupAnimationType : uint8
{
	/** No animation: the pickup rests at its place. */
	None,
	/** Appearance: flight from the dropping pawn, then bounces. */
	Drop,
	/** Appearance: emerging from the ground. */
	RiseFromGround,
	/** Collection: up above the collector's head, then down into the collector. */
	Collect
};

/**
 * Replicated description of the animation a pickup is playing, set by the server when the pickup is
 * spawned or collected. Every machine plays it locally from this data; the pickup's logical position never moves.
 */
USTRUCT(BlueprintType)
struct FMKHPickupAnimationState
{
	GENERATED_BODY()

	/** Which animation is playing. */
	UPROPERTY(BlueprintReadOnly, Category = "Items|Pickup|Animation")
	EMKHPickupAnimationType Type = EMKHPickupAnimationType::None;

	/** Dropping pawn (Drop) or collecting pawn (Collect); nullptr otherwise or when not relevant on this machine. */
	UPROPERTY(BlueprintReadOnly, Category = "Items|Pickup|Animation")
	TObjectPtr<APawn> Pawn;

	/** Server-side fallback when Pawn is not available: start point of a drop, center of the collector for a collection. */
	UPROPERTY(BlueprintReadOnly, Category = "Items|Pickup|Animation")
	FVector_NetQuantize Origin = FVector::ZeroVector;

	/** Server world time at which the animation started; late receivers use it to skip animations that are already over. */
	UPROPERTY(BlueprintReadOnly, Category = "Items|Pickup|Animation")
	double ServerStartTime = 0.0;
};
