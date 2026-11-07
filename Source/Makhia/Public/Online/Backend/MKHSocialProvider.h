// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Backend/MKHOnlineTypes.h"
#include "Online/MKHFriendTypes.h"
#include "UObject/Object.h"
#include "MKHSocialProvider.generated.h"

class UTexture2D;

/** Whether the friends service of the backend can be used right now. */
enum class EMKHSocialAvailability : uint8
{
	/** The backend has no friends service. */
	Unavailable,
	/** Nobody is signed in. */
	SignedOut,
	/** Signed in with an account that has no friends (e.g. EOS device login). */
	NoSocialAccount,
	/** Signed in with a social account. */
	Available
};

/** Completion of a friend list read. Runs exactly once, possibly inline. */
DECLARE_DELEGATE_TwoParams(FMKHFriendsReadComplete, bool /*bSuccess*/, const FString& /*Error*/);

/** Completion of a friend request sent by display name. Runs exactly once, possibly inline. */
DECLARE_DELEGATE_OneParam(FMKHFriendRequestComplete, EMKHFriendRequestResult /*Result*/);

/** Completion of the lookup of the lobby an invite points at. May run more than once on some backends. */
DECLARE_DELEGATE_TwoParams(FMKHLobbyInviteResolved, bool /*bSuccess*/, const FMKHLobbySearchResult& /*Lobby*/);

/** A lobby invite arrived (sender name filled when the sender is a known friend). */
DECLARE_MULTICAST_DELEGATE_OneParam(FMKHLobbyInviteReceivedSignature, const FMKHLobbyInvite& /*Invite*/);

/** The profile picture of a user finished loading or changed. */
DECLARE_MULTICAST_DELEGATE_OneParam(FMKHAvatarChangedSignature, const FString& /*UserId*/);

/** Platform overlay pages the game can open (FMKHOnlineCapabilities::bHasPlatformOverlay). */
enum class EMKHSocialOverlayPage : uint8
{
	/** The platform friend list. */
	Friends,
	/** The profile of a user (e.g. to answer their friend request). */
	Profile,
	/** The prompt that sends a friend request to a user. */
	AddFriend
};

/**
 * Backend behind UMKHFriendsSubsystem: friend list, presence, friend requests, the lobby invites received and, when the
 * capabilities say so, avatars, recent players and the platform overlay.
 * The online backend (UMKHOnlineBackend) names the class; the subsystem owns the cache, the sorting and the UI
 * delegates. Lobby invites are sent and joined through UMKHSessionSubsystem.
 * Full model: see Documentation/OnlineBackends.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSocialProvider : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Called once right after creation: binds the sign-in notifications. */
	virtual void InitializeProvider() {}

	/** Called once before release: stops the service and unbinds everything. */
	virtual void ShutdownProvider() {}

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Broadcast when the sign-in state changed: GetAvailability may answer differently. */
	FSimpleMulticastDelegate OnLoginStateChanged;

	/** Broadcast when the cached friend list or a presence changed: GetCachedFriends has new data. */
	FSimpleMulticastDelegate OnFriendsChanged;

	/** Broadcast when the friend list must be read again (request received, friend removed...). */
	FSimpleMulticastDelegate OnFriendsListStale;

	/** Broadcast when a lobby invite arrives while the service runs. */
	FMKHLobbyInviteReceivedSignature OnLobbyInviteReceived;

	/** Broadcast when an avatar asked through GetAvatar is ready or changed. */
	FMKHAvatarChangedSignature OnAvatarChanged;

	/** Returns whether the friends service can be used now. */
	virtual EMKHSocialAvailability GetAvailability() const PURE_VIRTUAL(UMKHSocialProvider::GetAvailability, return EMKHSocialAvailability::Unavailable;);

	/** Starts listening to friends, presence and lobby invites (the player signed in with a social account). */
	virtual void StartService() {}

	/** Stops listening and forgets the cached data. */
	virtual void StopService() {}

	/**
	 * Reads the friend list from the backend.
	 *
	 * @param OnComplete  Receives the outcome; GetCachedFriends has the list on success.
	 */
	virtual void ReadFriends(const FMKHFriendsReadComplete& OnComplete) PURE_VIRTUAL(UMKHSocialProvider::ReadFriends, );

	/**
	 * Converts the cached friend list into UI entries (friends and pending requests; blocked or unknown ones skipped).
	 * bInvitedToLobby is left false: the subsystem tracks the invites it sent.
	 *
	 * @param OutFriends  Entries, unsorted.
	 */
	virtual void GetCachedFriends(TArray<FMKHFriendInfo>& OutFriends) const PURE_VIRTUAL(UMKHSocialProvider::GetCachedFriends, );

	/**
	 * Accepts a friend request (only with FMKHOnlineCapabilities::bSupportsInGameFriendRequests).
	 *
	 * @param FriendId    NetId of the sender.
	 * @param OnComplete  Runs when the backend answered (the list is then read again).
	 */
	virtual void AcceptFriendRequest(const FString& FriendId, const FSimpleDelegate& OnComplete) { OnComplete.ExecuteIfBound(); }

	/**
	 * Rejects a friend request (only with FMKHOnlineCapabilities::bSupportsInGameFriendRequests).
	 *
	 * @param FriendId  NetId of the sender.
	 * @return True when the backend took the request.
	 */
	virtual bool RejectFriendRequest(const FString& FriendId) { return false; }

	/**
	 * Sends a friend request by exact display name (only with FMKHOnlineCapabilities::bSupportsInGameFriendRequests).
	 *
	 * @param DisplayName  Display name of the player to add, trimmed and not empty.
	 * @param OnComplete   Receives the outcome.
	 */
	virtual void SendFriendRequest(const FString& DisplayName, const FMKHFriendRequestComplete& OnComplete)
	{
		OnComplete.ExecuteIfBound(EMKHFriendRequestResult::Failed);
	}

	/**
	 * Finds the lobby a received invite points at, ready to be joined by UMKHSessionSubsystem.
	 *
	 * @param Invite      Invite received through OnLobbyInviteReceived.
	 * @param OnResolved  Receives the lobby.
	 */
	virtual void ResolveLobbyInvite(const FMKHLobbyInvite& Invite, const FMKHLobbyInviteResolved& OnResolved)
	{
		OnResolved.ExecuteIfBound(false, FMKHLobbySearchResult());
	}

	/**
	 * Declines a received invite on the backend when it supports it (the subsystem drops it locally anyway).
	 *
	 * @param Invite  Invite to decline.
	 */
	virtual void DeclineLobbyInvite(const FMKHLobbyInvite& Invite) {}

	/**
	 * Returns the profile picture of a user (only with FMKHOnlineCapabilities::bSupportsAvatars).
	 * A picture not loaded yet is requested: OnAvatarChanged reports it when ready.
	 *
	 * @param UserId  NetId of a friend or a recent player.
	 * @return The picture, or null while loading or when the user has none.
	 */
	virtual UTexture2D* GetAvatar(const FString& UserId) { return nullptr; }

	/**
	 * Lists the players met in recent matches who are not friends (only with
	 * FMKHOnlineCapabilities::bSupportsRecentPlayers), most recent first.
	 *
	 * @param OutPlayers  Entries with the RecentPlayer relation.
	 */
	virtual void GetRecentPlayers(TArray<FMKHFriendInfo>& OutPlayers) const {}

	/**
	 * Tells the platform the local player is playing a match with these users (recent players list).
	 *
	 * @param UserIds  NetIds of the other human players of the match.
	 */
	virtual void ReportPlayedWith(const TArray<FString>& UserIds) {}

	/**
	 * Opens a page of the platform overlay (only with FMKHOnlineCapabilities::bHasPlatformOverlay).
	 *
	 * @param Page    Page to open.
	 * @param UserId  NetId of the user the page is about; ignored by Friends.
	 * @return True when the overlay was asked to open.
	 */
	virtual bool OpenOverlay(EMKHSocialOverlayPage Page, const FString& UserId) { return false; }

	/**
	 * Returns true when the local player blocked a user on the platform (their voice is always muted).
	 *
	 * @param UserId  NetId of the user.
	 * @return False on backends without blocking.
	 */
	virtual bool IsBlocked(const FString& UserId) const { return false; }

	/**
	 * Publishes what the local player is doing to their friends (read back into FMKHFriendInfo::Activity on their
	 * side). Called while the service runs, only when the activity changed.
	 *
	 * @param Activity  Current activity of the local player.
	 */
	virtual void SetLocalActivity(const FMKHPlayerActivity& Activity) {}
};
