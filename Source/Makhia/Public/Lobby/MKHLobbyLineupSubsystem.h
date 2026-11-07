// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MKHLobbyLineupSubsystem.generated.h"

class AMKHLobbyPlayerPlatform;
class APlayerState;

DECLARE_MULTICAST_DELEGATE(FMKHLobbyLineupChangedSignature);

/**
 * Local lineup of the lobby: decides on each machine which player stands on which platform.
 *
 * The server only keeps the data (GameState->PlayerArray and AMKHLobbyPlayerState); every machine places the
 * local player on slot 0 (the central platform) and the others after it by PlayerId, which is the same order
 * everywhere. Rebuilt when a player joins or leaves (AMKHLobbyGameState) or when the local player state arrives.
 * Inactive in worlds without lobby platforms. Full model: see Documentation/LobbyPresentation.md.
 */
UCLASS()
class MAKHIA_API UMKHLobbyLineupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UWorldSubsystem Interface
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem Interface

	//~ Begin USubsystem Interface
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the lineup subsystem of the world of a context object.
	 *
	 * @param WorldContextObject  Any object living in a world.
	 * @return The subsystem, or null without a world.
	 */
	static UMKHLobbyLineupSubsystem* Get(const UObject* WorldContextObject);

	/** Asks for a rebuild of the lineup on the next frame (several requests in a frame make one rebuild). */
	void RequestRebuild();

	/** Returns true once the local player stands on the central platform. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Lineup")
	bool IsLineupReady() const { return bLineupReady; }

	/** Returns true when the world has lobby platforms (the lobby level). */
	UFUNCTION(BlueprintPure, Category = "Lobby|Lineup")
	bool HasPlatforms() const { return Platforms.Num() > 0; }

	/** Broadcast after every rebuild. */
	FMKHLobbyLineupChangedSignature OnLineupChanged;

protected:
	//~ Begin UWorldSubsystem Interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Collects the platforms of the level, sorted by slot. */
	void GatherPlatforms();

	/** Places the players on the platforms (local player first). */
	void Rebuild();

	/** Returns the local player's player state, or null while it has not arrived. */
	APlayerState* GetLocalPlayerState() const;

	/**
	 * Returns the players in lineup order: the local player, then the others by PlayerId.
	 *
	 * @param LocalPlayerState  Player state of the local player (not null).
	 */
	TArray<APlayerState*> BuildPlayerOrder(APlayerState* LocalPlayerState) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Platforms of the level, sorted by slot (slot 0 first). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AMKHLobbyPlayerPlatform>> Platforms;

	/** True once the local player stands on the central platform. */
	bool bLineupReady = false;

	/** True while a rebuild is scheduled for the next frame. */
	bool bRebuildPending = false;
};
