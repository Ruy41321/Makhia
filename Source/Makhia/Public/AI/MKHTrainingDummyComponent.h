// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MKHTrainingDummyComponent.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
struct FGameplayEventData;

/** What the training dummy lets the player test. */
UENUM(BlueprintType)
enum class EMKHTrainingDummyPhase : uint8
{
	/** It blocks until its stamina breaks: stamina damage. */
	Guarding,
	/** It stands open until its health reaches 0: health damage. */
	Open
};

/**
 * Turns the duel bot into the target of the Training mode (AMKHGameMode adds it at the spawn, server only): the bot
 * has no StateTree and stands still, facing the player. It alternates two phases: Guarding until the guard break, Open
 * until its health runs out, when it recovers everything without dying and guards again. Full model: see
 * Documentation/OfflineModes.md.
 */
UCLASS(ClassGroup = (Custom))
class MAKHIA_API UMKHTrainingDummyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Ticks to keep the guard up and to find the player. */
	UMKHTrainingDummyComponent();

	/** Makes the owner unkillable and listens to its guard break and lethal hit events. */
	virtual void BeginPlay() override;

	/**
	 * Keeps the owner facing the player and, in the Guarding phase, retries the block until it is up (a hit or a crowd
	 * control can refuse it).
	 *
	 * @param DeltaTime          Frame time.
	 * @param TickType           Kind of tick.
	 * @param ThisTickFunction   Tick function of this component.
	 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The current phase. */
	UFUNCTION(BlueprintPure, Category = "Training")
	EMKHTrainingDummyPhase GetPhase() const { return Phase; }

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Event.Combat.GuardBreak callback: the guard stays down, the stamina regenerates on its own.
	 *
	 * @param Payload  The event payload (unused).
	 */
	void HandleGuardBreak(const FGameplayEventData* Payload);

	/**
	 * Event.Combat.LethalHitPrevented callback: schedules the recovery. Deferred because the event comes from inside
	 * an attribute change.
	 *
	 * @param Payload  The event payload (unused).
	 */
	void HandleLethalHitPrevented(const FGameplayEventData* Payload);

	/** Restores health and stamina and goes back to the Guarding phase. */
	void Recover();

	/** Focuses the player once it exists, so the guard always faces it. */
	void FocusPlayer();

	// ============================================================
	// Properties
	// ============================================================

	/** Current phase. */
	EMKHTrainingDummyPhase Phase = EMKHTrainingDummyPhase::Guarding;

	/** Owner of this component. */
	TWeakObjectPtr<ACharacterBase> OwnerCharacter;

	/** Ability system of the owner. */
	TWeakObjectPtr<UAbilitySystemComponent> OwnerASC;

	/** True once the owner's controller focuses the player. */
	bool bPlayerFocused = false;

	/** True between a lethal hit and the recovery of the next tick. */
	bool bRecoveryPending = false;
};
