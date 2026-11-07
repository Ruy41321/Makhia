// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHFriendTypes.generated.h"

/** Online state of a friend, from the most to the least available. */
UENUM(BlueprintType)
enum class EMKHFriendPresence : uint8
{
	/** Playing Makhia right now: the only state that can receive a lobby invite. */
	InMakhia,
	/** Online on the platform, in another game or in its launcher. */
	Online,
	/** Online but marked away. */
	Away,
	/** Not connected. */
	Offline,
	/** Playing Makhia in the same lobby as the local player (listed first, no invite). */
	InYourParty
};

/** What a player is doing in Makhia, as published to their friends. */
UENUM(BlueprintType)
enum class EMKHPlayerActivity : uint8
{
	/** Not published (not in Makhia, or a backend that does not share it). */
	None,
	/** In the lobby, alone or with other players. */
	InLobby,
	/** In a match against the bot. */
	OfflineMatch,
	/** In a match against another player. */
	OnlineMatch
};

/**
 * Activity a player publishes to their friends (platform rich presence): where they are in the game and, during a
 * match, the round and the score seen from their side (their own score first).
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHPlayerActivity
{
	GENERATED_BODY()

	/** Lobby or match. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends|Activity")
	EMKHPlayerActivity Activity = EMKHPlayerActivity::None;

	/** InLobby: players in the lobby, the player included. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends|Activity")
	int32 LobbyPlayers = 0;

	/** InLobby: players the lobby holds. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends|Activity")
	int32 LobbyMaxPlayers = 0;

	/** Matches: current round (1 for the first duel), 0 before the match starts. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends|Activity")
	int32 Round = 0;

	/** Matches: rounds won by the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends|Activity")
	int32 OwnScore = 0;

	/** Matches: rounds won by the player's opponent. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends|Activity")
	int32 OpponentScore = 0;

	/** Career levels the player beat: each machine turns them into the title in its own language. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends|Activity")
	int32 CareerLevels = 0;

	/** Returns true when both activities would be published the same way. */
	bool operator==(const FMKHPlayerActivity& Other) const
	{
		return Activity == Other.Activity && LobbyPlayers == Other.LobbyPlayers && LobbyMaxPlayers == Other.LobbyMaxPlayers
			&& Round == Other.Round && OwnScore == Other.OwnScore && OpponentScore == Other.OpponentScore
			&& CareerLevels == Other.CareerLevels;
	}
};

/** Relationship between the local player and an entry of the friend list. */
UENUM(BlueprintType)
enum class EMKHFriendRelation : uint8
{
	/** Accepted friendship. */
	Friend,
	/** The other player asked to become friends; the local player can accept or reject. */
	RequestReceived,
	/** The local player asked to become friends and is waiting for an answer. */
	RequestSent,
	/** Not a friend: met in a recent match (FMKHOnlineCapabilities::bSupportsRecentPlayers), can be added on the platform. */
	RecentPlayer
};

/** Availability of the friends service, used by the UI to pick the message to show. */
UENUM(BlueprintType)
enum class EMKHFriendsServiceState : uint8
{
	/** The online backend has no friends service (disabled, or a backend without friends like Null). */
	Unavailable,
	/** No player is signed in. */
	SignedOut,
	/** Signed in without a social account (e.g. EOS device login): friends need the platform account. */
	NoSocialAccount,
	/** The friend list is being read. */
	Loading,
	/** The friend list is up to date. */
	Ready,
	/** The last read failed. */
	Error
};

/** Outcome of a friend request sent by display name. */
UENUM(BlueprintType)
enum class EMKHFriendRequestResult : uint8
{
	/** The request was delivered to the backend. */
	Sent,
	/** The name was empty, or is the local player's own name. */
	InvalidName,
	/** No account has that display name. */
	UserNotFound,
	/** That player is already a friend or already has a pending request. */
	AlreadyFriends,
	/** The backend refused the request (e.g. the app lacks the Friends Management permission). */
	Failed
};

/** One entry of the friend list, as shown by the UI. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHFriendInfo
{
	GENERATED_BODY()

	/** Opaque backend id of the friend (EOS: "EpicAccountId|ProductUserId", Steam: SteamID64) for every action on it. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	FString NetId;

	/** Platform display name. Player data: shown as culture invariant text. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	FString DisplayName;

	/** Current online state. Always Offline for pending requests (presence is only shared between friends). */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	EMKHFriendPresence Presence = EMKHFriendPresence::Offline;

	/** Friendship or pending request. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	EMKHFriendRelation Relation = EMKHFriendRelation::Friend;

	/** True when the friend can receive a lobby invite of this game (EOS: has a product user id, i.e. played Makhia). */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	bool bHasPlayedMakhia = false;

	/** True after the local player invited this friend to the current lobby. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	bool bInvitedToLobby = false;

	/** What the friend is doing in Makhia (InMakhia / InYourParty friends of backends that share it). */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	FMKHPlayerActivity Activity;

	/** Recent players only: when the last match together was played (UTC). */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	FDateTime LastPlayedTogether;
};

/** An invitation to join another player's lobby, received while signed in. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHLobbyInvite
{
	GENERATED_BODY()

	/** Backend id of the invite (EOS invite id, Steam lobby id), used to accept or decline it. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	FString InviteId;

	/** Backend id of the player who sent the invitation (EOS product user id, SteamID64). */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	FString SenderId;

	/** Display name of the sender, empty when the sender is not in the friend list. Player data: culture invariant. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	FString SenderDisplayName;
};
