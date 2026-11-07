// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/MKHMatchMode.h"
#include "Online/Backend/MKHOnlineTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "MKHLobbySubsystem.generated.h"

class APlayerState;
class UMKHGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReadyStateChangedDelegate, bool, bIsReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartMatchDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartMatchmakingDelegate);
DECLARE_MULTICAST_DELEGATE_OneParam(FMKHOfflineMatchPendingChangedSignature, bool /*bPending*/);

/**
 * Lobby room state and the ways a match starts from the lobby, for the whole life of the game instance.
 *
 * Owns the room counters (players in the lobby, ready, needed to start), the local ready state, the matchmaking
 * flag and region, the short countdown before a lobby match (mirrored on AMKHLobbyGameState) and the cancellable
 * wait before a match against the bot. Session state, travels and the boot stay in UMKHGameInstance.
 * Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHLobbySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Returns the lobby subsystem of the game instance of a world context object.
	 *
	 * @param WorldContextObject  Any object living in a world.
	 * @return The subsystem, or null without a game instance.
	 */
	static UMKHLobbySubsystem* Get(const UObject* WorldContextObject);

	// ------------------------------------------------------------
	// Delegates
	// ------------------------------------------------------------

	/** Broadcast when the local player's ready state changes. */
	UPROPERTY(BlueprintAssignable, Category = "Lobby|Ready")
	FOnReadyStateChangedDelegate OnReadyStateChanged;

	/** Broadcast (host) when every player is ready, there are enough of them and the countdown ran out. */
	UPROPERTY(BlueprintAssignable, Category = "Lobby|Ready")
	FOnStartMatchDelegate OnStartMatchDelegate;

	/** Broadcast (host) when every player is ready but more players are needed: matchmaking starts. */
	UPROPERTY(BlueprintAssignable, Category = "Lobby|Matchmaking")
	FOnStartMatchmakingDelegate OnStartMatchmakingDelegate;

	/** Broadcast when a match against the bot starts or stops waiting (RequestOfflineMatch, cancel, start). */
	FMKHOfflineMatchPendingChangedSignature OnOfflineMatchPendingChanged;

	// ------------------------------------------------------------
	// Room
	// ------------------------------------------------------------

	/** Returns the number of players in the lobby (host side). */
	UFUNCTION(BlueprintPure, Category = "Lobby|Room")
	int32 GetPlayersInLobby() const { return PlayersInLobby; }

	/** Counts a player who joined the lobby (host side). @return The new count. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Room")
	int32 IncrementPlayersInLobby() { return ++PlayersInLobby; }

	/** Counts a player who left the lobby (host side) and stops the match countdown. @return The new count. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Room")
	int32 DecrementPlayersInLobby();

	/** Returns the number of players needed to start a match. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Room")
	int32 GetPlayersToStart() const { return PlayersToStart; }

	/** Sets the number of players needed to start a match. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Room")
	void SetPlayersToStart(int32 NewPlayersToStart) { PlayersToStart = NewPlayersToStart; }

	/** Returns true when another player shares the lobby with the local one (host or client side). */
	UFUNCTION(BlueprintPure, Category = "Lobby|Room")
	bool HasOtherPlayersInLobby() const;

	/** The local player now hosts a lobby alone (UMKHGameInstance::OnLobbyCreated). */
	void HandleLobbyHosted();

	/** The local player left the session: the room counters go back to zero. */
	void HandleSessionLeft();

	/** Resets the room counters, the ready state, matchmaking and the match countdown to their pre-lobby defaults. */
	void ResetLobbyState();

	// --- Voice party (Documentation/VoiceChat.md) ---

	/**
	 * Client: remembers whether the lobby was joined through an invite or a friend's "Join" (not the matchmaking).
	 *
	 * @param bThroughInvite  True for an invite join.
	 */
	void SetJoinedThroughInvite(bool bThroughInvite) { bJoinedThroughInvite = bThroughInvite; }

	/** Client: returns true when the current lobby was joined through an invite or a friend's "Join". */
	bool HasJoinedThroughInvite() const { return bJoinedThroughInvite; }

	/**
	 * Host: adds a player to the voice party (accepted by UMKHVoiceAuthoritySubsystem). Kept across the travels
	 * of the match, forgotten with the lobby.
	 *
	 * @param PlayerId  NetId of the player.
	 */
	void AddVoicePartyMember(const FString& PlayerId) { VoicePartyMemberIds.Add(PlayerId); }

	/**
	 * Host: returns true when a player joined the lobby as party.
	 *
	 * @param PlayerId  NetId of the player.
	 */
	bool IsVoicePartyMember(const FString& PlayerId) const { return !PlayerId.IsEmpty() && VoicePartyMemberIds.Contains(PlayerId); }

	// ------------------------------------------------------------
	// Ready
	// ------------------------------------------------------------

	/** Returns true when the local player is ready. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Ready")
	bool GetIsReady() const { return bIsReady; }

	/**
	 * Sets the local ready state and broadcasts OnReadyStateChanged when it changes.
	 *
	 * @param bNewReady  New ready state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Ready")
	void SetIsReady(bool bNewReady);

	/**
	 * Host: counts a player becoming ready or not. When every player is ready, starts the match countdown (enough
	 * players) or the matchmaking (not enough).
	 *
	 * @param bNewReady  New ready state of that player.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Ready")
	void HandlePlayerReady(bool bNewReady);

	/** Returns true when the host can start the match (enough players, all ready or matchmaking). */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Ready")
	bool CanStartGame() const;

	/**
	 * Returns true while the short countdown before a lobby match runs on the host (every player ready,
	 * UMKHLobbySettings::MatchStartCountdown seconds during which anyone can cancel). Clients read it from
	 * AMKHLobbyGameState.
	 */
	UFUNCTION(BlueprintPure, Category = "Lobby|Ready")
	bool IsMatchCountdownRunning() const;

	// ------------------------------------------------------------
	// Matchmaking
	// ------------------------------------------------------------

	/** Returns true while the matchmaking search runs. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Matchmaking")
	bool GetIsMatchmaking() const { return bIsMatchmaking; }

	/** Sets the matchmaking flag. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Matchmaking")
	void SetIsMatchmaking(bool bNewMatchmaking) { bIsMatchmaking = bNewMatchmaking; }

	/** Returns the region the matchmaking searches in. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Matchmaking")
	EMKHMatchmakingRegion GetSelectedRegion() const { return SelectedRegion; }

	/** Sets the region the matchmaking searches in. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Matchmaking")
	void SetSelectedRegion(EMKHMatchmakingRegion NewRegion) { SelectedRegion = NewRegion; }

	/** A new matchmaking search result is being examined: no lobby is chosen to join yet. */
	void ResetJoinTarget() { bJoinTargetChosen = false; }

	/**
	 * Chooses the lobby the matchmaking joins for the current search result; only the first call per search succeeds.
	 *
	 * @param LobbyId  Backend id of the lobby.
	 * @return True when this lobby is the one to join.
	 */
	bool TryChooseJoinTarget(const FString& LobbyId);

	/** The join of the chosen lobby failed: the next searches skip it for UMKHLobbySettings::UnreachableLobbyMemory. */
	void MarkJoinTargetUnreachable();

	/**
	 * Returns true when a lobby failed to be joined recently (see MarkJoinTargetUnreachable).
	 *
	 * @param LobbyId  Backend id of the lobby.
	 */
	bool IsLobbyUnreachable(const FString& LobbyId) const;

	// ------------------------------------------------------------
	// Match against the bot
	// ------------------------------------------------------------

	/**
	 * Asks for a match against the bot after a short random wait the player can cancel (UMKHLobbySettings), like a
	 * matchmaking search; then UMKHGameInstance::StartOfflineMatch. Refused under the rules of IsOfflineMatchRefused.
	 * The wait is a timer of the lobby world, so leaving the level drops it.
	 *
	 * @param Mode              Offline mode to play.
	 * @param CareerLevelIndex  Career only: level to play (UMKHCareerSubsystem); a locked one is refused, -1 means the
	 *                          first one not beaten yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Bot")
	void RequestOfflineMatch(EMKHOfflineMode Mode = EMKHOfflineMode::RandomDuel, int32 CareerLevelIndex = -1);

	/** Cancels a pending RequestOfflineMatch; does nothing when none is pending. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Bot")
	void CancelOfflineMatchRequest();

	/** Returns true while a match against the bot requested by RequestOfflineMatch is waiting to start. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Bot")
	bool IsOfflineMatchPending() const;

	/** Returns true when a match against the bot is refused: matchmaking or other players in the lobby (logged). */
	bool IsOfflineMatchRefused() const;

	// ------------------------------------------------------------
	// Notices
	// ------------------------------------------------------------

	/**
	 * Posts a short notice for the lobby (e.g. "Luigi left the lobby"), shown by AMKHLobbyHUD as a toast that fades
	 * out by itself once no loading screen covers the lobby. It survives a return to the lobby; a newer notice
	 * replaces an unseen one.
	 *
	 * @param Message           Notice to show.
	 * @param bShowAfterTravel  True when the lobby is about to be left: the notice waits for the next lobby
	 *                          instead of flashing in the one being left.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Notice")
	void PostLobbyNotice(const FText& Message, bool bShowAfterTravel = false);

	/**
	 * Posts "<name> left the lobby" for a player who left the room.
	 *
	 * @param PlayerState       Player who left (its lobby name, else its player name, else "A player").
	 * @param bShowAfterTravel  See PostLobbyNotice.
	 */
	void PostPlayerLeftNotice(const APlayerState* PlayerState, bool bShowAfterTravel = false);

	/** Returns true when a notice waits to be shown. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Notice")
	bool HasPendingLobbyNotice() const { return !PendingLobbyNotice.IsEmpty(); }

	/** Returns the waiting notice and forgets it. */
	FText ConsumeLobbyNotice();

	/** Broadcast when a notice is posted. */
	FSimpleMulticastDelegate OnLobbyNoticePosted;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Every player is ready: starts the countdown before OnStartMatchDelegate (immediate when it is 0). */
	void BeginMatchCountdown();

	/** Stops the countdown started by BeginMatchCountdown (a player is no longer ready or left). */
	void CancelMatchCountdown();

	/** End of the countdown: broadcasts OnStartMatchDelegate if every player is still ready. */
	void HandleMatchCountdownFinished();

	/**
	 * Mirrors the countdown on AMKHLobbyGameState so the clients can show it.
	 *
	 * @param Duration  Seconds left, negative when no countdown runs.
	 */
	void PublishMatchCountdown(float Duration) const;

	/** End of the RequestOfflineMatch wait: starts the match against the bot. */
	void HandleOfflineMatchDelayFinished();

	/** Returns the owning game instance as UMKHGameInstance (null for another class). */
	UMKHGameInstance* GetMKHGameInstance() const;

	/** Returns true when the loaded map is the lobby level. */
	bool IsInLobbyLevel() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Number of players in the lobby, counted by the host (BP_LobbyMode). */
	int32 PlayersInLobby = 0;

	/** Number of players needed to start a match (set by BP_LobbyMode). */
	int32 PlayersToStart = 2;

	/** Number of players marked as ready, counted by the host. */
	int32 PlayersReady = 0;

	/** Ready state of the local player. */
	bool bIsReady = false;

	/** True while the matchmaking search runs. */
	bool bIsMatchmaking = false;

	/** Region the matchmaking searches in (only used by backends that support region selection). */
	EMKHMatchmakingRegion SelectedRegion = EMKHMatchmakingRegion::NoSelection;

	/** Countdown before a lobby match (world timer of the lobby, host only). */
	FTimerHandle MatchCountdownHandle;

	/** Wait of RequestOfflineMatch (world timer of the lobby). */
	FTimerHandle OfflineMatchDelayHandle;

	/** What the wait of RequestOfflineMatch opens when it ends. */
	FMKHOfflineMatchRequest PendingOfflineRequest;

	/** Notice waiting to be shown in the lobby (empty when none). */
	FText PendingLobbyNotice;

	/** Lobbies the matchmaking failed to join, with the wall-clock time of the failure. */
	TMap<FString, double> UnreachableLobbies;

	/** Backend id of the lobby the matchmaking is joining. */
	FString JoinTargetLobbyId;

	/** True once a lobby was chosen for the current search result. */
	bool bJoinTargetChosen = false;

	/** Host: NetIds of the players who joined the lobby as party (voice chat rule). */
	TSet<FString> VoicePartyMemberIds;

	/** Client: true when the current lobby was joined through an invite or a friend's "Join". */
	bool bJoinedThroughInvite = false;

	/** Forgets the voice party and the invite join (the lobby they belong to is gone). */
	void ResetVoiceParty();
};
