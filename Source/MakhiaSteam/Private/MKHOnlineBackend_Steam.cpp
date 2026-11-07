// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "MKHOnlineBackend_Steam.h"

#include "MKHAuthProvider_Steam.h"
#include "MKHNetTransportProvider_Steam.h"
#include "MKHSocialProvider_Steam.h"
#include "MKHSteamShared.h"
#include "HAL/PlatformMisc.h"
#include "Misc/NetworkVersion.h"
#include "Online/Backend/MKHVoiceProvider_OSS.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace MKHSessionSteam
{
	/** Lobby attribute naming the game: AppID 480 lobbies of other games never match it. */
	static const FName GameKey(TEXT("MKH_Game"));

	/** Value of GameKey. */
	static const FString GameValue(TEXT("Makhia"));

	/** Lobby attribute holding the network version: builds that cannot connect to each other never meet. */
	static const FName NetVersionKey(TEXT("MKH_NetVersion"));
}

// ============================================================
// UMKHOnlineBackend_Steam
// ============================================================

FName UMKHOnlineBackend_Steam::GetBackendName() const
{
	static const FName Name(TEXT("Steam"));
	return Name;
}

FMKHOnlineCapabilities UMKHOnlineBackend_Steam::GetCapabilities() const
{
	FMKHOnlineCapabilities Capabilities;
	Capabilities.bHasPlatformOverlay = true;
	Capabilities.bSupportsAvatars = true;
	Capabilities.bSupportsRecentPlayers = true;
	Capabilities.bSupportsVoiceChat = true;
	Capabilities.bSupportsVoiceOutputDevice = true;
	return Capabilities;
}

FText UMKHOnlineBackend_Steam::GetPlatformDisplayName() const
{
	return INVTEXT("Steam");
}

bool UMKHOnlineBackend_Steam::IsRunningOnHandheld() const
{
	// The Steam client sets SteamDeck=1 for every game it starts on a Deck, Proton passes it on; it needs no Steam API.
	if (FPlatformMisc::GetEnvironmentVariable(TEXT("SteamDeck")) == TEXT("1"))
	{
		return true;
	}

	// At first launch this runs before anything started the subsystem: SteamUtils() is null until SteamAPI_Init
	// (and stays null without a Steam client, with -nosteam, in the editor).
	MKHSteam::GetSubsystem();
	return SteamUtils() && SteamUtils()->IsSteamRunningOnSteamDeck();
}

TSubclassOf<UMKHAuthProvider> UMKHOnlineBackend_Steam::GetAuthProviderClass() const
{
	return UMKHAuthProvider_Steam::StaticClass();
}

TSubclassOf<UMKHSessionProvider> UMKHOnlineBackend_Steam::GetSessionProviderClass() const
{
	return UMKHSessionProvider_Steam::StaticClass();
}

TSubclassOf<UMKHSocialProvider> UMKHOnlineBackend_Steam::GetSocialProviderClass() const
{
	return UMKHSocialProvider_Steam::StaticClass();
}

TSubclassOf<UMKHNetTransportProvider> UMKHOnlineBackend_Steam::GetTransportProviderClass() const
{
	return UMKHNetTransportProvider_Steam::StaticClass();
}

TSubclassOf<UMKHVoiceProvider> UMKHOnlineBackend_Steam::GetVoiceProviderClass() const
{
	return UMKHVoiceProvider_OSS::StaticClass();
}

// ============================================================
// UMKHSessionProvider_Steam
// ============================================================

FName UMKHSessionProvider_Steam::GetOnlineSubsystemName() const
{
	return MKHSteam::SubsystemName;
}

void UMKHSessionProvider_Steam::ConfigureSessionSettings(const FMKHLobbySessionSettings& Settings, FOnlineSessionSettings& OutSettings) const
{
	Super::ConfigureSessionSettings(Settings, OutSettings);

	OutSettings.bUseLobbiesVoiceChatIfAvailable = false;

	// Passive lobby: friends-only instead of private, so friends see it (Steam friend list "Join game", the lobby of
	// an invite found through FindFriendSession); lobby searches still only return public (matchmaking) lobbies.
	OutSettings.bAllowJoinViaPresenceFriendsOnly = !Settings.bAdvertise;
	OutSettings.bShouldAdvertise = true;
	OutSettings.Set(MKHSessionSteam::GameKey, MKHSessionSteam::GameValue, EOnlineDataAdvertisementType::ViaOnlineService);
	OutSettings.Set(MKHSessionSteam::NetVersionKey, GetNetVersionValue(), EOnlineDataAdvertisementType::ViaOnlineService);
}

void UMKHSessionProvider_Steam::ConfigureSearch(const FMKHLobbySearchQuery& Query, FOnlineSessionSearch& OutSearch) const
{
	Super::ConfigureSearch(Query, OutSearch);

	// String settings become Steam lobby list filters: the filtering happens on the Steam backend.
	OutSearch.QuerySettings.Set(MKHSessionSteam::GameKey, MKHSessionSteam::GameValue, EOnlineComparisonOp::Equals);
	OutSearch.QuerySettings.Set(MKHSessionSteam::NetVersionKey, GetNetVersionValue(), EOnlineComparisonOp::Equals);
}

FString UMKHSessionProvider_Steam::GetNetVersionValue()
{
	return LexToString(FNetworkVersion::GetLocalNetworkVersion());
}
