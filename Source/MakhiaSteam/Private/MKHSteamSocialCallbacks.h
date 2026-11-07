// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

class UMKHSocialProvider_Steam;

/**
 * Steam friends and matchmaking notifications of UMKHSocialProvider_Steam. Steam dispatches them on the online
 * thread of the Steam online subsystem (SteamAPI_RunCallbacks): each one is copied and handed to the provider on the
 * game thread. Registered on construction, unregistered on destruction.
 */
class FMKHSteamSocialCallbacks
{
public:
	/**
	 * Registers the notifications.
	 *
	 * @param InOwner  Provider receiving them on the game thread.
	 */
	explicit FMKHSteamSocialCallbacks(UMKHSocialProvider_Steam* InOwner);

private:
	/** Provider receiving the notifications (declared first: set before the callbacks register). */
	TWeakObjectPtr<UMKHSocialProvider_Steam> Owner;

	/**
	 * Runs a provider handler on the game thread, if the provider still exists then.
	 *
	 * @param Handler  Handler to run.
	 */
	void RunOnGameThread(TUniqueFunction<void(UMKHSocialProvider_Steam&)>&& Handler) const;

	/** Name, status, game, avatar or relationship of a user changed. */
	STEAM_CALLBACK(FMKHSteamSocialCallbacks, OnPersonaStateChange, PersonaStateChange_t);

	/** Rich presence of a friend changed. */
	STEAM_CALLBACK(FMKHSteamSocialCallbacks, OnFriendRichPresenceUpdate, FriendRichPresenceUpdate_t);

	/** An avatar requested with GetMediumFriendAvatar finished downloading. */
	STEAM_CALLBACK(FMKHSteamSocialCallbacks, OnAvatarImageLoaded, AvatarImageLoaded_t);

	/** A member entered or left a lobby the local player is in. */
	STEAM_CALLBACK(FMKHSteamSocialCallbacks, OnLobbyChatUpdate, LobbyChatUpdate_t);

	/** A friend invited the local player to a lobby. */
	STEAM_CALLBACK(FMKHSteamSocialCallbacks, OnLobbyInvite, LobbyInvite_t);
};
