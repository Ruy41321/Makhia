// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "MKHNetTransportProvider_Steam.h"

#include "Engine/NetConnection.h"
#include "IPAddress.h"
#include "MKHLogChannels.h"
#include "MKHSteamConnectionTracker.h"
#include "MKHSteamShared.h"
#include "Misc/ScopeLock.h"
#include "SteamOnlineDefines.h"

THIRD_PARTY_INCLUDES_START
#include "steam/isteamnetworkingutils.h"
THIRD_PARTY_INCLUDES_END

// ============================================================
// FMKHSteamConnectionTracker
// ============================================================

HSteamNetConnection FMKHSteamConnectionTracker::FindConnection(uint64 RemoteSteamId) const
{
	FScopeLock ScopeLock(&Lock);
	const HSteamNetConnection* Found = Connections.Find(RemoteSteamId);
	return Found ? *Found : k_HSteamNetConnection_Invalid;
}

void FMKHSteamConnectionTracker::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* Data)
{
	const uint64 RemoteSteamId = Data->m_info.m_identityRemote.GetSteamID64();
	if (RemoteSteamId == 0)
	{
		return;
	}

	const ESteamNetworkingConnectionState State = Data->m_info.m_eState;
	const bool bOpen = State == k_ESteamNetworkingConnectionState_Connecting || State == k_ESteamNetworkingConnectionState_FindingRoute
		|| State == k_ESteamNetworkingConnectionState_Connected;

	FScopeLock ScopeLock(&Lock);
	if (bOpen)
	{
		Connections.Add(RemoteSteamId, Data->m_hConn);
	}
	else if (const HSteamNetConnection* Known = Connections.Find(RemoteSteamId); Known && *Known == Data->m_hConn)
	{
		Connections.Remove(RemoteSteamId);
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHNetTransportProvider_Steam::InitializeProvider()
{
	// A running Steam subsystem means the Steam API is initialised and its libraries are loaded.
	ISteamNetworkingUtils* NetworkingUtils = MKHSteam::GetSubsystem() ? SteamNetworkingUtils() : nullptr;
	if (!NetworkingUtils)
	{
		UE_LOG(LogMKHNet, Log, TEXT("Steam transport: Steam is not running, the game connections use plain IP."));
		return;
	}

	const int32 Candidates = k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Private | k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Public;
	const bool bApplied = NetworkingUtils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable, Candidates);
	UE_LOG(LogMKHNet, Log, TEXT("Steam transport: ICE (private + public candidates) %s."), bApplied ? TEXT("enabled") : TEXT("could not be enabled"));

	ConnectionTracker = MakeShared<FMKHSteamConnectionTracker>();
}

void UMKHNetTransportProvider_Steam::ShutdownProvider()
{
	ConnectionTracker.Reset();
}

// ============================================================
// Public Interface
// ============================================================

EMKHConnectionPath UMKHNetTransportProvider_Steam::GetConnectionPath(UNetConnection& Connection) const
{
	if (!IsSteamP2PConnection(Connection))
	{
		return Super::GetConnectionPath(Connection);
	}

	uint32 Handle = k_HSteamNetConnection_Invalid;
	SteamNetConnectionInfo_t Info;
	if (!FindSteamConnection(Connection, Handle) || !SteamNetworkingSockets()->GetConnectionInfo(Handle, &Info)
		|| Info.m_eState != k_ESteamNetworkingConnectionState_Connected)
	{
		return EMKHConnectionPath::Unknown;
	}
	return (Info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Relayed) != 0 ? EMKHConnectionPath::Relayed : EMKHConnectionPath::Direct;
}

FString UMKHNetTransportProvider_Steam::GetRelayLocation(UNetConnection& Connection) const
{
	uint32 Handle = k_HSteamNetConnection_Invalid;
	SteamNetConnectionInfo_t Info;
	if (!FindSteamConnection(Connection, Handle) || !SteamNetworkingSockets()->GetConnectionInfo(Handle, &Info) || Info.m_idPOPRelay == 0)
	{
		return FString();
	}

	char PopCode[8] = {};
	GetSteamNetworkingLocationPOPStringFromID(Info.m_idPOPRelay, PopCode);
	return UTF8_TO_TCHAR(PopCode);
}

bool UMKHNetTransportProvider_Steam::GetConnectionQuality(UNetConnection& Connection, float& OutLocal, float& OutRemote) const
{
	uint32 Handle = k_HSteamNetConnection_Invalid;
	SteamNetConnectionRealTimeStatus_t Status;
	if (!FindSteamConnection(Connection, Handle)
		|| SteamNetworkingSockets()->GetConnectionRealTimeStatus(Handle, &Status, 0, nullptr) != k_EResultOK
		|| Status.m_eState != k_ESteamNetworkingConnectionState_Connected)
	{
		return false;
	}

	// Negative until the first stats exchange of the connection.
	if (Status.m_flConnectionQualityLocal < 0.f || Status.m_flConnectionQualityRemote < 0.f)
	{
		return false;
	}
	OutLocal = FMath::Min(Status.m_flConnectionQualityLocal, 1.f);
	OutRemote = FMath::Min(Status.m_flConnectionQualityRemote, 1.f);
	return true;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHNetTransportProvider_Steam::IsSteamP2PConnection(UNetConnection& Connection)
{
	const TSharedPtr<const FInternetAddr> RemoteAddr = Connection.GetRemoteAddr();
	return RemoteAddr.IsValid() && RemoteAddr->GetProtocolType() == FNetworkProtocolTypes::SteamSocketsP2P;
}

bool UMKHNetTransportProvider_Steam::FindSteamConnection(UNetConnection& Connection, uint32& OutHandle) const
{
	if (!ConnectionTracker.IsValid() || !SteamNetworkingSockets() || !IsSteamP2PConnection(Connection))
	{
		return false;
	}

	// A SteamSockets P2P address prints as the SteamID64 of the other machine.
	const uint64 RemoteSteamId = MKHSteam::ParseSteamId64(Connection.GetRemoteAddr()->ToString(false));
	OutHandle = ConnectionTracker->FindConnection(RemoteSteamId);
	return OutHandle != k_HSteamNetConnection_Invalid;
}
