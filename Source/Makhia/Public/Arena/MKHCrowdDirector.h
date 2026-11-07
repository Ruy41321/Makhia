// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MKHCrowdDirector.generated.h"

class ACharacterBase;
class AGameStateBase;
class AMKHGameState;
class UMaterialParameterCollection;
class UMKHCrowdAudioComponent;
enum class EMKHCrowdReaction : uint8;
namespace Scalability { struct FQualityLevels; }

/**
 * Drives how excited the arena crowd is from the duel: hits, deaths, duel start, sudden death and match end.
 *
 * One per arena level. Every peer runs its own director from replicated data (AMKHGameState events and the
 * participants' replicated Health), so nothing of it replicates. Each event raises an excitement level that holds,
 * then decays down to MinExcitement; the displayed value eases toward it and is written to a material parameter collection that the
 * crowd material (M_MKN_Crowd) reads to blend each spectator from its idle clip to its excited clip.
 * The same events drive the crowd sounds through CrowdAudio (sounds in Project Settings -> Makhia Audio -> Crowd).
 * It also thins the crowd on lower Effects quality: the crowd actor's components carry a density tier tag
 * (CrowdTier0..2) and a tier shows only from the matching Effects level on (Low = tier 0, about a third).
 * Full model: see Documentation/MatchFlowArchitecture.md ("Crowd reactions").
 */
UCLASS()
class MAKHIA_API AMKHCrowdDirector : public AActor
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Ticks only while the crowd calms down or gets excited. */
	AMKHCrowdDirector();

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Raises the crowd excitement to at least this level and holds it for HoldSeconds before it decays.
	 *
	 * @param Level  Excitement in [0, 1] (1 = every spectator plays its excited clip).
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena|Crowd")
	void AddExcitement(float Level);

	/** Excitement currently written to the material parameter collection. */
	UFUNCTION(BlueprintPure, Category = "Arena|Crowd")
	float GetExcitement() const { return DisplayedExcitement; }

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Binds to the game state's duel events and to the registered participants (UWorld::GameStateSetEvent on clients). */
	void BindGameState(AGameStateBase* GameState);

	/** Removes every game state and participant binding. */
	void UnbindAll();

	/**
	 * Listens to a participant's health (idempotent).
	 *
	 * @param Participant  Duel participant to watch.
	 */
	void BindParticipant(ACharacterBase* Participant);

	/** OnParticipantRegistered handler: watches the new participant's health. */
	UFUNCTION()
	void HandleParticipantRegistered(int32 SlotIndex, ACharacterBase* Character);

	/** OnHealthChanged handler of a participant: a hit during the duel excites the crowd by the damage dealt. */
	UFUNCTION()
	void HandleHealthChanged(float OldHealth, float CurrentHealth, float MaxHealth);

	/** OnDuelStarted handler. */
	UFUNCTION()
	void HandleDuelStarted();

	/** OnDuelTimeOver handler (sudden death begins). */
	UFUNCTION()
	void HandleDuelTimeOver();

	/** OnPlayerDied handler. */
	UFUNCTION()
	void HandlePlayerDied(ACharacterBase* DeadPlayer, ACharacterBase* ScoringPlayer, int32 ScoringScore);

	/** OnMatchOver handler. */
	UFUNCTION()
	void HandleMatchOver(ACharacterBase* WinningPlayer, bool bIsDraw);

	/**
	 * Excitement caused by a health loss, or 0 when it is too small to count (sudden-death drain ticks).
	 *
	 * @param Damage     Health lost.
	 * @param MaxHealth  Participant's maximum health.
	 * @return Excitement level in [0, 1].
	 */
	float ComputeHitExcitement(float Damage, float MaxHealth) const;

	/**
	 * Plays a crowd reaction sound (silent while its slot in the audio settings is empty).
	 *
	 * @param Reaction   Which reaction.
	 * @param Intensity  [0, 1], scales hit reactions.
	 */
	void PlayCrowdReaction(EMKHCrowdReaction Reaction, float Intensity = 1.f) const;

	/** True while a duel is being fought (Duel or SuddenDeath phase). */
	bool IsDuelRunning() const;

	/**
	 * Advances the held / decaying level and eases the displayed value toward it.
	 *
	 * @param DeltaSeconds  Frame time.
	 */
	void UpdateExcitement(float DeltaSeconds);

	/** Writes DisplayedExcitement to the crowd material parameter collection and the excited crowd loop. */
	void PushExcitement() const;

	/**
	 * Scalability change handler: shows the density tiers allowed by the new Effects quality.
	 *
	 * @param QualityLevels  The applied scalability levels.
	 */
	void HandleScalabilityChanged(const Scalability::FQualityLevels& QualityLevels);

	/**
	 * Shows the crowd components whose density tier is at most EffectsQuality and hides the others.
	 *
	 * @param EffectsQuality  Effects scalability level (0 = Low .. 3 = Epic).
	 */
	void ApplyCrowdDensity(int32 EffectsQuality) const;

	/**
	 * Density tier of a crowd component from its CrowdTier<N> tag.
	 *
	 * @param Component  Component of the crowd actor.
	 * @return The tier, or INDEX_NONE when the component has no tier tag (always shown).
	 */
	static int32 GetDensityTier(const UActorComponent* Component);

	// ============================================================
	// Properties
	// ============================================================

	/** Collection read by the crowd material (MPC_MKN_Crowd). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd")
	TObjectPtr<UMaterialParameterCollection> CrowdCollection;

	/** Plays the crowd loops and reactions; the excited loop follows the displayed excitement. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Crowd")
	TObjectPtr<UMKHCrowdAudioComponent> CrowdAudio;

	/** Actor holding the instanced crowd (MKN_Crowd); its components tagged CrowdTier<N> are thinned on low Effects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd")
	TObjectPtr<AActor> CrowdActor;

	/** Scalar parameter of CrowdCollection that blends idle and excited clips. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd")
	FName ExcitementParameter = TEXT("Excitement");

	/** Floor the excitement never decays below, from BeginPlay on (the crowd keeps a base level of cheering). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0", ClampMax = "1"))
	float MinExcitement = 0.25f;

	/** Excitement when a duel starts (the gates open). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0", ClampMax = "1"))
	float DuelStartExcitement = 0.8f;

	/** Excitement when the duel timer runs out and sudden death begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0", ClampMax = "1"))
	float SuddenDeathExcitement = 0.6f;

	/** Excitement when a participant dies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0", ClampMax = "1"))
	float DeathExcitement = 1.f;

	/** Excitement when the match is decided. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0", ClampMax = "1"))
	float MatchOverExcitement = 1.f;

	/** Excitement of the smallest hit that counts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0", ClampMax = "1"))
	float MinHitExcitement = 0.25f;

	/** Extra excitement per fraction of maximum health lost (0.2 of max health x 2.5 = +0.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0"))
	float HitExcitementPerHealthFraction = 2.5f;

	/** Health losses below this fraction of maximum health are ignored (sudden-death drain ticks). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0", ClampMax = "1"))
	float MinHitHealthFraction = 0.04f;

	/** Seconds an excitement level holds before it starts to decay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0"))
	float HoldSeconds = 1.5f;

	/** Excitement lost per second once the hold is over (1 / DecayPerSecond seconds from full to calm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0.01"))
	float DecayPerSecond = 0.25f;

	/** Excitement gained per second by the displayed value while it catches up (no snap into the excited clips). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Crowd", meta = (ClampMin = "0.01"))
	float RisePerSecond = 3.f;

private:
	/** Game state whose events drive the crowd. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AMKHGameState> BoundGameState;

	/** Participants whose health is watched. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ACharacterBase>> BoundParticipants;

	/** Subscription to UWorld::GameStateSetEvent while waiting for the game state to replicate. */
	FDelegateHandle GameStateSetHandle;

	/** Subscription to Scalability::OnScalabilitySettingsChanged. */
	FDelegateHandle ScalabilityHandle;

	/** Level raised by the events, held then decaying. */
	float TargetExcitement = 0.f;

	/** Seconds left before TargetExcitement starts to decay. */
	float HoldRemaining = 0.f;

	/** Value written to the collection, easing toward TargetExcitement. */
	float DisplayedExcitement = 0.f;
};
