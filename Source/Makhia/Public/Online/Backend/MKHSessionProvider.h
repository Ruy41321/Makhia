// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Backend/MKHOnlineTypes.h"
#include "UObject/Object.h"
#include "MKHSessionProvider.generated.h"

/**
 * Backend behind UMKHSessionSubsystem: hosts, updates, searches, joins and destroys lobbies, sends lobby invites and
 * reports the invites accepted outside the game UI. The game never talks to a provider directly; the online backend
 * (UMKHOnlineBackend) creates it and UMKHSessionSubsystem owns the matchmaking rules on top of it.
 *
 * Every completion delegate runs exactly once, possibly before the call returns.
 * Full model: see Documentation/OnlineBackends.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSessionProvider : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Called once right after creation: binds the backend notifications. */
	virtual void InitializeProvider() {}

	/** Called once before release: unbinds everything; pending completions are dropped. */
	virtual void ShutdownProvider() {}

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Broadcast when the player accepted a lobby invite outside the game UI (overlay, platform chat). */
	FMKHLobbyInviteAcceptedSignature OnLobbyInviteAccepted;

	/** Returns true when the backend sessions are enabled. */
	virtual bool IsBackendAvailable() const PURE_VIRTUAL(UMKHSessionProvider::IsBackendAvailable, return false;);

	/**
	 * Hosts a lobby for the local player.
	 *
	 * @param Settings    Lobby settings; Settings.SessionName names the local session.
	 * @param OnComplete  Receives the outcome.
	 */
	virtual void HostLobby(const FMKHLobbySessionSettings& Settings, const FMKHSessionOperationComplete& OnComplete)
		PURE_VIRTUAL(UMKHSessionProvider::HostLobby, );

	/**
	 * Changes the settings of the lobby hosted by the local player.
	 *
	 * @param Settings    New settings; Settings.SessionName names the local session.
	 * @param OnComplete  Receives the outcome.
	 */
	virtual void UpdateLobby(const FMKHLobbySessionSettings& Settings, const FMKHSessionOperationComplete& OnComplete)
		PURE_VIRTUAL(UMKHSessionProvider::UpdateLobby, );

	/**
	 * Searches lobbies. One search runs at a time: a second one fails at once.
	 *
	 * @param Query       What to look for.
	 * @param OnComplete  Receives the lobbies found.
	 */
	virtual void SearchLobbies(const FMKHLobbySearchQuery& Query, const FMKHLobbySearchComplete& OnComplete)
		PURE_VIRTUAL(UMKHSessionProvider::SearchLobbies, );

	/**
	 * Joins a lobby produced by this provider (search result or accepted invite).
	 *
	 * @param LocalSessionName  Local name the joined session is registered under.
	 * @param Lobby             Lobby to join.
	 * @param OnComplete        Receives the outcome and, on success, the address to travel to.
	 */
	virtual void JoinLobby(FName LocalSessionName, const FMKHLobbySearchResult& Lobby, const FMKHLobbyJoinComplete& OnComplete)
		PURE_VIRTUAL(UMKHSessionProvider::JoinLobby, );

	/**
	 * Leaves (client) or destroys (host) a local session.
	 *
	 * @param SessionName  Local session name.
	 * @param OnComplete   Receives the outcome.
	 */
	virtual void DestroySession(FName SessionName, const FMKHSessionOperationComplete& OnComplete)
		PURE_VIRTUAL(UMKHSessionProvider::DestroySession, );

	/**
	 * Invites a friend to a local session.
	 *
	 * @param SessionName  Local session name.
	 * @param FriendId     Backend id of the friend.
	 * @return True when the invite was handed to the backend.
	 */
	virtual bool SendLobbyInvite(FName SessionName, const FString& FriendId)
		PURE_VIRTUAL(UMKHSessionProvider::SendLobbyInvite, return false;);

	/**
	 * Returns the free slots of a local session.
	 *
	 * @param SessionName  Local session name.
	 * @return The free slots, or -1 when the session does not exist.
	 */
	virtual int32 GetOpenSlots(FName SessionName) const PURE_VIRTUAL(UMKHSessionProvider::GetOpenSlots, return -1;);
};
