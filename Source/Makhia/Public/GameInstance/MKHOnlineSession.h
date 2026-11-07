// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineSession.h"
#include "MKHOnlineSession.generated.h"

/**
 * Online session used by UMKHGameInstance.
 *
 * Exists to replace the engine's default disconnect handling: the base class browses to the
 * default map, which drops the client into the lobby level with stale session state and no
 * hosted lobby. This routes the disconnect through UMKHGameInstance::BackToLobby instead.
 */
UCLASS()
class MAKHIA_API UMKHOnlineSession : public UOnlineSession
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Called by the engine when the connection to the server is lost or a travel fails.
	 * Returns to the lobby as host so the player re-creates their own lobby on arrival.
	 * When the failing driver is our own listen socket the lobby is reopened offline instead:
	 * hosting again would fail the same way and loop.
	 * Ignored while a return to the lobby is already in progress (the engine repeats the failure every tick).
	 * Does not call Super: the base implementation would browse to the default map and
	 * race with the travel started here.
	 *
	 * @param World     World the failing net driver belonged to.
	 * @param NetDriver Net driver that reported the failure.
	 */
	virtual void HandleDisconnect(UWorld* World, UNetDriver* NetDriver) override;
};
