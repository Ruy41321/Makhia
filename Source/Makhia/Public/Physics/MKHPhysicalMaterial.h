// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "MKHPhysicalMaterial.generated.h"

class UMaterialInterface;

/**
 * Physical material carrying the cosmetic reactions of a surface to the characters walking on it: the footprint
 * decals left on sand and the footstep sound, both played by UMKHFootprintComponent. Assign it to the surface's
 * material (PhysMaterial).
 *
 * Full model: see Documentation/Footprints.md and Documentation/AudioSystemArchitecture.md (Sound effects).
 */
UCLASS(BlueprintType)
class MAKHIA_API UMKHPhysicalMaterial : public UPhysicalMaterial
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

public:
	/** Returns true if this surface keeps footprints (both decal materials set). */
	UFUNCTION(BlueprintPure, Category = "Footprints")
	bool HasFootprints() const;

	/**
	 * Returns the footprint decal material of one foot.
	 *
	 * @param bLeftFoot  True for the left foot, false for the right one.
	 * @return The decal material, or nullptr if the surface keeps no footprints.
	 */
	UFUNCTION(BlueprintPure, Category = "Footprints")
	UMaterialInterface* GetFootprintDecal(bool bLeftFoot) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Deferred decal stamped under a left foot (left boot print). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprints")
	TObjectPtr<UMaterialInterface> FootprintDecalLeft;

	/** Deferred decal stamped under a right foot (usually the left print mirrored). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprints")
	TObjectPtr<UMaterialInterface> FootprintDecalRight;

	/** Size of the print on the ground in cm: X = width, Y = length (heel to toe), as covered by the decal texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprints", meta = (ClampMin = "1.0"))
	FVector2D FootprintSize = FVector2D(18.0, 36.0);

	/** Depth of the decal projection box in cm (centred on the ground, keeps uneven ground covered). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprints", meta = (ClampMin = "1.0"))
	float FootprintProjectionDepth = 20.0f;

	/** Seconds a print stays fully visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprints", meta = (ClampMin = "0.0"))
	float FootprintHoldTime = 3.0f;

	/** Seconds a print takes to fade out after the hold time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprints", meta = (ClampMin = "0.0"))
	float FootprintFadeTime = 2.0f;

	/** Footstep sound effect of this surface (DT_SoundEffects row); empty = the footprint component's default footstep. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (Categories = "SFX.Foley.Footstep"))
	FGameplayTag FootstepSoundTag;
};
