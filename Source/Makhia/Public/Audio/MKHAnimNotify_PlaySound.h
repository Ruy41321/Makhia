// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "MKHAnimNotify_PlaySound.generated.h"

/**
 * Anim notify that plays a sound effect of DT_SoundEffects by tag, attached to the animated mesh (at the row's Socket):
 * the "animation" route of the sound effects, for every sound that must land on a frame (attacks, hit and death
 * voices, dodge, drinking). It runs on every machine that plays the animation (montages replicate through the ability
 * system, the anim blueprint runs everywhere), so nothing is sent over the network. Also plays in the animation editor.
 *
 * Full model: see Documentation/AudioSystemArchitecture.md (Sound effects).
 */
UCLASS(meta = (DisplayName = "MKH Play Sound"))
class MAKHIA_API UMKHAnimNotify_PlaySound : public UAnimNotify
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

public:
	/**
	 * Plays SoundTag on the mesh, through UMKHSoundEffectSubsystem in game or loading the row on the spot in previews.
	 *
	 * @param MeshComp        Mesh playing the animation.
	 * @param Animation       Animation that owns the notify (unused).
	 * @param EventReference  Notify event being triggered (unused).
	 */
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** @return The sound tag without its "SFX." prefix (e.g. "Attack.Sword.Light"), shown on the timeline. */
	virtual FString GetNotifyName_Implementation() const override;

	/** @return The sound effect this notify plays. */
	UFUNCTION(BlueprintPure, Category = "MKH|Audio")
	FGameplayTag GetSoundTag() const { return SoundTag; }

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** Sound effect to play (row name of DT_SoundEffects). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MKH|Audio", meta = (Categories = "SFX"))
	FGameplayTag SoundTag;
};
