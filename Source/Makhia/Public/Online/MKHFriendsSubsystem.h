// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Online/MKHFriendTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHFriendsSubsystem.generated.h"

class AGameStateBase;
class AMKHGameState;
class UMKHSocialProvider;
class UTexture2D;
struct FMKHLobbySearchResult;
enum class EMKHSocialOverlayPage : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMKHFriendsChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMKHFriendRequestResultSignature, EMKHFriendRequestResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMKHLobbyInvitesChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMKHLobbyInviteAcceptResultSignature, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMKHFriendAvatarChangedSignature, const FString&, NetId);

/**
 * Friend list, presence, friend requests and lobby invites of the signed-in player, for the game UI.
 *
 * The data comes from the social provider of the online backend (UMKHSocialProvider); this subsystem owns the
 * cached and sorted list, the service state, the invites sent and received and the UI delegates. Lobby invites are
 * sent and joined through UMKHSessionSubsystem, like an invite accepted from the platform overlay. Features the
 * backend lacks are reported by the capabilities (e.g. SupportsInGameFriendRequests).
 * Full flow: see Documentation/GameMenuArchitecture.md (Friend list) and Documentation/OnlineBackends.md.
 */
UCLASS()
class MAKHIA_API UMKHFriendsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin USubsystem Interface
	/** Creates the social provider of the online backend and evaluates the sign-in state. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Stops the service and releases the provider. */
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/** Broadcast when the friend list, a presence or the service state changed. */
	UPROPERTY(BlueprintAssignable, Category = "Friends")
	FMKHFriendsChangedSignature OnFriendsChanged;

	/**
	 * Returns what the local player is doing now: lobby with its player count, or match with round and score.
	 * Also used by other presence publishers (Discord), which run without a signed-in social account.
	 *
	 * @param GameInstance  Game instance of the local player.
	 * @param Previous      Activity published last, kept while travelling (no match game state yet).
	 * @return The activity to publish; None when there is no world.
	 */
	static FMKHPlayerActivity BuildLocalActivity(const UGameInstance* GameInstance, const FMKHPlayerActivity& Previous);

	/** Broadcast when a friend request sent with SendFriendRequest completes. */
	UPROPERTY(BlueprintAssignable, Category = "Friends")
	FMKHFriendRequestResultSignature OnFriendRequestResult;

	/** Broadcast when a lobby invite arrives or leaves the pending list. */
	UPROPERTY(BlueprintAssignable, Category = "Friends|Lobby")
	FMKHLobbyInvitesChangedSignature OnLobbyInvitesChanged;

	/** Broadcast when accepting a lobby invite resolved the lobby (true) or found it gone or full (false). */
	UPROPERTY(BlueprintAssignable, Category = "Friends|Lobby")
	FMKHLobbyInviteAcceptResultSignature OnLobbyInviteAcceptResult;

	/** Broadcast when the avatar of a friend or recent player finished loading or changed. */
	UPROPERTY(BlueprintAssignable, Category = "Friends")
	FMKHFriendAvatarChangedSignature OnFriendAvatarChanged;

	/**
	 * Returns the friends subsystem of the game instance that owns the given object.
	 *
	 * @param WorldContextObject  Any object living in a game world.
	 * @return The subsystem, or null outside a game instance.
	 */
	static UMKHFriendsSubsystem* Get(const UObject* WorldContextObject);

	/** Reads the friend list again from the backend. Starts the service first if the player signed in meanwhile. */
	UFUNCTION(BlueprintCallable, Category = "Friends")
	void RefreshFriends();

	/** Returns the current availability of the friends service. */
	UFUNCTION(BlueprintPure, Category = "Friends")
	EMKHFriendsServiceState GetServiceState() const { return ServiceState; }

	/** Returns every entry (friends and pending requests), sorted for display. */
	UFUNCTION(BlueprintPure, Category = "Friends")
	TArray<FMKHFriendInfo> GetFriends() const { return Friends; }

	/** Returns how many accepted friends are online (including away and in Makhia). */
	UFUNCTION(BlueprintPure, Category = "Friends")
	int32 GetOnlineFriendCount() const;

	/** Returns how many accepted friends the player has. */
	UFUNCTION(BlueprintPure, Category = "Friends")
	int32 GetFriendCount() const;

	/**
	 * Returns true when a user is an accepted friend of the local player on the platform (voice chat rule).
	 *
	 * @param NetId  NetId of the user.
	 */
	UFUNCTION(BlueprintPure, Category = "Friends")
	bool IsFriend(const FString& NetId) const;

	/**
	 * Returns true when the local player blocked a user on the platform (their voice is always muted).
	 *
	 * @param NetId  NetId of the user.
	 */
	UFUNCTION(BlueprintPure, Category = "Friends")
	bool IsBlockedUser(const FString& NetId) const;

	/** Returns true when friend requests can be sent, accepted and rejected in the game (backend capability). */
	UFUNCTION(BlueprintPure, Category = "Friends")
	bool SupportsInGameFriendRequests() const;

	/**
	 * Accepts a friend request received from another player (only when SupportsInGameFriendRequests).
	 *
	 * @param NetId  NetId of an entry whose relation is RequestReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Friends")
	void AcceptFriendRequest(const FString& NetId);

	/**
	 * Rejects a friend request received from another player (only when SupportsInGameFriendRequests).
	 *
	 * @param NetId  NetId of an entry whose relation is RequestReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Friends")
	void RejectFriendRequest(const FString& NetId);

	/**
	 * Looks an account up by display name and sends it a friend request (only when SupportsInGameFriendRequests).
	 * The outcome is broadcast by OnFriendRequestResult; one request runs at a time.
	 *
	 * @param DisplayName  Exact display name of the player to add.
	 */
	UFUNCTION(BlueprintCallable, Category = "Friends")
	void SendFriendRequest(const FString& DisplayName);

	/** Returns true while a friend request sent with SendFriendRequest is in flight. */
	UFUNCTION(BlueprintPure, Category = "Friends")
	bool IsFriendRequestPending() const { return !PendingFriendRequestName.IsEmpty(); }

	/** Returns true when the player is in their lobby and it still has a free slot. */
	UFUNCTION(BlueprintPure, Category = "Friends|Lobby")
	bool CanInviteToLobby() const;

	/**
	 * Invites a friend who is playing Makhia to the current lobby.
	 *
	 * @param NetId  NetId of the friend to invite.
	 * @return True if the invite was handed to the backend.
	 */
	UFUNCTION(BlueprintCallable, Category = "Friends|Lobby")
	bool InviteToLobby(const FString& NetId);

	/**
	 * Returns how many things in the friend list wait for an answer: lobby invites received and friend requests
	 * received. Drives the notification badge of the game menu button.
	 */
	UFUNCTION(BlueprintPure, Category = "Friends")
	int32 GetPendingNotificationCount() const;

	/** Returns the lobby invites received and not answered yet, oldest first. */
	UFUNCTION(BlueprintPure, Category = "Friends|Lobby")
	TArray<FMKHLobbyInvite> GetLobbyInvites() const { return LobbyInvites; }

	/** Returns true when accepting a lobby invite is allowed (only from the lobby, never during a match). */
	UFUNCTION(BlueprintPure, Category = "Friends|Lobby")
	bool CanAcceptLobbyInvites() const;

	/**
	 * Joins the lobby a pending invite points at, leaving the current lobby.
	 * The outcome of the lookup is broadcast by OnLobbyInviteAcceptResult.
	 *
	 * @param InviteId  Id of a pending invite.
	 */
	UFUNCTION(BlueprintCallable, Category = "Friends|Lobby")
	void AcceptLobbyInvite(const FString& InviteId);

	/**
	 * Drops a pending lobby invite from the list (and declines it on backends that support it).
	 *
	 * @param InviteId  Id of a pending invite.
	 */
	UFUNCTION(BlueprintCallable, Category = "Friends|Lobby")
	void DeclineLobbyInvite(const FString& InviteId);

	/** Returns the players met in recent matches who are not friends, most recent first (empty without the capability). */
	UFUNCTION(BlueprintPure, Category = "Friends|Recent Players")
	TArray<FMKHFriendInfo> GetRecentPlayers() const { return RecentPlayers; }

	/** Returns true when the backend lists recent players (capability). */
	UFUNCTION(BlueprintPure, Category = "Friends|Recent Players")
	bool SupportsRecentPlayers() const;

	/**
	 * Tells the platform who the local player is facing, for its recent players list. Called on every machine when a
	 * match starts; bots and the local player are skipped.
	 *
	 * @param GameState  Game state of the match.
	 */
	void ReportMatchOpponents(const AGameStateBase* GameState);

	/** Returns true when the backend gives profile pictures (capability). */
	UFUNCTION(BlueprintPure, Category = "Friends|Avatar")
	bool SupportsAvatars() const;

	/**
	 * Returns the profile picture of a friend or recent player. A picture not loaded yet is requested and announced by
	 * OnFriendAvatarChanged.
	 *
	 * @param NetId  NetId of the entry.
	 * @return The picture, or null while loading, without one or without the capability.
	 */
	UFUNCTION(BlueprintCallable, Category = "Friends|Avatar")
	UTexture2D* GetFriendAvatar(const FString& NetId);

	/** Returns true when the backend has a platform overlay the game can open (capability). */
	UFUNCTION(BlueprintPure, Category = "Friends|Platform")
	bool HasPlatformOverlay() const;

	/** Returns the platform name shown in the buttons that open its overlay (e.g. "Steam"). */
	UFUNCTION(BlueprintPure, Category = "Friends|Platform")
	FText GetPlatformDisplayName() const;

	/** Opens the platform friend list in its overlay (friend requests are sent and answered there on some backends). */
	UFUNCTION(BlueprintCallable, Category = "Friends|Platform")
	void OpenPlatformFriends();

	/**
	 * Opens the profile of a player in the platform overlay, e.g. to answer their friend request there.
	 *
	 * @param NetId  NetId of the entry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Friends|Platform")
	void OpenPlatformProfile(const FString& NetId);

	/**
	 * Opens the platform prompt that sends a friend request to a recent player.
	 *
	 * @param NetId  NetId of a recent player.
	 */
	UFUNCTION(BlueprintCallable, Category = "Friends|Platform")
	void AddFriendOnPlatform(const FString& NetId);

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns true when the loaded map is the lobby level. */
	bool IsInLobbyLevel() const;

	/** Picks the service state from the provider availability and, when a social account is signed in, starts the service. */
	void EvaluateLoginState();

	/** Starts the provider service and reads the friend list. */
	void ActivateService();

	/** Stops the provider service and clears the cached lists. */
	void DeactivateService(EMKHFriendsServiceState NewState);

	/** Friend list read completion. */
	void HandleReadFriendsComplete(bool bSuccess, const FString& Error);

	/** Provider notification: the cached list or a presence changed. */
	void HandleProviderFriendsChanged();

	/** Provider notification: a lobby invite arrived. */
	void HandleLobbyInviteReceived(const FMKHLobbyInvite& Invite);

	/** Coalesces bursts of notifications into one rebuild on the next tick. */
	void RequestCacheRebuild();

	/** Rebuilds Friends from the provider cache and broadcasts OnFriendsChanged. */
	void RebuildFriendCache();

	/** Sorts entries: received requests, then friends by presence, then sent requests; alphabetical inside a group. */
	static void SortFriends(TArray<FMKHFriendInfo>& InOutFriends);

	/** Updates ServiceState and broadcasts OnFriendsChanged when it changed. */
	void SetServiceState(EMKHFriendsServiceState NewState);

	/** Completion of SendFriendRequest: ends the request in flight, broadcasts its result and re-reads the list when it was sent. */
	void FinishFriendRequest(EMKHFriendRequestResult Result);

	/** ResolveLobbyInvite completion of AcceptLobbyInvite: joins the lobby through UMKHSessionSubsystem. */
	void HandleInviteResolved(bool bSuccess, const FMKHLobbySearchResult& Lobby, FString InviteId);

	/** Removes a pending lobby invite and broadcasts OnLobbyInvitesChanged when it was listed. */
	void RemoveLobbyInvite(const FString& InviteId);

	/** Ticker: publishes the activity of the local player when it changed. */
	bool HandleActivityTick(float DeltaTime);

	/**
	 * Fills the round and the score of a match, the local player's score first.
	 *
	 * @param GameState    Game state of the match.
	 * @param OutActivity  Activity to complete.
	 */
	static void ReadMatchScore(const AMKHGameState& GameState, FMKHPlayerActivity& OutActivity);

	/**
	 * Reads how many career levels the local player beat.
	 *
	 * @param InGameInstance  Game instance of the local player.
	 * @return The levels beaten, 0 without the career.
	 */
	static int32 ReadCareerLevels(const UGameInstance& InGameInstance);

	/** Provider notification: relays an avatar change to the UI. */
	void HandleAvatarChanged(const FString& UserId);

	/**
	 * Opens a platform overlay page when the backend has an overlay and the service runs.
	 *
	 * @param Page    Page to open.
	 * @param NetId   User the page is about (ignored by the friend list).
	 */
	void OpenOverlayPage(EMKHSocialOverlayPage Page, const FString& NetId) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Social provider of the online backend; null for a backend without friends. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHSocialProvider> Provider;

private:
	/** Friends and pending requests, sorted for display. */
	TArray<FMKHFriendInfo> Friends;

	/** Players met in recent matches who are not friends, most recent first. */
	TArray<FMKHFriendInfo> RecentPlayers;

	/** Lobby invites received and not answered yet, oldest first. */
	TArray<FMKHLobbyInvite> LobbyInvites;

	/** NetIds of the friends invited to the current lobby. Cleared on every friend list read. */
	TSet<FString> InvitedFriendNetIds;

	/** Activity last handed to the provider (republished on every service start). */
	FMKHPlayerActivity PublishedActivity;

	/** Handle of the activity ticker, running while the service is active. */
	FTSTicker::FDelegateHandle ActivityTickHandle;

	/** Seconds between two activity checks. */
	static constexpr float ActivityTickInterval = 2.f;

	/** Current availability of the service. */
	EMKHFriendsServiceState ServiceState = EMKHFriendsServiceState::Unavailable;

	/** Display name of the friend request in flight; empty when none. */
	FString PendingFriendRequestName;

	/** True while the provider service runs for a signed-in social account. */
	bool bIsServiceActive = false;

	/** True while a rebuild is scheduled for the next tick. */
	bool bCacheRebuildPending = false;
};
