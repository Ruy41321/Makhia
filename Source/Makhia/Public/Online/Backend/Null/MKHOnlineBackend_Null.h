// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Backend/MKHOnlineBackend.h"
#include "Online/Backend/MKHSessionProvider_OSS.h"
#include "MKHOnlineBackend_Null.generated.h"

/**
 * Development backend on the engine's Null online subsystem: LAN lobbies and IP connections, no accounts, no
 * friends. Lets two processes on the same PC test the lobby and the matchmaking (-MKHOnline=Null), which a platform
 * backend like Steam cannot do. Also the fallback when the configured backend cannot be loaded; logins are refused
 * in Shipping builds, so there it means offline only. Full model: see Documentation/OnlineBackends.md.
 */
UCLASS()
class MAKHIA_API UMKHOnlineBackend_Null : public UMKHOnlineBackend
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Returns "Null". */
	virtual FName GetBackendName() const override;

	/** No region, no friend requests, no overlay, no NAT report; voice chat on the LAN connection. */
	virtual FMKHOnlineCapabilities GetCapabilities() const override;

	/** Returns UMKHAuthProvider_Null. */
	virtual TSubclassOf<UMKHAuthProvider> GetAuthProviderClass() const override;

	/** Returns UMKHSessionProvider_Null. */
	virtual TSubclassOf<UMKHSessionProvider> GetSessionProviderClass() const override;

	/** Returns UMKHVoiceProvider_OSS (native voice of the Null subsystem). */
	virtual TSubclassOf<UMKHVoiceProvider> GetVoiceProviderClass() const override;
};

/**
 * Session provider on the Null online subsystem: LAN sessions found through the LAN beacon. The Null subsystem
 * neither filters searches by attribute nor re-advertises after an update, so the lobby is always advertised and the
 * base provider filters the results by attribute.
 */
UCLASS()
class MAKHIA_API UMKHSessionProvider_Null : public UMKHSessionProvider_OSS
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns "NULL". */
	virtual FName GetOnlineSubsystemName() const override;

	/** LAN match, always advertised (the attributes tell a passive lobby from a matchmaking one). */
	virtual void ConfigureSessionSettings(const FMKHLobbySessionSettings& Settings, FOnlineSessionSettings& OutSettings) const override;

	/** LAN query. */
	virtual void ConfigureSearch(const FMKHLobbySearchQuery& Query, FOnlineSessionSearch& OutSearch) const override;

	/** Counts the host: a Null session never registers its own host as a player. */
	virtual void ReadSearchResult(const FOnlineSessionSearchResult& Result, FMKHLobbySearchResult& OutLobby) const override;
};
