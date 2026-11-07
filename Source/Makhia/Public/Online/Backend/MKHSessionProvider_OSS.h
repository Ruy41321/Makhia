// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Online/Backend/MKHSessionProvider.h"
#include "OnlineSessionSettings.h"
#include "MKHSessionProvider_OSS.generated.h"

class IOnlineSubsystem;

/** Lobby handle of the providers built on IOnlineSession: the raw search result. */
struct MAKHIA_API FMKHLobbyHandle_OSS : public FMKHLobbyHandle
{
	/** Search result to hand back to IOnlineSession::JoinSession. */
	FOnlineSessionSearchResult Result;
};

/**
 * Session provider on top of an engine online subsystem (IOnlineSession). Owns the request bookkeeping and the
 * conversion between the backend-independent lobby types and FOnlineSessionSettings / FOnlineSessionSearch.
 * A concrete backend names its online subsystem and adjusts the settings and searches through the Configure* hooks.
 *
 * The session notifications are bound for the provider lifetime and filtered by session name, so other users of
 * the same session interface are never disturbed. Full model: see Documentation/OnlineBackends.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSessionProvider_OSS : public UMKHSessionProvider
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Binds the session notifications of the online subsystem. */
	virtual void InitializeProvider() override;

	/** Unbinds the session notifications and drops pending completions. */
	virtual void ShutdownProvider() override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Returns true when the online subsystem exists and exposes a session interface. */
	virtual bool IsBackendAvailable() const override;

	/** Creates the session with CreateSession; a second host of the same name while one is pending fails. */
	virtual void HostLobby(const FMKHLobbySessionSettings& Settings, const FMKHSessionOperationComplete& OnComplete) override;

	/** Updates the named session with UpdateSession, starting from its current settings. */
	virtual void UpdateLobby(const FMKHLobbySessionSettings& Settings, const FMKHSessionOperationComplete& OnComplete) override;

	/** Runs FindSessions; results that miss a required attribute are dropped (some backends do not filter). */
	virtual void SearchLobbies(const FMKHLobbySearchQuery& Query, const FMKHLobbySearchComplete& OnComplete) override;

	/** Runs JoinSession and resolves the connect string of the joined session. */
	virtual void JoinLobby(FName LocalSessionName, const FMKHLobbySearchResult& Lobby, const FMKHLobbyJoinComplete& OnComplete) override;

	/** Runs DestroySession; a destroy of a session already being destroyed waits for the same answer. */
	virtual void DestroySession(FName SessionName, const FMKHSessionOperationComplete& OnComplete) override;

	/** Sends an invite with SendSessionInviteToFriend; the friend id is parsed by the identity interface. */
	virtual bool SendLobbyInvite(FName SessionName, const FString& FriendId) override;

	/** Returns the open public and private connections of the named session, -1 without one. */
	virtual int32 GetOpenSlots(FName SessionName) const override;

	/**
	 * Converts an engine session search result into a lobby (id, attributes as strings, player counts, OSS handle).
	 * Used by the base ReadSearchResult and by the social providers that resolve invites into sessions.
	 *
	 * @param Result  Search result of any engine online subsystem.
	 */
	static FMKHLobbySearchResult MakeLobbyFromSearchResult(const FOnlineSessionSearchResult& Result);

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns the name of the online subsystem this provider drives (e.g. "EIK", "NULL", "STEAM"). */
	virtual FName GetOnlineSubsystemName() const PURE_VIRTUAL(UMKHSessionProvider_OSS::GetOnlineSubsystemName, return NAME_None;);

	/**
	 * Fills the session settings of a hosted or updated lobby. The base version sets a presence lobby with invites,
	 * the lobby attributes and the owner's session name; overrides add backend-specific settings.
	 *
	 * @param Settings     Backend-independent settings.
	 * @param OutSettings  Session settings to fill (already holding the current settings on update).
	 */
	virtual void ConfigureSessionSettings(const FMKHLobbySessionSettings& Settings, FOnlineSessionSettings& OutSettings) const;

	/**
	 * Fills a lobby search. The base version asks for lobbies with the required attributes.
	 *
	 * @param Query      Backend-independent query.
	 * @param OutSearch  Search to fill.
	 */
	virtual void ConfigureSearch(const FMKHLobbySearchQuery& Query, FOnlineSessionSearch& OutSearch) const;

	/**
	 * Converts a backend search result. The base version copies the id, the attributes (as strings) and the player
	 * counts, and wraps the result in a FMKHLobbyHandle_OSS.
	 *
	 * @param Result    Backend search result.
	 * @param OutLobby  Lobby to fill.
	 */
	virtual void ReadSearchResult(const FOnlineSessionSearchResult& Result, FMKHLobbySearchResult& OutLobby) const;

	/** Returns the online subsystem of the game instance world, or null when it is disabled. */
	IOnlineSubsystem* GetOnlineSubsystem() const;

	/** Returns the session interface of the online subsystem, or null. */
	IOnlineSessionPtr GetSessionInterface() const;

	/** Local player index used for every online call. Makhia has a single local player. */
	static constexpr int32 LocalUserNum = 0;

private:
	/** Completions waiting for a session notification, by local session name. */
	using FPendingOperations = TMap<FName, TArray<FMKHSessionOperationComplete>>;

	/**
	 * Runs and forgets the completions waiting for a session name.
	 *
	 * @param Pending      Waiting completions.
	 * @param SessionName  Session the notification is about.
	 * @param bSuccess     Outcome to report.
	 */
	static void CompletePending(FPendingOperations& Pending, FName SessionName, bool bSuccess);

	/**
	 * Runs and forgets the completion of a join.
	 *
	 * @param SessionName    Local session name of the join.
	 * @param Result         Outcome to report.
	 * @param ConnectString  Address to travel to (success only).
	 */
	void CompleteJoin(FName SessionName, EMKHJoinLobbyResult Result, const FString& ConnectString);

	/**
	 * Runs and forgets the completion of the pending search.
	 *
	 * @param bSuccess  Outcome of FindSessions.
	 */
	void CompleteSearch(bool bSuccess);

	/** Converts a backend search result into a lobby (see ReadSearchResult). */
	FMKHLobbySearchResult MakeLobbyResult(const FOnlineSessionSearchResult& Result) const;

	/** Returns true when a lobby has every attribute required by a query. */
	static bool MatchesQuery(const FMKHLobbySearchResult& Lobby, const FMKHLobbySearchQuery& Query);

	/** Maps an online subsystem join result to the backend-independent one. */
	static EMKHJoinLobbyResult ToJoinLobbyResult(EOnJoinSessionCompleteResult::Type Result);

	/** CreateSession notification. */
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	/** UpdateSession notification. */
	void HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful);

	/** FindSessions notification. */
	void HandleFindSessionsComplete(bool bWasSuccessful);

	/** JoinSession notification. */
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	/** DestroySession notification. */
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	/** Invite accepted outside the game UI (overlay, platform chat). */
	void HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	// ============================================================
	// Properties
	// ============================================================

	/** Pending CreateSession completions. */
	FPendingOperations PendingCreates;

	/** Pending UpdateSession completions. */
	FPendingOperations PendingUpdates;

	/** Pending DestroySession completions. */
	FPendingOperations PendingDestroys;

	/** Pending JoinSession completions, by local session name. */
	TMap<FName, FMKHLobbyJoinComplete> PendingJoins;

	/** Search in flight; null when none. */
	TSharedPtr<FOnlineSessionSearch> PendingSearch;

	/** Query of the search in flight. */
	FMKHLobbySearchQuery PendingSearchQuery;

	/** Completion of the search in flight. */
	FMKHLobbySearchComplete PendingSearchComplete;

	/** Session interface the notifications are bound to (kept to unbind the same instance). */
	TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> BoundSessions;

	/** True once the online subsystem failed to start: it is not asked again (each attempt restarts it). */
	mutable bool bSubsystemStartFailed = false;

	/** Notification handles, removed on shutdown. */
	FDelegateHandle CreateCompleteHandle;
	FDelegateHandle UpdateCompleteHandle;
	FDelegateHandle FindCompleteHandle;
	FDelegateHandle JoinCompleteHandle;
	FDelegateHandle DestroyCompleteHandle;
	FDelegateHandle InviteAcceptedHandle;
};
