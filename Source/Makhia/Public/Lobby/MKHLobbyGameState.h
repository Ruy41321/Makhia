// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "MKHLobbyGameState.generated.h"

/**
 * Game state of the lobby (GameStateClass of BP_LobbyMode).
 *
 * Replicates the short countdown that runs on the host once every player is ready (UMKHLobbySubsystem), so every
 * lobby card can show it and offer to cancel, tells the local lineup (UMKHLobbyLineupSubsystem) when a player
 * joins or leaves, and posts a lobby notice when a player leaves the room.
 * Full model: see Documentation/OfflineModeArchitecture.md and LobbyPresentation.md.
 */
UCLASS()
class MAKHIA_API AMKHLobbyGameState : public AGameState
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin AActor Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface

	//~ Begin AGameStateBase Interface
	/** Adds the player and rebuilds the local lineup (runs on the server and on every client). */
	virtual void AddPlayerState(APlayerState* PlayerState) override;

	/**
	 * Removes the player and rebuilds the local lineup (runs on the server and on every client); a player leaving
	 * the room posts the lobby notice "<name> left the lobby".
	 */
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	//~ End AGameStateBase Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Server only: starts or clears the countdown shown before a lobby match.
	 *
	 * @param Duration  Seconds left before the match starts, negative to clear the countdown.
	 */
	void SetMatchCountdown(float Duration);

	/** Returns the seconds left before the lobby match starts, negative when no countdown runs. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Match")
	float GetMatchCountdownRemaining() const;

private:
	// ============================================================
	// Internal Logic
	// ============================================================

	/**
	 * Returns true when a removed player really left the room: another player, outside travels, world teardown and
	 * loading screens.
	 *
	 * @param PlayerState  Player being removed.
	 */
	bool IsPlayerLeavingRoom(const APlayerState* PlayerState) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Server world time at which the countdown ends, negative when no countdown runs. */
	UPROPERTY(Replicated)
	double MatchCountdownEndTime = -1.0;
};
