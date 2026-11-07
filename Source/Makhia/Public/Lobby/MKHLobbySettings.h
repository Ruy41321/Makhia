// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MKHLobbySettings.generated.h"

class UMKHLobbyBackdropDefinition;

/**
 * Lobby settings exposed in Project Settings -> Game -> Makhia Lobby: background picture and match start timings.
 *
 * Values are stored in Config/DefaultGame.ini under [/Script/Makhia.MKHLobbySettings].
 * Full model: see Documentation/LobbyPresentation.md.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Makhia Lobby"))
class MAKHIA_API UMKHLobbySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Places the settings under Project Settings -> Game -> Makhia Lobby. */
	UMKHLobbySettings();

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Convenience accessor for the lobby settings.
	 *
	 * @return The CDO of UMKHLobbySettings.
	 */
	static const UMKHLobbySettings* Get();

	// ============================================================
	// Properties
	// ============================================================

	/** Background shown in the lobby. Loaded only by AMKHLobbyBackdrop, so it costs nothing outside the lobby. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Backdrop")
	TSoftObjectPtr<UMKHLobbyBackdropDefinition> Backdrop;

	/** Seconds between "every player ready" and the start of a lobby match; anyone can cancel meanwhile. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Match Start", meta = (ClampMin = "0.0", Units = "s"))
	float MatchStartCountdown = 2.f;

	/** Shortest wait before a match against the bot (UMKHLobbySubsystem::RequestOfflineMatch). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Match Start", meta = (ClampMin = "0.0", Units = "s"))
	float OfflineMatchDelayMin = 2.f;

	/** Longest wait before a match against the bot; the wait is random in between, like a search. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Match Start", meta = (ClampMin = "0.0", Units = "s"))
	float OfflineMatchDelayMax = 4.f;

	/**
	 * Seconds a lobby the matchmaking failed to join is skipped by the next searches. EOS keeps a destroyed lobby in
	 * its search index for a while; see UMKHSessionSubsystem::FilterSearchResults.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Matchmaking", meta = (ClampMin = "0.0", Units = "s"))
	float UnreachableLobbyMemory = 60.f;

	/** Seconds between two matchmaking searches (UMKHSessionSubsystem). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Matchmaking", meta = (ClampMin = "0.5", Units = "s"))
	float MatchmakingSearchInterval = 2.f;
};
