// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHNetDiagnosticsTypes.generated.h"

/** NAT type of a machine, when the transport of the online backend reports it (EOS P2P does, Steam does not). */
UENUM(BlueprintType)
enum class EMKHNatType : uint8
{
	/** Not measured yet, not signed in, not reported by the backend, or the backend could not tell. */
	Unknown,
	/** Every peer can reach this machine directly. */
	Open,
	/** Direct connections work with Open and Moderate peers. */
	Moderate,
	/** Direct connections only work with Open peers; others go through a relay. */
	Strict
};

/** Route the game traffic takes to the other machine. */
UENUM(BlueprintType)
enum class EMKHConnectionPath : uint8
{
	/** The transport has not reported the connection yet. */
	Unknown,
	/** Peer to peer over the internet or the local network (or a plain IP connection). */
	Direct,
	/** Through a relay server of the platform (Epic, Valve): adds latency. */
	Relayed
};

/** What the network stats overlay shows (user setting "Network Stats"). */
UENUM(BlueprintType)
enum class EMKHNetStatsMode : uint8
{
	/** Overlay hidden. */
	Off,
	/** Ping, signal bars, host/client chip and relay warning. */
	Compact,
	/** Compact row plus jitter, packet loss, route (with relay location), transport quality and NAT types when reported. */
	Detailed
};

/** Live measurements of the game connection, read by the network stats overlay. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHNetStatsSnapshot
{
	GENERATED_BODY()

	/** True while this machine is connected to another player (false offline, vs bot, or alone). */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	bool bConnected = false;

	/** True on the listen server: the values then describe the connection to the client. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	bool bIsHost = false;

	/** Average round trip time over the last second, in milliseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	int32 PingMs = 0;

	/** Average variation of the packet timing, in milliseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	int32 JitterMs = 0;

	/** Incoming packet loss, rolling average in [0, 1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	float PacketLossIn = 0.f;

	/** Outgoing packet loss, rolling average in [0, 1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	float PacketLossOut = 0.f;

	/** Direct or relayed route of the connection. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	EMKHConnectionPath Path = EMKHConnectionPath::Unknown;

	/** Location of the relay the traffic goes through (e.g. a Valve data center code), empty when unknown or direct. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	FString RelayLocation;

	/** True when the backend reports NAT types (FMKHOnlineCapabilities::bReportsNatType); the overlay hides them otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	bool bNatTypeReported = false;

	/** NAT type of this machine. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	EMKHNatType LocalNatType = EMKHNatType::Unknown;

	/** NAT type of the other player, replicated through their player state. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	EMKHNatType RemoteNatType = EMKHNatType::Unknown;

	/** True when the transport measures the connection quality (Steam); the overlay hides it otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	bool bQualityReported = false;

	/** Share of packets delivered end to end, measured by this machine, in [0, 1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	float QualityLocal = 0.f;

	/** Share of packets delivered end to end, measured by the other machine, in [0, 1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Network")
	float QualityRemote = 0.f;
};

/** Broadcast when the NAT type of this machine is measured or changes. */
DECLARE_MULTICAST_DELEGATE_OneParam(FMKHOnLocalNatTypeChanged, EMKHNatType /*NewNatType*/);
