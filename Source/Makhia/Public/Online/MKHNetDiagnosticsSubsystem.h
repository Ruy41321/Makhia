// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Online/MKHNetDiagnosticsTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHNetDiagnosticsSubsystem.generated.h"

class AMKHPlayerState;
class UMKHNetTransportProvider;
class UNetConnection;

/**
 * Network diagnostics of the P2P game connection (log LogMKHNet).
 *
 * The transport provider of the online backend (UMKHNetTransportProvider) tells the NAT type of this machine and
 * whether the traffic goes direct or through a relay (and where). Every second this subsystem refreshes the
 * provider, samples the game connection, and logs a ping summary when the connection ends.
 * The network stats overlay (UMKHNetStatsWidget) reads everything through GetNetStats().
 * Full model in Documentation/NetworkDiagnostics.md.
 */
UCLASS()
class MAKHIA_API UMKHNetDiagnosticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Creates the transport provider of the online backend and starts the one-second ticker. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Logs the last ping summary, stops the ticker and releases the transport provider. */
	virtual void Deinitialize() override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Returns the diagnostics subsystem of the game instance that owns the given object.
	 *
	 * @param WorldContext Any object living in a game world.
	 * @return The subsystem, or nullptr without a game instance.
	 */
	static UMKHNetDiagnosticsSubsystem* Get(const UObject* WorldContext);

	/** NAT type of this machine; Unknown until measured, and always on backends that do not report it. */
	UFUNCTION(BlueprintPure, Category = "Online|Network Diagnostics")
	EMKHNatType GetLocalNatType() const;

	/**
	 * Reads the live measurements of the game connection of the current world.
	 * bConnected is false offline, against the bot or while alone on a listen server.
	 */
	UFUNCTION(BlueprintPure, Category = "Online|Network Diagnostics")
	FMKHNetStatsSnapshot GetNetStats() const;

	/** Fired when the local NAT type is measured or changes (the player controller replicates it). */
	FMKHOnLocalNatTypeChanged OnLocalNatTypeChanged;

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** One-second ticker: refreshes the transport provider and samples the game connection. */
	bool HandleTick(float DeltaTime);

	/** Provider notification: relays the new local NAT type. */
	void HandleLocalNatTypeChanged(EMKHNatType NewNatType);

	/** Adds the current ping to the running summary; logs the summary when the connection changed. */
	void SampleGameConnection();

	/**
	 * Remembers the route of the sampled connection and logs it when it changed (direct/relayed or relay location).
	 *
	 * @param Connection  Sampled game connection.
	 * @param NewPath     Route reported by the transport now.
	 */
	void LogRouteChange(UNetConnection& Connection, EMKHConnectionPath NewPath);

	/** Logs average and peak ping of the connection sampled so far, then clears the samples. */
	void LogConnectionSummary();

	/**
	 * Finds the connection to the other player in the current world.
	 *
	 * @param bOutIsHost Set to true when this machine is the listen server.
	 * @return The client's server connection, the host's first client connection, or nullptr.
	 */
	UNetConnection* FindGameConnection(bool& bOutIsHost) const;

	/** Route of a connection, from the transport provider (Direct without one). */
	EMKHConnectionPath GetConnectionPath(UNetConnection& Connection) const;

	/** Player state of the other human player, or nullptr. */
	const AMKHPlayerState* FindRemotePlayerState() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Transport provider of the online backend. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHNetTransportProvider> Transport;

	/** True when the backend reports NAT types (FMKHOnlineCapabilities::bReportsNatType). */
	bool bNatTypeReported = false;

	/** Seconds between two ticks of HandleTick. */
	static constexpr float TickInterval = 1.f;

	/** Handle of the ticker started in Initialize. */
	FTSTicker::FDelegateHandle TickHandle;

	/** Connection the ping summary is being built for. */
	TWeakObjectPtr<UNetConnection> SampledConnection;

	/** Remote address of SampledConnection, kept for the summary after the connection is gone. */
	FString SampledRemoteAddress;

	/** Running ping summary of SampledConnection. */
	double PingSumMs = 0.0;
	int32 PingMaxMs = 0;
	int32 PingSampleCount = 0;
	EMKHConnectionPath SampledPath = EMKHConnectionPath::Unknown;

	/** Relay location of SampledConnection last logged, empty when direct or unknown. */
	FString SampledRelayLocation;
};
