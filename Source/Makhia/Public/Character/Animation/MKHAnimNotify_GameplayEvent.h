// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "MKHAnimNotify_GameplayEvent.generated.h"

/**
 * Anim notify that sends a gameplay event (EventTag) to the owner of the animated mesh, when it has an ability system:
 * abilities wait for these events to time their strikes (Event.Animation.HitScan.Start, .SpawnProjectile) and combo
 * windows (.ContinueCombo.Start/End). Base class of the AN_* notify Blueprints, which only set EventTag.
 * Being native, the event tag of every notify in a montage can be read without playing it: the duel bot reads the
 * strike times this way (UMKHDataSubsystem::FindAbilityTiming).
 *
 * Full model: see Documentation/AbilityTuning.md (Timing).
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "MKH Gameplay Event"))
class MAKHIA_API UMKHAnimNotify_GameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

public:
	/**
	 * Sends EventTag to the mesh owner. Super is not called on purpose: it would also run the Blueprint
	 * Received_Notify graph, which used to send the same event.
	 *
	 * @param MeshComp        Mesh playing the animation.
	 * @param Animation       Animation (usually a montage) that owns the notify.
	 * @param EventReference  Notify event being triggered (unused).
	 */
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** @return The event tag without its "Event.Animation." prefix (e.g. "HitScan.Start"), shown on the timeline. */
	virtual FString GetNotifyName_Implementation() const override;

	/** @return The gameplay event this notify sends. */
	UFUNCTION(BlueprintPure, Category = "MKH|Animation")
	FGameplayTag GetEventTag() const { return EventTag; }

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** Gameplay event sent to the mesh owner's ability system when the notify triggers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MKH|Animation", meta = (Categories = "Event.Animation"))
	FGameplayTag EventTag;
};
