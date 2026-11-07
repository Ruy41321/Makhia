// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MKHArenaGate.generated.h"

class AGameStateBase;
class AMKHGameState;
class UAudioComponent;
class UBoxComponent;
class USoundAttenuation;
class USoundBase;
class UStaticMeshComponent;
enum class EMKHMatchPhase : uint8;

/**
 * Portcullis that closes a preparation room: an iron grille that rises when each duel starts.
 *
 * Every peer drives its own gate from the replicated match phase of AMKHGameState (nothing of the gate replicates):
 * closed during the preparation phases, rising from the start of the duel. GrilleCollision moves with the grille,
 * so a participant can walk out as soon as its feet clear their head. Its sounds (rise, raised, drop, slam) come
 * from UMKHAudioSettings ("Arena Gates").
 * Full model: see Documentation/MatchFlowArchitecture.md ("Arena gates").
 */
UCLASS()
class MAKHIA_API AMKHArenaGate : public AActor
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Creates the grille and its collision box; ticks only while the grille moves. */
	AMKHArenaGate();

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Starts raising (true) or lowering (false) the grille, and its collision, from where it is.
	 *
	 * @param bOpen     True to raise the grille.
	 * @param bInstant  True to snap the grille to its end position (gate joining a phase already under way).
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena|Gate")
	void SetGateOpen(bool bOpen, bool bInstant = false);

	/** True while the grille is raised or rising. */
	UFUNCTION(BlueprintPure, Category = "Arena|Gate")
	bool IsGateOpen() const { return bTargetOpen; }

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Binds to the game state's phase events and applies the current phase (UWorld::GameStateSetEvent on clients). */
	void BindGameState(AGameStateBase* GameState);

	/** Removes the phase-event bindings and the pending game state subscription. */
	void UnbindGameState();

	/** OnMatchPhaseChanged handler: applies the new phase. */
	UFUNCTION()
	void HandleMatchPhaseChanged(EMKHMatchPhase NewPhase);

	/**
	 * Opens or closes the gate for the bound game state's current phase.
	 *
	 * @param bInstant  True to snap the grille instead of animating it.
	 */
	void ApplyMatchState(bool bInstant);

	/**
	 * Grille height for a travel fraction: smooth start and stop when rising, accelerating fall when lowering.
	 *
	 * @param Alpha  0 = closed, 1 = open.
	 * @return Height of the grille above its closed position, in cm.
	 */
	float ComputeLift(float Alpha) const;

	/**
	 * Travel fraction at which ComputeLift (for the current direction) gives this height, so that reversing the
	 * grille mid-travel does not make it jump.
	 *
	 * @param Lift  Current height of the grille above its closed position, in cm.
	 * @return Matching travel fraction in [0, 1].
	 */
	float FindAlphaForLift(float Lift) const;

	/** Moves the grille (and GrilleCollision with it) to the current OpenAlpha. */
	void UpdateGrillePosition() const;

	/** Loads the gate sound slots of UMKHAudioSettings (empty slots stay null); nothing on a dedicated server. */
	void LoadGateSounds();

	/**
	 * Plays one gate sound at the grille, moving with it.
	 *
	 * @param Sound       Loaded slot (null = silent).
	 * @param bTravelling True for the rise / drop sound, which is cut short when the grille reverses.
	 */
	void PlayGateSound(USoundBase* Sound, bool bTravelling);

	/** Fades out the rise / drop sound still playing (the grille turned back). */
	void StopTravelSound();

	// ============================================================
	// Properties
	// ============================================================

	/** Scene root at the foot of the grille, in the middle of the passage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Gate")
	TObjectPtr<USceneComponent> GateRoot;

	/** The moving grille. Its mesh is set in the Blueprint (SM_MKN_Portcullis, no collision of its own). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Gate")
	TObjectPtr<UStaticMeshComponent> GateMesh;

	/** Collision of the grille: a slab attached to GateMesh that rises with it; size it to the grille. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Gate")
	TObjectPtr<UBoxComponent> GrilleCollision;

	/** How high the grille rises (cm); its feet must clear the arch crown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Gate", meta = (ClampMin = "0"))
	float LiftHeight = 440.f;

	/** Seconds to rise fully, from the start of the duel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Gate", meta = (ClampMin = "0.05"))
	float OpenDuration = 2.5f;

	/** Seconds to drop fully. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Gate", meta = (ClampMin = "0.05"))
	float CloseDuration = 1.2f;

private:
	/** Game state whose phase events drive the gate. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AMKHGameState> BoundGameState;

	/** Gate sounds loaded from UMKHAudioSettings at BeginPlay (null when the slot is empty). */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> RaiseSound;

	/** See RaiseSound. */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> RaisedSound;

	/** See RaiseSound. */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LowerSound;

	/** See RaiseSound. */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SlamSound;

	/** Attenuation applied to every gate sound (null = each sound's own). */
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	/** The rise / drop sound currently playing, stopped if the grille reverses. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TravelAudio;

	/** Subscription to UWorld::GameStateSetEvent while waiting for the game state to replicate. */
	FDelegateHandle GameStateSetHandle;

	/** Travel fraction of the grille: 0 = closed, 1 = open. */
	float OpenAlpha = 0.f;

	/** Where the grille is heading. */
	bool bTargetOpen = false;
};
