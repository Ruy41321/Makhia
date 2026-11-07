// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MKHItemSpawnGroup.generated.h"

class UBillboardComponent;

/**
 * Caps how many pickups a set of item spawn points may hold at once (e.g. six spots around the arena,
 * two weapons at a time). Spawn points join through AMKHItemSpawnPoint::SpawnGroup. When the match enters a
 * phase that triggers several members, UMKHItemSpawnSubsystem lets a random subset of them spawn, up to the
 * free slots. Server only: never loaded on clients.
 *
 * Full model: see Documentation/ItemSpawnSystemArchitecture.md.
 */
UCLASS(meta = (DisplayName = "Item Spawn Group"))
class MAKHIA_API AMKHItemSpawnGroup : public AActor
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Creates the root and the editor sprite; the group exists on the server only. */
	AMKHItemSpawnGroup();

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Gets the maximum number of member spawn points holding (or about to spawn) a pickup at the same time. */
	UFUNCTION(BlueprintPure, Category = "Items|Spawn")
	int32 GetMaxActivePickups() const { return MaxActivePickups; }

private:
	// ============================================================
	// Properties
	// ============================================================

	/** Maximum number of member spawn points holding (or about to spawn) a pickup at the same time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items|Spawn", meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 MaxActivePickups = 1;

	/** Root of the group actor. */
	UPROPERTY(VisibleAnywhere, Category = "Items|Spawn")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	/** Editor-only icon of the group. */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> SpriteComponent;
#endif
};
