// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MKHLobbyBackdrop.generated.h"

class UDirectionalLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMKHLobbyBackdropDefinition;
class UStaticMeshComponent;

/**
 * Lobby background: an unlit picture behind the players that always fills the screen, plus two shadowless
 * lights (key and rim) tinted to match it. Purely cosmetic and local: every machine fits it to its own camera.
 *
 * The picture plane is re-fitted to the local camera whenever the view, the field of view or the aspect ratio
 * changes, and the picture is cropped (never stretched) around the definition's Focus. In the editor the actor
 * previews the fit as if the camera were at the actor (place it on the lobby PlayerStart).
 * Full model: see Documentation/LobbyPresentation.md.
 */
UCLASS()
class MAKHIA_API AMKHLobbyBackdrop : public AActor
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Creates the picture plane and the two lights. */
	AMKHLobbyBackdrop();

	//~ Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows another background (e.g. for an event) until the lobby is reloaded.
	 *
	 * @param NewBackdrop  Background to show; null goes back to the configured one (BackdropOverride, else the project settings).
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Backdrop")
	void SetBackdrop(UMKHLobbyBackdropDefinition* NewBackdrop);

	/** Returns the background currently shown (null when none is configured). */
	UFUNCTION(BlueprintPure, Category = "Lobby|Backdrop")
	UMKHLobbyBackdropDefinition* GetBackdrop() const { return ActiveBackdrop; }

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns BackdropOverride, or the background of the project settings (loaded on demand). */
	UMKHLobbyBackdropDefinition* ResolveBackdrop() const;

	/** Pushes the active background into the material and the lights. */
	void ApplyBackdrop();

	/** Creates the dynamic material instance of the plane if needed; returns it (null without BackdropMaterial). */
	UMaterialInstanceDynamic* GetOrCreateMaterialInstance();

	/**
	 * Reads the local player's camera.
	 *
	 * @param OutLocation      Camera location.
	 * @param OutRotation      Camera rotation.
	 * @param OutHalfTangents  Tangents of the horizontal and vertical half field of view.
	 * @return False when there is no local player with a viewport (dedicated server, not started yet).
	 */
	bool ReadLocalView(FVector& OutLocation, FRotator& OutRotation, FVector2D& OutHalfTangents) const;

	/** Editor preview view: camera at the actor, EditorPreviewFOV and EditorPreviewAspectRatio. */
	void ReadPreviewView(FVector& OutLocation, FRotator& OutRotation, FVector2D& OutHalfTangents) const;

	/** Places and scales the plane so it covers the view at Distance, then updates the crop. */
	void FitToView(const FVector& ViewLocation, const FRotator& ViewRotation, const FVector2D& HalfTangents);

	/**
	 * Computes the UV scale (xy) and offset (zw) of the picture: cover crop to the view aspect ratio around Focus,
	 * plus the horizon alignment of the definition.
	 *
	 * @param HalfTangents  Tangents of the horizontal and vertical half field of view.
	 * @param ViewPitch     Camera pitch in degrees (positive looks up), which moves the real horizon on screen.
	 */
	FLinearColor ComputeImageCrop(const FVector2D& HalfTangents, float ViewPitch) const;

	// ============================================================
	// Properties - Components
	// ============================================================

	/** Root; the lights keep their rotation relative to it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Plane showing the picture; placed in world space in front of the camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BackdropMesh;

	/** Main light on the players (movable, no shadows). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	/** Back light outlining the players (movable, no shadows). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> RimLight;

	// ============================================================
	// Properties - Settings
	// ============================================================

	/** Unlit material of the plane; needs the parameters Image, Tint, Brightness and UVTransform. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Backdrop")
	TObjectPtr<UMaterialInterface> BackdropMaterial;

	/** Background shown instead of the project setting (for testing a picture in this level only). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Lobby|Backdrop")
	TObjectPtr<UMKHLobbyBackdropDefinition> BackdropOverride;

	/** Distance of the plane from the camera; must stay behind the players. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Backdrop", meta = (ClampMin = "100.0", Units = "cm"))
	float Distance = 2500.f;

	/** Extra size of the plane over the exact view, so no edge ever shows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Backdrop", meta = (ClampMin = "1.0"))
	float Overscan = 1.02f;

	/** Horizontal field of view of the editor preview (the lobby camera's FOV). */
	UPROPERTY(EditAnywhere, Category = "Lobby|Backdrop|Editor Preview", meta = (ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float EditorPreviewFOV = 90.f;

	/** Aspect ratio of the editor preview. */
	UPROPERTY(EditAnywhere, Category = "Lobby|Backdrop|Editor Preview", meta = (ClampMin = "0.5"))
	float EditorPreviewAspectRatio = 16.f / 9.f;

private:
	/** Background currently shown. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHLobbyBackdropDefinition> ActiveBackdrop;

	/** Dynamic instance of BackdropMaterial on the plane. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	/** Camera location of the last fit; the fit runs again only when the view changes. */
	FVector FittedLocation = FVector::ZeroVector;

	/** Camera rotation of the last fit. */
	FRotator FittedRotation = FRotator::ZeroRotator;

	/** Half field of view tangents of the last fit (zero before the first one). */
	FVector2D FittedHalfTangents = FVector2D::ZeroVector;
};
