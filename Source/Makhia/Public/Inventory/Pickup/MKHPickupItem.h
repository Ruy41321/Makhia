// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Inventory/MKHItemEntry.h"
#include "Inventory/Pickup/MKHPickupAnimationTypes.h"
#include "MKHPickupItem.generated.h"

class APawn;
class APlayerState;
class UMKHItemPickupWidget;
class UMKHPickupAnimationComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;
struct FCollisionQueryParams;
struct FStreamableHandle;

/**
 * World representation of one item lying on the ground. Replicated and server-authoritative:
 * only the server spawns it (SpawnGeneratedPickup / SpawnPickupFromItem, used by inventory drops and by
 * AMKHItemSpawnPoint, which is how items are placed in levels); the replicated Item drives the visuals on every machine.
 * The pickup mesh and its relative transform come from the item definition; rarity color, widget and VFX
 * are applied by the Blueprint in OnPickupItemInitialized. Players collect it through
 * AMKHPlayerController::Server_PickupItem, after aiming at it (UMKHPickupInteractionComponent); an exclusive
 * pickup can only be collected by one player.
 * Appearing (drop, rise from the ground) and being collected are animated on every machine by
 * UMKHPickupAnimationComponent, which only moves VisualRoot: the actor stays at its resting place.
 *
 * Full model: see Documentation/PickupSystemArchitecture.md.
 */
UCLASS()
class MAKHIA_API AMKHPickupItem : public AActor
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Creates the root, visual root, pickup mesh, interaction sphere and animation components and enables replication. */
	AMKHPickupItem();

	/** Registers Item, the collection rights and the animation state for replication. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Server: registers with UMKHItemSubsystem. Then starts the appearance animation and applies the item visuals. */
	virtual void BeginPlay() override;

	/** Cancels any pending mesh load and unregisters the pickup from UMKHItemSubsystem (server). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Server only: generates a brand new item (fresh ItemID, rolled rarity and stats) and spawns it
	 * in the world as a pickup.
	 *
	 * @param WorldContextObject Any object living in the world to spawn into.
	 * @param ItemTag            Tag of the item definition to instantiate.
	 * @param Quantity           Stack size of the new item (must be at least 1).
	 * @param SpawnTransform     World transform of the spawned pickup.
	 * @param ExclusiveCollector When set, only this player may collect the pickup; nullptr lets anyone collect it.
	 * @param Appearance         How the pickup appears (see SpawnPickupFromItem).
	 * @param DroppedBy          Pawn the item is thrown from; required by the Drop appearance.
	 * @return The spawned pickup, or nullptr without authority or when the item cannot be generated.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Pickup",
		meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "ExclusiveCollector,Appearance,DroppedBy"))
	static AMKHPickupItem* SpawnGeneratedPickup(const UObject* WorldContextObject, const FGameplayTag& ItemTag, int32 Quantity,
		const FTransform& SpawnTransform, APlayerState* ExclusiveCollector = nullptr,
		EMKHPickupAppearance Appearance = EMKHPickupAppearance::Instant, APawn* DroppedBy = nullptr);

	/**
	 * Server only: spawns an existing item (keeping its ItemID and rolls) in the world as a pickup.
	 * The caller is responsible for removing the item from wherever it was stored.
	 * An animated appearance plays on every machine; the pickup cannot be collected until it has ended.
	 *
	 * @param WorldContextObject Any object living in the world to spawn into.
	 * @param InItem             The item the pickup will hold; must be valid.
	 * @param SpawnTransform     World transform of the spawned pickup (its resting place).
	 * @param ExclusiveCollector When set, only this player may collect the pickup; nullptr lets anyone collect it.
	 * @param Appearance         Instant, Drop (thrown from DroppedBy) or RiseFromGround.
	 * @param DroppedBy          Pawn the item is thrown from; a Drop without it appears instantly.
	 * @return The spawned pickup, or nullptr without authority or with an invalid item.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Pickup",
		meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "ExclusiveCollector,Appearance,DroppedBy"))
	static AMKHPickupItem* SpawnPickupFromItem(const UObject* WorldContextObject, const FMKHItemEntry& InItem,
		const FTransform& SpawnTransform, APlayerState* ExclusiveCollector = nullptr,
		EMKHPickupAppearance Appearance = EMKHPickupAppearance::Instant, APawn* DroppedBy = nullptr);

	/**
	 * Finds where a pickup placed from Origin towards Target comes to rest: the horizontal trace stops
	 * WallClearance short of anything blocking on the way, then the point drops to the ground below.
	 * Shared by inventory drops and item spawn points.
	 *
	 * @param World               World to trace in.
	 * @param Origin              Start of the horizontal trace (e.g. the dropping pawn).
	 * @param Target              Desired landing point before collisions.
	 * @param WallClearance       Gap kept from a blocking hit between Origin and Target.
	 * @param GroundTraceDistance How far below the point the ground is searched for.
	 * @param FallbackZ           Height used when no ground is found.
	 * @param QueryParams         Trace parameters (ignored actors).
	 * @return The resting location.
	 */
	static FVector FindRestLocation(const UWorld& World, const FVector& Origin, const FVector& Target, float WallClearance,
		float GroundTraceDistance, float FallbackZ, const FCollisionQueryParams& QueryParams);

	/** Gets the item held by this pickup. */
	const FMKHItemEntry& GetItem() const { return Item; }

	/** Gets the component displaying the item's pickup mesh. */
	UStaticMeshComponent* GetPickupMesh() const { return PickupMesh; }

	/** Gets the component moved by the pickup animations (parent of the mesh and of the visuals that follow it). */
	USceneComponent* GetVisualRoot() const { return VisualRoot; }

	/** Gets the component playing the pickup animations. */
	UMKHPickupAnimationComponent* GetPickupAnimation() const { return PickupAnimation; }

	/** Gets the sphere defining the range in which a pawn can pick this item up. */
	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }

	// --- Animation ---

	/**
	 * Server only: starts the collection, called once the item is in the collector's inventory. Every
	 * machine plays the collection animation towards the collector; the pickup can no longer be interacted
	 * with and the server destroys it once the animation is over (UMKHPickupAnimationComponent::GetCollectDestroyDelay).
	 *
	 * @param Collector Pawn of the collecting player (the animation target); may be nullptr.
	 */
	void StartCollect(APawn* Collector);

	/** Whether the pickup has been collected and is only waiting for its animation to end (replicated). */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	bool IsBeingCollected() const { return AnimationState.Type == EMKHPickupAnimationType::Collect; }

	/** Whether an animation (drop, rise, collection) is playing on this machine. */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	bool IsPlayingPickupAnimation() const;

	/**
	 * Local view: whether players on this machine may interact with the pickup now — not while an animation
	 * plays and never once collected. Drives the local selection and the interaction widgets.
	 */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	bool IsInteractable() const;

	/**
	 * Server only: whether the appearance animation (drop, rise) is over, so the pickup may be collected.
	 * Measured on the server clock, which clients never run ahead of.
	 */
	bool HasFinishedAppearing() const;

	// --- Collection rights ---

	/**
	 * Whether a player may collect this pickup: always for a free pickup; for an exclusive one only its
	 * exclusive collector, and nobody once that player has left the game.
	 *
	 * @param Collector Player state of the player trying to collect.
	 * @return True when the player is allowed to collect this pickup.
	 */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	bool CanBeCollectedBy(const APlayerState* Collector) const;

	/** Whether the first local player of this machine may collect this pickup (see CanBeCollectedBy); drives local UI. */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	bool IsCollectibleByLocalPlayer() const;

	/** Whether only one player may collect this pickup. */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	bool IsExclusive() const { return bExclusive; }

	/** Gets the only player allowed to collect this pickup; nullptr for a free pickup or once that player has left. */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	APlayerState* GetExclusiveCollector() const { return ExclusiveCollector; }

	// --- Interaction ---

	/**
	 * Tests whether a pawn is within the interaction range: InteractionSphere against the pawn's upright
	 * collision capsule, i.e. the same result as an overlap, computed geometrically so the local selection
	 * and the server validation always apply the same rule.
	 *
	 * @param Pawn       The pawn to test.
	 * @param ExtraReach Margin added to the sphere radius (the server uses it to absorb movement lag).
	 * @return True when the capsule touches the (enlarged) sphere.
	 */
	bool IsPawnInInteractionRange(const APawn& Pawn, float ExtraReach = 0.f) const;

	/** Gets the point players aim at to focus this pickup: the center of the displayed mesh, or the actor location without one. */
	FVector GetInteractionFocusPoint() const;

	/**
	 * Server only: claims this pickup for collection, so it is granted at most once even if several
	 * requests reach the server before it is destroyed.
	 *
	 * @return True the first time it is called on this pickup; false on every later call.
	 */
	bool TryClaimForPickup();

	/**
	 * Sets the local interaction state, refreshed by the local player's UMKHPickupInteractionComponent.
	 * Never replicated: which pickup a player is aiming at only matters to that player. Fires
	 * OnInteractionStateChanged only when the state actually changes.
	 *
	 * @param bNewIsCandidate Whether the local pawn is within the interaction range.
	 * @param bNewIsFocused   Whether this pickup is the one the local player is aiming at (implies bNewIsCandidate).
	 */
	void SetInteractionState(bool bNewIsCandidate, bool bNewIsFocused);

	/** Whether the local pawn is within the interaction range (local state, see SetInteractionState). */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	bool IsInteractionCandidate() const { return bIsInteractionCandidate; }

	/** Whether the local player is aiming at this pickup, i.e. Interact would collect it (local state). */
	UFUNCTION(BlueprintPure, Category = "Items|Pickup")
	bool IsInteractionFocused() const { return bIsInteractionFocused; }

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Called on every machine once the item visuals are applied (the pickup mesh is already set, so
	 * dynamic material instances can be created here). Runs again if the replicated item changes.
	 *
	 * @param InitializedItem The item now displayed by this pickup.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Items|Pickup")
	void OnPickupItemInitialized(const FMKHItemEntry& InitializedItem);

	/**
	 * Called on the local machine whenever the local interaction state changes (see SetInteractionState).
	 * The Blueprint shows the detail widget while focused and the small icon while only a candidate.
	 *
	 * @param bIsCandidate Whether the local pawn is within the interaction range.
	 * @param bIsFocused   Whether the local player is aiming at this pickup.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Items|Pickup")
	void OnInteractionStateChanged(bool bIsCandidate, bool bIsFocused);

	/**
	 * Called on every machine that plays a pickup animation, when it starts. The interaction widgets must
	 * stay hidden while it plays (see IsInteractable).
	 *
	 * @param AnimationType The animation that started.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Items|Pickup")
	void OnPickupAnimationStarted(EMKHPickupAnimationType AnimationType);

	/**
	 * Called on every machine that played a pickup animation, when it reaches its end (after a collection
	 * the visuals are already hidden).
	 *
	 * @param AnimationType The animation that ended.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Items|Pickup")
	void OnPickupAnimationFinished(EMKHPickupAnimationType AnimationType);

private:
	/** Local: refreshes the hold-to-equip hint of the detail prompts for the current item (on gaining the focus). */
	void RefreshHoldHint() const;

	/**
	 * Runs a function on every UMKHItemPickupWidget shown by a widget component of this pickup (the detail prompt).
	 *
	 * @param Function Called once per widget.
	 */
	void ForEachPickupWidget(TFunctionRef<void(UMKHItemPickupWidget&)> Function) const;

	/** Re-applies the item visuals when a new item replicates after BeginPlay. */
	UFUNCTION()
	void OnRep_Item();

	/** Plays the animation received from the server (the collection; appearances start from BeginPlay). */
	UFUNCTION()
	void OnRep_AnimationState();

	/**
	 * Server, before the pickup finishes spawning: sets the replicated appearance animation and the time
	 * from which the pickup may be collected.
	 *
	 * @param Appearance How the pickup appears.
	 * @param DroppedBy  Pawn the item is thrown from (Drop).
	 */
	void InitializeAppearance(EMKHPickupAppearance Appearance, APawn* DroppedBy);

	/**
	 * Plays AnimationState on this machine, or skips it to its final state when it is already over
	 * (received late), then fires OnPickupAnimationStarted when it plays.
	 */
	void PlayAnimationState();

	/** Whether AnimationState ended more than the animation's late start tolerance ago, on the server clock. */
	bool IsAnimationStateExpired() const;

	/** Forwards the end of a local animation to OnPickupAnimationFinished. */
	void HandlePickupAnimationFinished(EMKHPickupAnimationType FinishedType);

	/**
	 * Gets the current server world time as known on this machine (synchronized clock on clients).
	 *
	 * @return The time in seconds; negative when unknown (no game state yet on a client).
	 */
	double GetServerTimeSeconds() const;

	/**
	 * Resolves the item definition and applies its pickup mesh (loading it if needed), then fires
	 * OnPickupItemInitialized. Without a configured mesh, the component keeps its default mesh.
	 */
	void InitializeFromItem();

	/**
	 * Applies the mesh right away when already in memory, otherwise loads it asynchronously first.
	 *
	 * @param MeshAsset     Pickup mesh from the item definition.
	 * @param MeshTransform Per-item transform from the item definition (see ApplyPickupMesh).
	 */
	void LoadAndApplyPickupMesh(const TSoftObjectPtr<UStaticMesh>& MeshAsset, const FTransform& MeshTransform);

	/**
	 * Sets the pickup mesh, then fires OnPickupItemInitialized. The per-item transform is composed on
	 * top of the mesh layout authored in the Blueprint, so an identity transform keeps that layout.
	 *
	 * @param Mesh          Loaded mesh; nullptr keeps the current mesh.
	 * @param MeshTransform Per-item transform, relative to the Blueprint mesh layout.
	 */
	void ApplyPickupMesh(UStaticMesh* Mesh, const FTransform& MeshTransform);

	/** Returns the relative transform of PickupMesh as authored in the class defaults (Blueprint layout). */
	FTransform GetDefaultPickupMeshTransform() const;

	/** Cancels the pending asynchronous mesh load, if any. */
	void CancelPickupMeshLoad();

	/**
	 * Returns the pickup class configured in Project Settings -> Game -> Makhia Data, falling back
	 * to this native class (mesh only, no Blueprint visuals) when it is not configured.
	 *
	 * @param WorldContextObject Any object living in a world owned by a GameInstance.
	 */
	static UClass* ResolvePickupClass(const UObject* WorldContextObject);

	// ============================================================
	// Properties
	// ============================================================

	/** Root of the pickup, at its resting place; the visual root, the interaction ranges and the widgets attach to it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Items|Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PickupRoot;

	/**
	 * Parent of everything the pickup animations move: the mesh and the visuals that must follow it (glow).
	 * Rests at the identity transform; only UMKHPickupAnimationComponent moves it.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Items|Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> VisualRoot;

	/** Displays the item's pickup mesh with the transform from its definition. No collision. Pivot of the animations. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Items|Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/** Plays the drop, rise and collection animations; its parameters are tuned in the Blueprint class defaults. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Items|Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMKHPickupAnimationComponent> PickupAnimation;

	/**
	 * Interaction range: pawns inside it are pickup candidates. Query only and overlapping every
	 * channel (custom ones included), so it never blocks characters.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Items|Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionSphere;

	/** The item held by this pickup; set by the server before the actor finishes spawning. */
	UPROPERTY(ReplicatedUsing = OnRep_Item, BlueprintReadOnly, Category = "Items|Pickup", meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true"))
	FMKHItemEntry Item;

	/** Whether only ExclusiveCollector may collect this pickup; set by the server before the actor finishes spawning. */
	UPROPERTY(Replicated)
	bool bExclusive = false;

	/** The only player allowed to collect an exclusive pickup; becomes nullptr if that player leaves. */
	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> ExclusiveCollector;

	/** Animation every machine plays: the appearance (set before the pickup finishes spawning), then the collection. */
	UPROPERTY(ReplicatedUsing = OnRep_AnimationState)
	FMKHPickupAnimationState AnimationState;

	/** Server only: world time from which the pickup may be collected (end of its appearance animation). */
	double ServerAppearEndTime = 0.0;

	/** Pending asynchronous load of the pickup mesh; cancelled when the item changes or the pickup ends play. */
	TSharedPtr<FStreamableHandle> PickupMeshLoadHandle;

	/** Server only: true once a player has claimed this pickup (see TryClaimForPickup). */
	bool bBeingCollected = false;

	/** Local only: true while the local pawn is within the interaction range. */
	bool bIsInteractionCandidate = false;

	/** Local only: true while the local player is aiming at this pickup. */
	bool bIsInteractionFocused = false;
};
