// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MKHFootprintComponent.generated.h"

class ACharacter;
class UDecalComponent;
class UMKHPhysicalMaterial;
class USkeletalMeshComponent;

/** Plant tracking of one foot. */
USTRUCT()
struct FMKHFootTrack
{
	GENERATED_BODY()

	/** True once the foot has lifted above the lift height: the next plant plays a footstep and stamps a print. */
	bool bArmed = false;
};

/**
 * Footprints and footsteps: detects each foot plant from the animated foot bones, plays the footstep sound effect of the
 * surface (UMKHPhysicalMaterial::FootstepSoundTag, else DefaultFootstepSoundTag) and stamps a fading decal on the
 * ground when its physical material keeps footprints (sand). Runs on every machine that renders (never on a dedicated
 * server), from the local animation: nothing is replicated.
 *
 * Full model: see Documentation/Footprints.md and Documentation/AudioSystemArchitecture.md (Sound effects).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHFootprintComponent : public UActorComponent
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Ticks after the animation update, so the foot bones are this frame's. */
	UMKHFootprintComponent();

	/** Turns the tick off where nothing is rendered (dedicated server). */
	virtual void BeginPlay() override;

	/** Updates both feet; each plant plays a footstep and stamps a print. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/**
	 * Arms the foot when it lifts and handles the plant when an armed foot comes down.
	 *
	 * @param Track      The foot's plant tracking.
	 * @param bLeftFoot  True for the left foot.
	 * @param Mesh       The character's skeletal mesh.
	 * @param GroundZ    World Z of the capsule bottom.
	 */
	void UpdateFoot(FMKHFootTrack& Track, bool bLeftFoot, const USkeletalMeshComponent& Mesh, double GroundZ);

	/**
	 * Traces under the foot, plays the surface's footstep and, on a footprint surface, spawns the fading decal aligned
	 * with the foot.
	 *
	 * @param bLeftFoot  True for the left foot.
	 * @param Mesh       The character's skeletal mesh.
	 */
	void HandleFootPlant(bool bLeftFoot, const USkeletalMeshComponent& Mesh);

	/**
	 * Traces down from above a point for the ground.
	 *
	 * @param Location  Point under which to look for the ground.
	 * @param OutHit    The ground hit (with its physical material).
	 * @return True when ground was found.
	 */
	bool TraceGround(const FVector& Location, FHitResult& OutHit) const;

	/**
	 * Plays the footstep sound effect of the surface under the foot.
	 *
	 * @param Surface   The surface's physical material when it is a UMKHPhysicalMaterial, else nullptr.
	 * @param Location  Where the foot landed.
	 */
	void PlayFootstep(const UMKHPhysicalMaterial* Surface, const FVector& Location) const;

	/**
	 * Spawns the decal of one print and keeps track of it.
	 *
	 * @param Surface    The surface's physical material (decal and timing).
	 * @param bLeftFoot  True for the left foot.
	 * @param Hit        The ground hit under the print centre.
	 * @param Forward    Heel-to-toe direction of the foot.
	 */
	void SpawnFootprintDecal(const UMKHPhysicalMaterial& Surface, bool bLeftFoot, const FHitResult& Hit, const FVector& Forward);

	/** Drops the expired prints and destroys the oldest ones above MaxFootprints. */
	void TrimFootprints();

	/** Returns the owning character, or nullptr. */
	ACharacter* GetOwnerCharacter() const;

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** Foot bone of the left foot (plant detection, heel side of the print; Gideon's sits at the heel, at sole level). */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints")
	FName LeftFootBone = TEXT("foot_l");

	/** Ball bone of the left foot (toe side of the print). */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints")
	FName LeftBallBone = TEXT("ball_l");

	/** Foot bone of the right foot. */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints")
	FName RightFootBone = TEXT("foot_r");

	/** Ball bone of the right foot. */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints")
	FName RightBallBone = TEXT("ball_r");

	/** Foot bone height above the capsule bottom (cm) under which an armed foot counts as planted (Gideon: ~0 planted). */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints", meta = (ClampMin = "0.0"))
	float PlantHeight = 5.0f;

	/** Foot bone height above the capsule bottom (cm) over which the foot counts as lifted (arms the next plant). */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints", meta = (ClampMin = "0.0"))
	float LiftHeight = 12.0f;

	/** Print centre along foot bone → ball (0 = foot bone, 1 = ball): matches the sole drawn on the decal texture. */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	float PrintCentreAlpha = 0.7f;

	/** Ground trace starts this far above the print centre (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints", meta = (ClampMin = "0.0"))
	float TraceUp = 30.0f;

	/** Ground trace ends this far below the print centre (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints", meta = (ClampMin = "0.0"))
	float TraceDown = 60.0f;

	/** Most prints alive at once for this character; the oldest goes first. */
	UPROPERTY(EditDefaultsOnly, Category = "Footprints", meta = (ClampMin = "1"))
	int32 MaxFootprints = 24;

	/** Footstep sound effect on surfaces whose physical material names none (DT_SoundEffects row). */
	UPROPERTY(EditDefaultsOnly, Category = "Footsteps", meta = (Categories = "SFX.Foley.Footstep"))
	FGameplayTag DefaultFootstepSoundTag;

private:
	/** Left foot plant tracking. */
	FMKHFootTrack LeftFoot;

	/** Right foot plant tracking. */
	FMKHFootTrack RightFoot;

	/** Prints alive, oldest first. */
	TArray<TWeakObjectPtr<UDecalComponent>> ActiveFootprints;
};
