// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHOnlineTypes.generated.h"

/**
 * Backend-independent types of the online layer: what the game knows about lobbies, searches and backend features.
 * No backend type (EOS, Steam, OSS) ever appears here. Full model: see Documentation/OnlineBackends.md.
 */

/** Region the matchmaking searches in; only meaningful when the backend supports region selection. */
UENUM(BlueprintType)
enum class EMKHMatchmakingRegion : uint8
{
	/** No region filter: the backend decides. */
	NoSelection,
	Asia,
	NorthAmerica,
	SouthAmerica,
	Africa,
	Europe,
	Australia
};

/** Outcome of joining a lobby found by a search or an invite. */
UENUM(BlueprintType)
enum class EMKHJoinLobbyResult : uint8
{
	/** Joined: the connect string is valid. */
	Success,
	/** The lobby has no free slot. */
	SessionIsFull,
	/** The lobby is gone. */
	SessionDoesNotExist,
	/** Joined, but the host address could not be resolved. */
	CouldNotRetrieveAddress,
	/** A session with the same local name already exists. */
	AlreadyInSession,
	/** Any other backend failure. */
	UnknownError
};

/** Features that differ between backends; the UI and the game layer read them instead of testing the backend. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHOnlineCapabilities
{
	GENERATED_BODY()

	/** The matchmaking region selector is shown (the backend groups lobbies by a region chosen by the player). */
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	bool bSupportsRegionSelection = false;

	/** Friend requests can be sent, accepted and rejected from the game UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	bool bSupportsInGameFriendRequests = false;

	/** The platform has an overlay the game can open (friends, profiles). */
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	bool bHasPlatformOverlay = false;

	/** The transport reports the NAT type of the local machine. */
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	bool bReportsNatType = false;

	/** The social provider gives a profile picture for friends and recent players. */
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	bool bSupportsAvatars = false;

	/** The social provider lists the players met in recent matches, to add them as friends. */
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	bool bSupportsRecentPlayers = false;

	/** The backend carries voice chat between the players of a lobby or a match (UMKHVoiceProvider). */
	UPROPERTY(BlueprintReadOnly, Category = "Online|Voice")
	bool bSupportsVoiceChat = false;

	/** The voice of the other players can be sent to an output device other than the game's. */
	UPROPERTY(BlueprintReadOnly, Category = "Online|Voice")
	bool bSupportsVoiceOutputDevice = false;

	/** The microphone can be chosen in the game (false: the system default communication device is used). */
	UPROPERTY(BlueprintReadOnly, Category = "Online|Voice")
	bool bSupportsVoiceInputDevice = false;
};

/** Lobby attribute keys shared by every backend. Values are always strings. */
struct MAKHIA_API FMKHLobbyKeys
{
	/** Session name of the owner ("<UserId>_LOBBY"): identifies the owner across searches. */
	static const FName SessionName;

	/** "1" while the owner searches an opponent, "0" otherwise. */
	static const FName IsMatchmaking;

	/** Unix time the owner started searching, "No" while not searching. */
	static const FName MatchmakingTime;

	/** Value of a true boolean attribute. */
	static const FString TrueValue;

	/** Value of a false boolean attribute. */
	static const FString FalseValue;
};

/** Settings of a lobby hosted (or updated) by the local player. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHLobbySessionSettings
{
	GENERATED_BODY()

	/** Local name of the session ("<UserId>_LOBBY"). */
	UPROPERTY(BlueprintReadWrite, Category = "Online|Lobby")
	FName SessionName = NAME_None;

	/** Players the lobby holds, host included. */
	UPROPERTY(BlueprintReadWrite, Category = "Online|Lobby")
	int32 MaxPlayers = 2;

	/** True when searches may return the lobby (matchmaking), false for a lobby joined only through invites. */
	UPROPERTY(BlueprintReadWrite, Category = "Online|Lobby")
	bool bAdvertise = false;

	/** Region chosen by the player (see FMKHOnlineCapabilities::bSupportsRegionSelection). */
	UPROPERTY(BlueprintReadWrite, Category = "Online|Lobby")
	EMKHMatchmakingRegion Region = EMKHMatchmakingRegion::NoSelection;

	/** Searchable attributes (see FMKHLobbyKeys). */
	UPROPERTY(BlueprintReadWrite, Category = "Online|Lobby")
	TMap<FName, FString> Attributes;
};

/** What a lobby search looks for. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHLobbySearchQuery
{
	GENERATED_BODY()

	/** Maximum number of lobbies returned. */
	UPROPERTY(BlueprintReadWrite, Category = "Online|Lobby")
	int32 MaxResults = 10;

	/** Region chosen by the player (see FMKHOnlineCapabilities::bSupportsRegionSelection). */
	UPROPERTY(BlueprintReadWrite, Category = "Online|Lobby")
	EMKHMatchmakingRegion Region = EMKHMatchmakingRegion::NoSelection;

	/** Attributes a lobby must have, with exactly these values. */
	UPROPERTY(BlueprintReadWrite, Category = "Online|Lobby")
	TMap<FName, FString> RequiredAttributes;
};

/** Backend data needed to join a search result; each session provider derives its own. */
struct MAKHIA_API FMKHLobbyHandle
{
	virtual ~FMKHLobbyHandle() = default;
};

/** A lobby returned by a search or carried by an accepted invite. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHLobbySearchResult
{
	GENERATED_BODY()

	/** Backend id of the lobby (empty when unknown). */
	UPROPERTY(BlueprintReadOnly, Category = "Online|Lobby")
	FString LobbyId;

	/** Session name of the owner (FMKHLobbyKeys::SessionName attribute). */
	UPROPERTY(BlueprintReadOnly, Category = "Online|Lobby")
	FString OwnerSessionName;

	/** Every attribute of the lobby, as strings. */
	UPROPERTY(BlueprintReadOnly, Category = "Online|Lobby")
	TMap<FName, FString> Attributes;

	/** Players in the lobby. */
	UPROPERTY(BlueprintReadOnly, Category = "Online|Lobby")
	int32 CurrentPlayers = 0;

	/** Players the lobby holds. */
	UPROPERTY(BlueprintReadOnly, Category = "Online|Lobby")
	int32 MaxPlayers = 0;

	/** Backend data of the result; null for an invalid result. */
	TSharedPtr<const FMKHLobbyHandle> Handle;

	/** Returns true when the result can be joined by the provider that produced it. */
	bool IsValid() const { return Handle.IsValid(); }

	/**
	 * Returns an attribute of the lobby.
	 *
	 * @param Key  Attribute key.
	 * @return The value, or an empty string when missing.
	 */
	FString GetAttribute(FName Key) const;

	/** Returns true when the lobby has the attribute. */
	bool HasAttribute(FName Key) const { return Attributes.Contains(Key); }

	/** Returns the FMKHLobbyKeys::MatchmakingTime attribute as a number, 0 when missing or not a number. */
	int64 GetMatchmakingTime() const;
};

/** Completion of a session operation (host, update, destroy). Runs exactly once, possibly inline. */
DECLARE_DELEGATE_OneParam(FMKHSessionOperationComplete, bool /*bSuccess*/);

/** Completion of a lobby search. Runs exactly once, possibly inline. */
DECLARE_DELEGATE_TwoParams(FMKHLobbySearchComplete, bool /*bSuccess*/, const TArray<FMKHLobbySearchResult>& /*Results*/);

/** Completion of a lobby join. Runs exactly once, possibly inline. */
DECLARE_DELEGATE_TwoParams(FMKHLobbyJoinComplete, EMKHJoinLobbyResult /*Result*/, const FString& /*ConnectString*/);

/** A platform invite was accepted (overlay, platform chat, command line). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FMKHLobbyInviteAcceptedSignature, bool /*bSuccess*/, const FMKHLobbySearchResult& /*Lobby*/);
