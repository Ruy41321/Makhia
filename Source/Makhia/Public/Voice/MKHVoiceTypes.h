// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHVoiceTypes.generated.h"

/**
 * Types of the voice chat shared by the local voice state (UMKHVoiceChatSubsystem), the server rule
 * (FMKHVoicePermissionPolicy, UMKHVoiceAuthoritySubsystem) and the UI. Full model: see Documentation/VoiceChat.md.
 */

/** How the local microphone transmits while the voice chat is on. */
UENUM(BlueprintType)
enum class EMKHVoiceMicMode : uint8
{
	/** Always open; the engine silence detection decides when voice is sent. */
	OpenMic,
	/** Open only while the push-to-talk key is held. */
	PushToTalk,
	/** Never transmits: the player only listens. */
	Disabled
};

/** Where the voice chat of the local player is connected. */
UENUM(BlueprintType)
enum class EMKHVoiceChannel : uint8
{
	/** No voice: offline, alone, travelling or against the bot. */
	None,
	/** In a networked lobby with other players. */
	Lobby,
	/** In a networked match. */
	Match
};

/**
 * Why the voice between the local player and another one is closed (server rule, Documentation/VoiceChat.md).
 * Seen from the local player: "Other" reasons are about the other player's choices, never who muted whom.
 */
UENUM(BlueprintType)
enum class EMKHVoiceBlockReason : uint8
{
	/** Voice open. */
	None,
	/** The local player turned the voice chat off. */
	VoiceDisabled,
	/** The other player turned the voice chat off (or did not tell the server yet). */
	OtherVoiceDisabled,
	/** Not friends nor party, and the local player does not allow voice with non-friends. */
	NonFriendsNotAllowed,
	/** Not friends nor party, and the other player does not allow voice with non-friends. */
	OtherNonFriendsNotAllowed,
	/** The local player muted or blocked the other one. */
	MutedByYou,
	/** Closed for a reason the local player is not told (e.g. the other player muted them). */
	Unavailable
};

/** Voice choices a machine sends to the server for its own player (validated Server RPC). */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHVoicePreferences
{
	GENERATED_BODY()

	/** Voice chat master switch: false means the player neither hears nor speaks. */
	UPROPERTY(BlueprintReadWrite, Category = "Voice")
	bool bVoiceEnabled = false;

	/** The player accepts voice with players who are neither friends nor in their party. */
	UPROPERTY(BlueprintReadWrite, Category = "Voice")
	bool bAllowNonFriends = false;

	/** The player joined this lobby through an invite or a friend's "Join" (checked by the server, see the policy). */
	UPROPERTY(BlueprintReadWrite, Category = "Voice")
	bool bJoinedAsParty = false;

	/** NetIds of the players muted by the player, blocked users included. */
	UPROPERTY(BlueprintReadWrite, Category = "Voice")
	TArray<FString> MutedPlayerIds;

	/** Returns true when every field is within the limits the server accepts. */
	bool IsWithinLimits() const;

	/** Field-wise equality. */
	bool operator==(const FMKHVoicePreferences& Other) const = default;
};

/** Voice state of another player as decided by the server, replicated only to the player it is about. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHVoicePeerStatus
{
	GENERATED_BODY()

	/** NetId of the other player. */
	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	FString PlayerId;

	/** Why the voice is closed, None when open. */
	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	EMKHVoiceBlockReason Reason = EMKHVoiceBlockReason::Unavailable;

	/** Field-wise equality. */
	bool operator==(const FMKHVoicePeerStatus& Other) const = default;
};

/** One row of the voice panel: a player of the current lobby or match. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHVoiceTalkerInfo
{
	GENERATED_BODY()

	/** NetId of the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	FString PlayerId;

	/** Name shown in the panel. */
	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	FString DisplayName;

	/** True for the local player (always the first row). */
	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	bool bIsLocalPlayer = false;

	/** True while voice of this player is being sent (local) or played (remote). */
	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	bool bIsTalking = false;

	/** True when the local player muted this player (saved) or blocked them on the platform. */
	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	bool bIsMutedByLocalPlayer = false;

	/** True when the player is blocked on the platform (cannot be unmuted from the game). */
	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	bool bIsBlocked = false;

	/** Server decision for this player, None when the voice is open (always None for the local row). */
	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	EMKHVoiceBlockReason BlockReason = EMKHVoiceBlockReason::None;

	/** Field-wise equality. */
	bool operator==(const FMKHVoiceTalkerInfo& Other) const = default;
};
