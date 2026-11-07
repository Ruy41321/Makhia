// Copyright (c) 2025 Luigi Pennisi. All rights reserved.


#include "GameInstance/MKHOnlineSession.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "GameInstance/MKHGameInstance.h"
#include "MKHLogChannels.h"

void UMKHOnlineSession::HandleDisconnect(UWorld* World, UNetDriver* NetDriver)
{
	UMKHGameInstance* GameInstance = Cast<UMKHGameInstance>(GetOuter());
	if (!IsValid(GameInstance))
	{
		UE_LOG(LogMKHSession, Warning, TEXT("HandleDisconnect: no MKH game instance, falling back to the default handling."));
		Super::HandleDisconnect(World, NetDriver);
		return;
	}

	// A closing connection repeats its failure every tick until the travel starts: the first call already returns.
	if (GameInstance->IsReturningToLobby())
	{
		UE_LOG(LogMKHSession, Verbose, TEXT("HandleDisconnect: return to the lobby already in progress, ignored."));
		return;
	}

	// A driver without a server connection is our own listen socket: hosting again would fail the
	// same way and loop forever, so the lobby is reopened offline instead.
	if (IsValid(NetDriver) && !IsValid(NetDriver->ServerConnection))
	{
		UE_LOG(LogMKHSession, Error, TEXT("HandleDisconnect: could not host (listen failed), reopening the lobby offline. Is the player logged in on this world's online subsystem?"));
		GameInstance->BackToLobby(false);
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("HandleDisconnect: connection lost, returning to the lobby as host."));

	// Returns as host so the lobby is re-created on arrival, mirroring the post-login flow.
	GameInstance->BackToLobby(true);
}
