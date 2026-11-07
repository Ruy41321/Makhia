// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Backend/MKHNetTransportProvider.h"
#include "MKHNetTransportProvider_Steam.generated.h"

class FMKHSteamConnectionTracker;

/**
 * Transport provider on SteamSockets (SteamSocketsNetDriver, IpNetDriver as fallback). At creation it allows the
 * Steam networking library to share private and public ICE candidates, so P2P game connections go direct when the
 * NATs allow it and through a Valve relay (SDR) otherwise; without this Steam applies the user's own defaults.
 * Route (direct or relayed, with the relay data center code) and delivery quality of a SteamSockets connection come
 * from the Steam networking library (FMKHSteamConnectionTracker finds its handle); plain IP connections are direct.
 * Steam does not report NAT types. Full model: see Documentation/OnlineBackends.md and NetworkDiagnostics.md.
 */
UCLASS()
class MAKHIASTEAM_API UMKHNetTransportProvider_Steam : public UMKHNetTransportProvider
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Enables the ICE candidates of the Steam P2P connections (before any listen socket or connection exists). */
	virtual void InitializeProvider() override;

	/** Stops tracking the Steam connections. */
	virtual void ShutdownProvider() override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Direct or Relayed for an established SteamSockets P2P connection (Unknown before), Direct for plain IP. */
	virtual EMKHConnectionPath GetConnectionPath(UNetConnection& Connection) const override;

	/** Code of the Valve data center relaying a SteamSockets connection (e.g. "fra"). */
	virtual FString GetRelayLocation(UNetConnection& Connection) const override;

	/** Local and remote delivery quality of a SteamSockets connection (GetConnectionRealTimeStatus). */
	virtual bool GetConnectionQuality(UNetConnection& Connection, float& OutLocal, float& OutRemote) const override;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns true when the connection goes through a SteamSockets P2P address ("steam.<SteamID64>"). */
	static bool IsSteamP2PConnection(UNetConnection& Connection);

	/**
	 * Reads the Steam networking information of a SteamSockets P2P game connection.
	 *
	 * @param Connection  Game connection.
	 * @param OutHandle   Steam connection handle (uint32 HSteamNetConnection).
	 * @return False for plain IP connections or when Steam has no open connection to that player.
	 */
	bool FindSteamConnection(UNetConnection& Connection, uint32& OutHandle) const;

private:
	/** Steam connections by remote player; null without Steam. */
	TSharedPtr<FMKHSteamConnectionTracker> ConnectionTracker;
};
