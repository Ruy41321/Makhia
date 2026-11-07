// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MKHCrowdAudioComponent.generated.h"

class UAudioComponent;
class USoundBase;

/** Crowd reaction one-shots (sounds in UMKHAudioSettings, "Crowd|Reactions"). */
UENUM(BlueprintType)
enum class EMKHCrowdReaction : uint8
{
	DuelStart,
	Hit,
	SuddenDeath,
	Death,
	MatchOver
};

/**
 * Plays the arena crowd: an ambience loop, a cheering loop whose volume follows the crowd excitement, and reaction
 * one-shots. Every sound comes from UMKHAudioSettings (Project Settings -> Game -> Makhia Audio -> Crowd); an empty
 * slot stays silent, so the component works before the sounds exist. Local only (each peer hears its own crowd),
 * non-spatialized, routed to the crowd submix (SBM_Crowd, set as Base Submix on each crowd sound).
 * Owned by AMKHCrowdDirector, which calls SetExcitement and PlayReaction. See Documentation/AudioSystemArchitecture.md.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHCrowdAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	UMKHCrowdAudioComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Sets the volume of the excited loop from the crowd excitement.
	 *
	 * @param Excitement  Crowd excitement in [0, 1].
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Crowd")
	void SetExcitement(float Excitement);

	/**
	 * Plays a reaction one-shot (no-op when its slot is empty; hits respect the hit cooldown).
	 *
	 * @param Reaction   Which reaction.
	 * @param Intensity  [0, 1]; scales hit reactions between 60 % and 100 % of the reaction volume.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Crowd")
	void PlayReaction(EMKHCrowdReaction Reaction, float Intensity = 1.f);

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Starts a looping sound from a settings slot as a 2D audio component.
	 *
	 * @param Sound          The settings slot.
	 * @param Volume         Starting volume multiplier.
	 * @param FadeInSeconds  Fade-in duration.
	 * @return The playing component, or nullptr when the slot is empty.
	 */
	UAudioComponent* StartLoop(const TSoftObjectPtr<USoundBase>& Sound, float Volume, float FadeInSeconds);

	/**
	 * Settings slot of a reaction.
	 *
	 * @param Reaction  Which reaction.
	 * @return The slot (may be empty).
	 */
	static const TSoftObjectPtr<USoundBase>& GetReactionSlot(EMKHCrowdReaction Reaction);

	/**
	 * Logs a warning when a crowd sound is not routed to the crowd submix (it would play under the SFX slider).
	 *
	 * @param Sound  A loaded crowd sound.
	 */
	static void WarnIfNotCrowdRouted(const USoundBase* Sound);

	// ============================================================
	// Properties
	// ============================================================

private:
	/** Playing ambience loop (null when the slot is empty). */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbienceAudio;

	/** Playing excited loop (null when the slot is empty). */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ExcitedAudio;

	/** World time of the last hit reaction, for the hit cooldown. */
	double LastHitSoundTime = -1.0e9;
};
