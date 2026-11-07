// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "MKHGameplayCueNotify_Burst.generated.h"

/**
 * Burst gameplay cue whose sound is a sound effect of DT_SoundEffects (SoundTag) instead of an asset in Burst Sounds:
 * the "visual effect" route of the sound effects. The sound belongs to the effect and plays where it appears (the
 * component the cue is attached to, else the cue location, else the target), wherever the cue is used.
 * Leave Burst Sounds empty on these cues.
 *
 * Full model: see Documentation/AudioSystemArchitecture.md (Sound effects).
 */
UCLASS(Blueprintable, Category = "GameplayCueNotify", meta = (DisplayName = "MKH GCN Burst", ShortTooltip = "Burst cue whose sound is a DT_SoundEffects tag."))
class MAKHIA_API UMKHGameplayCueNotify_Burst : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Spawns the burst effects, then plays SoundTag.
	 *
	 * @param MyTarget    Actor the cue runs on.
	 * @param Parameters  Cue parameters (attach component, location).
	 * @return The result of the burst.
	 */
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

	/**
	 * Plays SoundTag where the effect appears.
	 *
	 * @param MyTarget    Actor the cue runs on.
	 * @param Parameters  Cue parameters.
	 */
	void PlayEffectSound(AActor* MyTarget, const FGameplayCueParameters& Parameters) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Sound effect of this visual effect (row name of DT_SoundEffects); empty = no sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GCN Sound", meta = (Categories = "SFX"))
	FGameplayTag SoundTag;
};
