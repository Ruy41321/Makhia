// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MKHLobbyCharacterPreview.generated.h"

class UAnimSequenceBase;
class UMKHSkinComponent;
class USkeletalMeshComponent;

/**
 * Lightweight character shown on a lobby platform: a skeletal mesh looping an idle animation, nothing else (no
 * movement, no abilities, no collision, never replicated). Each machine spawns its own through
 * AMKHLobbyPlayerPlatform. The Blueprint (BP_LobbyCharacterPreview) sets the default mesh and idle; the platform
 * dresses it with SetSkin. Full model: see Documentation/LobbyPresentation.md and CharacterSkins.md.
 */
UCLASS()
class MAKHIA_API AMKHLobbyCharacterPreview : public AActor
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Creates the root, the mesh (placed like the mesh of the player character on its capsule) and the skin component. */
	AMKHLobbyCharacterPreview();

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Dresses the character with a skin (loaded asynchronously; the idle keeps running).
	 *
	 * @param SkinId  Skin to show; unknown identifiers show the default skin.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Preview")
	void SetSkin(FName SkinId);

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Loops IdleAnimation on the mesh, from a random point when bRandomizeIdleStart. */
	void PlayIdle() const;

	/**
	 * Skin component callback: shows the mesh and restarts the idle if the mesh swap stopped it.
	 *
	 * @param SkinId  Skin now shown.
	 */
	void HandleSkinApplied(FName SkinId);

	// ============================================================
	// Properties
	// ============================================================

	/** Root of the preview, placed on the platform anchor (where the capsule centre of a character would be). */
	UPROPERTY(VisibleAnywhere, Category = "Lobby|Preview")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Character mesh; the Blueprint sets the asset. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Preview")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** Puts the skins on Mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Preview")
	TObjectPtr<UMKHSkinComponent> SkinComponent;

	/** Animation looped by the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Preview")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	/** Starts the idle at a random point, so two characters do not breathe in sync. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Preview")
	bool bRandomizeIdleStart = true;

private:
	/** True once a skin is on the mesh (the mesh stays hidden until then). */
	bool bSkinApplied = false;
};
