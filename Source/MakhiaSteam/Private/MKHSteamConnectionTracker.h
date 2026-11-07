// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/isteamnetworkingsockets.h"
THIRD_PARTY_INCLUDES_END

/**
 * Remembers the Steam networking connection of each remote player, so the transport provider can read route and
 * quality of a game connection: the SteamSockets net driver keeps its connection handles private. Fed by the
 * connection status notification, which Steam dispatches on the online thread (hence the lock).
 * Registered on construction, unregistered on destruction.
 */
class FMKHSteamConnectionTracker
{
public:
	/**
	 * Returns the live Steam connection to a remote player.
	 *
	 * @param RemoteSteamId  SteamID64 of the other machine.
	 * @return The connection handle, or k_HSteamNetConnection_Invalid when none is open.
	 */
	HSteamNetConnection FindConnection(uint64 RemoteSteamId) const;

private:
	/** Guards Connections (written on the online thread, read on the game thread). */
	mutable FCriticalSection Lock;

	/** Open connections by remote SteamID64. */
	TMap<uint64, HSteamNetConnection> Connections;

	/** A connection changed state: opened ones are remembered, closed ones forgotten. Declared last: registers once the map exists. */
	STEAM_CALLBACK(FMKHSteamConnectionTracker, OnConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t);
};
