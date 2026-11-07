// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Backend/MKHOnlineTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "MKHSessionSubsystem.generated.h"

class UMKHGameInstance;
class UMKHLobbySubsystem;
class UMKHSessionProvider;

/** Steps of the lobby and matchmaking flow, broadcast to the UI by OnSessionEvent (BP_MatchmakingComponent). */
UENUM(BlueprintType)
enum class EMKHSessionEvent : uint8
{
	/** Hosting the own lobby started. */
	CreateLobbyStart,
	/** The own lobby is hosted (only when the host asked to be notified). */
	CreateLobbySuccess,
	/** Hosting the own lobby failed. */
	CreateLobbyFail,
	/** The own lobby is now advertised to the matchmaking. */
	MatchmakingStart,
	/** The own lobby could not be advertised: matchmaking did not start. */
	MatchmakingStartError,
	/** A matchmaking search started. */
	SearchStart,
	/** A matchmaking search failed. */
	SearchError,
	/** A search result could not be used. */
	FoundInvalidLobby,
	/** Joining the opponent's lobby started. */
	JoiningStart,
	/** The opponent's lobby was joined. */
	JoiningSuccess,
	/** Joining the opponent's lobby failed; the own lobby is hosted again. */
	JoiningFail,
	/** Leaving the current lobby started. */
	DestroyingSession,
	/** The current lobby was left; the lobby level is reopened as host. */
	LobbyDestroyed,
	/** Leaving the current lobby failed. */
	DestroyFail,
	/** Matchmaking was cancelled and the own lobby is passive again. */
	CancelSuccess
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMKHSessionEventSignature, EMKHSessionEvent, Event);

/**
 * Lobby sessions and random matchmaking of the local player, on top of the session provider of the active online
 * backend. Each signed-in player hosts a 2-slot lobby ("<UserId>_LOBBY"); matchmaking advertises it with a start
 * time and searches the other advertised lobbies: the player who started later joins the one who started earlier.
 *
 * Session state (in session, host, session name) stays in UMKHGameInstance, room/ready/matchmaking flags in
 * UMKHLobbySubsystem. BP_MatchmakingComponent is a thin facade that forwards the lobby events here and relays
 * OnSessionEvent to its dispatchers. Full model: see Documentation/OnlineBackends.md.
 */
UCLASS()
class MAKHIA_API UMKHSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Creates the session provider of the active backend. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Releases the session provider and stops the matchmaking timer. */
	virtual void Deinitialize() override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Returns the session subsystem of the game instance of a world context object.
	 *
	 * @param WorldContextObject  Any object living in a world.
	 * @return The subsystem, or null without a game instance.
	 */
	static UMKHSessionSubsystem* Get(const UObject* WorldContextObject);

	/** Broadcast at every step of the lobby and matchmaking flow. */
	UPROPERTY(BlueprintAssignable, Category = "Session")
	FMKHSessionEventSignature OnSessionEvent;

	/** Returns true when the backend sessions are enabled. */
	UFUNCTION(BlueprintPure, Category = "Session")
	bool IsSessionBackendAvailable() const;

	// ------------------------------------------------------------
	// Lobby
	// ------------------------------------------------------------

	/**
	 * Lobby level start (BP_MatchmakingComponent BeginPlay): forgets the matchmaking state of the previous lobby and
	 * hosts the own lobby when the level was opened as listen server and no session exists yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Session|Lobby")
	void HandleLobbyLevelStarted();

	/**
	 * Hosts the own passive lobby (2 slots, not advertised).
	 *
	 * @param bShouldNotify  True to broadcast CreateLobbySuccess when it is hosted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Session|Lobby")
	void HostLobby(bool bShouldNotify);

	/**
	 * Leaves the current lobby when there is someone to leave (client, or host with another player), then reopens
	 * the lobby level as host.
	 */
	UFUNCTION(BlueprintCallable, Category = "Session|Lobby")
	void AbandonLobby();

	/** Host: starts the match when the lobby allows it (loading screen, passive lobby, server travel to the arena). */
	UFUNCTION(BlueprintCallable, Category = "Session|Lobby")
	void StartMatch();

	/** Returns true when the local player is in a lobby session that still has a free slot. */
	bool HasFreeLobbySlot() const;

	/**
	 * Invites a friend to the current lobby session.
	 *
	 * @param FriendId  Backend id of the friend (FMKHFriendInfo::NetId).
	 * @return True when the backend took the invite.
	 */
	bool InviteFriendToLobby(const FString& FriendId);

	/**
	 * Joins the lobby of an invite accepted in the game UI (UMKHFriendsSubsystem): leaves the own lobby first, like an
	 * invite accepted from the platform overlay.
	 *
	 * @param Lobby  Lobby resolved by the social provider.
	 */
	void JoinInvitedLobby(const FMKHLobbySearchResult& Lobby);

	/** Network or travel error: leaves the current session (whatever the answer) and reopens the lobby as host. */
	UFUNCTION(BlueprintCallable, Category = "Session|Lobby")
	void RestoreLobbyAfterError();

	// ------------------------------------------------------------
	// Matchmaking
	// ------------------------------------------------------------

	/** Advertises the own lobby with the start time and starts searching an opponent. */
	UFUNCTION(BlueprintCallable, Category = "Session|Matchmaking")
	void StartMatchmaking();

	/** Stops searching: the host returns to a passive lobby (or leaves when someone joined), a client leaves. */
	UFUNCTION(BlueprintCallable, Category = "Session|Matchmaking")
	void StopMatchmaking();

	/** The player pressed Cancel: not ready anymore, and the matchmaking stops unless a join is in flight. */
	UFUNCTION(BlueprintCallable, Category = "Session|Matchmaking")
	void CancelMatchmaking();

	/** Returns true while the matchmaking is joining an opponent's lobby. */
	UFUNCTION(BlueprintPure, Category = "Session|Matchmaking")
	bool IsJoiningLobby() const { return bIsJoiningLobby; }

	// ------------------------------------------------------------
	// Session teardown (game instance)
	// ------------------------------------------------------------

	/**
	 * Leaves or destroys the current session (UMKHGameInstance::GetCurrentSessionName).
	 *
	 * @param OnComplete  Runs exactly once with the outcome, possibly inline; false also when there was nothing to destroy.
	 * @return False when nothing was asked to the backend (no provider or no session name); OnComplete then never runs.
	 */
	bool DestroyCurrentSession(const FMKHSessionOperationComplete& OnComplete);

	/** Leaves or destroys the current session without waiting for the answer. */
	void DestroyCurrentSessionDetached();

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns the owning game instance as UMKHGameInstance (null for another class). */
	UMKHGameInstance* GetMKHGameInstance() const;

	/** Returns the lobby subsystem of the game instance. */
	UMKHLobbySubsystem* GetLobby() const;

	/** Returns the name of the current session, NAME_None without game instance. */
	FName GetCurrentSessionName() const;

	/** Returns true while the game instance is in a session. */
	bool IsInSession() const;

	/** Returns true while the game instance hosts its session. */
	bool IsHost() const;

	/** Broadcasts a step of the flow. */
	void BroadcastEvent(EMKHSessionEvent Event);

	/**
	 * Builds the settings of the own lobby.
	 *
	 * @param bMatchmaking  True for an advertised lobby that searches an opponent (with the start time).
	 */
	FMKHLobbySessionSettings MakeLobbySettings(bool bMatchmaking) const;

	/**
	 * Makes the own lobby passive again (not advertised, not matchmaking).
	 *
	 * @param OnComplete  Receives the outcome.
	 */
	void SetPassiveLobby(const FMKHSessionOperationComplete& OnComplete);

	/** Host lobby completion. */
	void HandleHostLobbyComplete(bool bSuccess);

	/** SetMatchmakingLobby completion: starts the first search. */
	void HandleMatchmakingLobbyComplete(bool bSuccess);

	/** Starts one matchmaking search (never as a client of another lobby). */
	void SearchLobbies();

	/** Search completion: tries the results, then schedules the next search. */
	void HandleSearchComplete(bool bSuccess, const TArray<FMKHLobbySearchResult>& Results);

	/**
	 * Keeps the most recent lobby of each owner and drops the lobbies recently failed to join. EOS keeps a destroyed
	 * lobby in its index for a while; see Documentation/LoadingScreen.md -> Matchmaking search results.
	 */
	TArray<FMKHLobbySearchResult> FilterSearchResults(const TArray<FMKHLobbySearchResult>& Results) const;

	/** Outcome of CanJoinLobby. */
	enum class ELobbyJoinCheck : uint8
	{
		/** Join it. */
		Joinable,
		/** Valid but not for us (own lobby, full, empty or the other player must join us). */
		NotJoinable,
		/** Unusable result. */
		Invalid
	};

	/** Returns whether a search result should be joined. */
	ELobbyJoinCheck CanJoinLobby(const FMKHLobbySearchResult& Lobby) const;

	/**
	 * Returns true when the local player must join this lobby rather than wait for its owner: the local player started
	 * searching later, or in the same second while the owner's session name is the smaller one.
	 */
	bool HasJoinPriority(const FMKHLobbySearchResult& Lobby) const;

	/**
	 * Leaves the own lobby, then joins the chosen one with the loading screen up.
	 *
	 * @param Lobby  Lobby chosen by the search.
	 */
	void BeginMatchmakingJoin(const FMKHLobbySearchResult& Lobby);

	/** Own lobby left (or not destroyable): starts the join of JoinTarget with the loading screen up. */
	void StartMatchmakingJoin();

	/**
	 * Joins a lobby once the own session is gone and travels to its host.
	 *
	 * @param Lobby  Lobby to join.
	 */
	void JoinLobby(const FMKHLobbySearchResult& Lobby);

	/** Join completion: travels to the host on success; on failure hosts the own lobby again and reports MM-JOIN-FAILED. */
	void HandleJoinLobbyComplete(EMKHJoinLobbyResult Result, const FString& ConnectString);

	/**
	 * Failed search: retried like an empty one, until MKHSession::MaxConsecutiveSearchFailures failures in a row
	 * report SearchError and stop the searches.
	 */
	void HandleSearchFailed();

	/** End of a search: schedules the next one unless the matchmaking is over. */
	void HandleSearchFinished();

	/** Timer: searches again, waits again, or stops. */
	void HandleSearchAgain();

	/** Schedules HandleSearchAgain after the search interval. */
	void ScheduleSearchAgain();

	/** Returns true when no further search is needed (matchmaking off, lobby full, or client of a lobby). */
	bool ShouldStopSearch() const;

	/** Returns true when the next search must wait (a join is in flight or the own lobby is gone). */
	bool ShouldSearchLater() const;

	/**
	 * Invite accepted outside the game UI: leaves the own lobby and joins the invited one. Outside a ready lobby
	 * (start-up from an invite, travel, match) the invite is kept and joined once the lobby is ready.
	 */
	void HandleLobbyInviteAccepted(bool bSuccess, const FMKHLobbySearchResult& Lobby);

	/** Returns true when an accepted invite can be joined now: lobby level loaded, in a session, no join in flight. */
	bool CanJoinInvitedLobbyNow() const;

	/** Joins the invite kept by HandleLobbyInviteAccepted, on the next tick, if the lobby is ready. */
	void ScheduleDeferredInvite();

	/** Forgets the session, stops the matchmaking and reopens the lobby level as host. */
	void ReopenLobbyAsHost();

	// ============================================================
	// Properties
	// ============================================================

	/** Session provider of the active backend. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHSessionProvider> Provider;

private:
	/** True when the current HostLobby must broadcast CreateLobbySuccess. */
	bool bShouldNotifyLobby = false;

	/** True once the player cancelled the running matchmaking. */
	bool bHasCancelled = false;

	/** True while the matchmaking joins an opponent's lobby. */
	bool bIsJoiningLobby = false;

	/** True while the lobby of an accepted invite is being joined (a failure hosts the own lobby again). */
	bool bIsJoiningInvite = false;

	/** Matchmaking searches that failed in a row. */
	int32 ConsecutiveSearchFailures = 0;

	/** Unix time the local player started searching. */
	int64 MatchmakingStartTime = 0;

	/** Lobby the matchmaking is joining. */
	FMKHLobbySearchResult JoinTarget;

	/** Lobby of an invite accepted before the own lobby was ready; invalid when none. */
	FMKHLobbySearchResult DeferredInvite;

	/** Timer between two searches (world timer of the lobby level). */
	FTimerHandle SearchAgainHandle;
};
