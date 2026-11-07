// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/MKHNetDiagnosticsTypes.h"
#include "UObject/Object.h"
#include "MKHNetTransportProvider.generated.h"

class UNetConnection;

/**
 * Transport side of the online backend, read by UMKHNetDiagnosticsSubsystem: route of the game connection (direct
 * or relayed, relay location) and NAT type of this machine. This base class is the plain IP transport (Null
 * backend, IpNetDriver fallback): every connection is direct and no NAT type is measured. Platform backends derive
 * from it. Full model: see Documentation/OnlineBackends.md and Documentation/NetworkDiagnostics.md.
 */
UCLASS()
class MAKHIA_API UMKHNetTransportProvider : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Called once right after creation. */
	virtual void InitializeProvider() {}

	/** Called once before release: unbinds every backend notification. */
	virtual void ShutdownProvider() {}

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Broadcast when the NAT type of this machine is measured or changes. */
	FMKHOnLocalNatTypeChanged OnLocalNatTypeChanged;

	/** Called every second by the diagnostics subsystem: follows sign-ins and refreshes the backend registration. */
	virtual void Refresh() {}

	/** Returns the NAT type of this machine, Unknown when not measured or not reported by the backend. */
	virtual EMKHNatType GetLocalNatType() const { return EMKHNatType::Unknown; }

	/**
	 * Returns the route of a game connection.
	 *
	 * @param Connection  Server connection (client) or client connection (host).
	 * @return Direct for a plain IP connection; platform transports report Direct or Relayed (Unknown until known).
	 */
	virtual EMKHConnectionPath GetConnectionPath(UNetConnection& Connection) const { return EMKHConnectionPath::Direct; }

	/**
	 * Returns where the relay of a relayed connection is (e.g. a Valve data center code).
	 *
	 * @param Connection  Game connection.
	 * @return The location, or an empty string when direct or unknown.
	 */
	virtual FString GetRelayLocation(UNetConnection& Connection) const { return FString(); }

	/**
	 * Reads the delivery quality the transport measures on a game connection (share of packets delivered end to end).
	 *
	 * @param Connection     Game connection.
	 * @param OutLocal       Quality measured by this machine, in [0, 1].
	 * @param OutRemote      Quality measured by the other machine, in [0, 1].
	 * @return False when the transport does not measure it (plain IP) or the connection is not established.
	 */
	virtual bool GetConnectionQuality(UNetConnection& Connection, float& OutLocal, float& OutRemote) const { return false; }
};
