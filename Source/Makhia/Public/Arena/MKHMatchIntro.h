// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MKHMatchIntro.generated.h"

class ALevelSequenceActor;
class AMKHPlayerController;
class ULevelSequence;
class ULevelSequencePlayer;
class UMKHIntroSkipHintWidget;

/** One intro variant: the sequence ending on the camera of the player who starts at EndLocation. */
USTRUCT(BlueprintType)
struct FMKHMatchIntroVariant
{
	GENERATED_BODY()

	/** Level sequence played for this start (it ends on that player's camera). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intro")
	TSoftObjectPtr<ULevelSequence> Sequence;

	/** Where the sequence ends (the player camera); the variant nearest to the local pawn is played. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intro")
	FVector EndLocation = FVector::ZeroVector;
};

/**
 * Match intro of an arena level: a short camera flight played once per level load, on every peer for its own local
 * player, when the initial preparation starts. Nothing replicates (like AMKHArenaGate). It streams the exterior
 * sub-level in for the flight and out after it, hides the HUD overlay and locks movement / look input while it plays;
 * the sequence ends on the local player's camera, so handing the view back is seamless. Holding the skip key
 * (IA_SkipIntro, see AMKHPlayerController::SetMatchIntroInputActive) ends it early, with an on-screen hint.
 * See Documentation/Handoff/MakhionCinematics.md.
 */
UCLASS()
class MAKHIA_API AMKHMatchIntro : public AActor
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	AMKHMatchIntro();

protected:
	/** Starts waiting for the local player (not on a dedicated server). */
	virtual void BeginPlay() override;

	/** Stops the sequence and restores the HUD if the level ends during the intro. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Waits for a fresh initial preparation with the local pawn, then plays once (frame 0 held under loading). */
	virtual void Tick(float DeltaSeconds) override;

	// ============================================================
	// Public Interface
	// ============================================================

public:
	/** Returns true while the intro is playing. */
	UFUNCTION(BlueprintPure, Category = "Arena|Intro")
	bool IsIntroPlaying() const;

	/** Ends the intro at once: jumps to the last frame (the player camera) and cleans up like a normal end. */
	UFUNCTION(BlueprintCallable, Category = "Arena|Intro")
	void SkipIntro();

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Returns true when the intro should start now: local pawn possessed and already the view target (the sequence
	 * restores the view target it starts from), initial preparation running with at least
	 * MinPhaseTimeLeft seconds left (a late join skips it).
	 * @param OutPawnLocation The local pawn's location, to pick the variant.
	 */
	bool IsReadyToPlay(FVector& OutPawnLocation) const;

	/** Returns the variant whose end is nearest to the pawn, or nullptr when none is set. */
	const FMKHMatchIntroVariant* FindVariant(const FVector& PawnLocation) const;

	/** Streams the exterior in (blocking), hides the HUD overlay and plays the variant's sequence. */
	void PlayIntro(const FMKHMatchIntroVariant& Variant);

	/** Sequence end: view back to the player, HUD back, exterior streamed out. */
	UFUNCTION()
	void HandleIntroFinished();

	/** Safety net after the sequence: if the local view target is not the pawn, sets it back to the pawn. */
	void RestorePlayerView() const;

	/** Flight starts: applies the skip input on the local controller and shows the skip hint. */
	void BeginSkipInput();

	/** Intro over: removes the skip input and the hint. */
	void EndSkipInput();

	/**
	 * Skip key held / released: forwards the held fraction to the hint.
	 * @param HoldFraction 0 (released) to 1 (skipping).
	 */
	void HandleSkipHoldChanged(float HoldFraction) const;

	/** Shows or hides the local HUD overlay. */
	void SetHUDOverlayHidden(bool bHideOverlay) const;

	/** Streams ExteriorLevel in (blocking, made visible) or out. */
	void SetExteriorLoaded(bool bLoaded);

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** One sequence per starting room (its end location tells which). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Intro")
	TArray<FMKHMatchIntroVariant> Variants;

	/** Streaming sub-level shown during the intro and unloaded after it (empty: none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Intro")
	FName ExteriorLevel = TEXT("Makhion_Exterior");

	/** Below this many seconds of initial preparation left (late join), the intro is not played. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Intro", meta = (ClampMin = "0"))
	float MinPhaseTimeLeft = 20.f;

	/** Seconds after BeginPlay after which the actor stops waiting (no pawn, not a duel map...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Intro", meta = (ClampMin = "1"))
	float WaitTimeout = 60.f;

	/** "Hold [key] to skip" hint shown while the intro flies (W_IntroSkipHint); none: the skip still works. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Intro")
	TSubclassOf<UMKHIntroSkipHintWidget> SkipHintClass;

	/** Viewport Z order of the skip hint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Intro")
	int32 SkipHintZOrder = 50;

private:
	/** Player of the running intro. */
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	/** Actor owning the player (destroyed at the end). */
	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	/** Skip hint on screen during the flight. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHIntroSkipHintWidget> SkipHint;

	/** Local controller whose skip input this intro turned on. */
	TWeakObjectPtr<AMKHPlayerController> SkipController;

	/** Bindings on SkipController's skip delegates. */
	FDelegateHandle SkipRequestedHandle;
	FDelegateHandle SkipHoldHandle;

	/** Seconds spent waiting so far. */
	float WaitedSeconds = 0.f;

	/** True once played (or given up): it plays once per level load, never on a rematch. */
	bool bDone = false;

	/** True while ExteriorLevel is streamed in by this actor. */
	bool bExteriorLoaded = false;

	/** True while the intro holds its first frame under the loading screen. */
	bool bHoldingFirstFrame = false;
};
