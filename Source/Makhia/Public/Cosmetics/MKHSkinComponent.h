// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MKHSkinComponent.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FStreamableHandle;
struct FMKHSkinDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FMKHSkinAppliedSignature, FName /*SkinId*/);

/**
 * Puts a skin on the skeletal mesh of its owner (the character mesh of an ACharacter, otherwise the first
 * skeletal mesh component): loads the skin's meshes asynchronously, swaps the mesh without restarting the
 * animation and attaches the skin's accessories. Local and never replicated: whoever owns the skin identifier
 * (lobby platform, player state) calls SetSkin on every machine. Full model: see Documentation/CharacterSkins.md.
 */
UCLASS(ClassGroup = (Makhia), meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHSkinComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Local component: no tick, no replication. */
	UMKHSkinComponent();

	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows a skin; unknown identifiers show the default skin. Does nothing when that skin is already shown or
	 * loading. The swap happens once the skin's meshes are loaded.
	 *
	 * @param SkinId  Skin to show.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics|Skin")
	void SetSkin(FName SkinId);

	/** Returns the skin shown (or being loaded), None before the first SetSkin. */
	UFUNCTION(BlueprintPure, Category = "Cosmetics|Skin")
	FName GetSkinId() const { return RequestedSkinId; }

	/** Broadcast once a skin is on the mesh. */
	FMKHSkinAppliedSignature OnSkinApplied;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Swaps the mesh and the accessories; the skin's assets must be loaded.
	 *
	 * @param SkinId  Skin to apply (looked up again: the catalog may not outlive the load).
	 */
	void ApplyLoadedSkin(FName SkinId);

	/**
	 * Replaces the accessories with the ones of a skin.
	 *
	 * @param Skin  Skin whose accessories to attach.
	 * @param Mesh  Character mesh the accessories follow.
	 */
	void RebuildAccessories(const FMKHSkinDefinition& Skin, USkeletalMeshComponent* Mesh);

	/** Destroys the accessories of the previous skin. */
	void DestroyAccessories();

	/** Returns the mesh to dress: the owner's character mesh, otherwise its first skeletal mesh component. */
	USkeletalMeshComponent* ResolveTargetMesh() const;

	/**
	 * Collects the asset paths a skin needs (mesh and accessories).
	 *
	 * @param Skin  Skin to load.
	 */
	static TArray<FSoftObjectPath> CollectSkinAssets(const FMKHSkinDefinition& Skin);

	// ============================================================
	// Properties
	// ============================================================

	/** Accessories spawned for the current skin. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Accessories;

	/** Skin shown or being loaded. */
	FName RequestedSkinId;

	/** Load in flight for RequestedSkinId, cancelled when another skin is asked first. */
	TSharedPtr<FStreamableHandle> LoadHandle;
};
