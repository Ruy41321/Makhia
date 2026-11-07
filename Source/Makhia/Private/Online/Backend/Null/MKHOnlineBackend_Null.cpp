// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Backend/Null/MKHOnlineBackend_Null.h"

#include "Online/Backend/MKHVoiceProvider_OSS.h"
#include "Online/Backend/Null/MKHAuthProvider_Null.h"

// ============================================================
// UMKHOnlineBackend_Null
// ============================================================

FName UMKHOnlineBackend_Null::GetBackendName() const
{
	static const FName Name(TEXT("Null"));
	return Name;
}

FMKHOnlineCapabilities UMKHOnlineBackend_Null::GetCapabilities() const
{
	FMKHOnlineCapabilities Capabilities;
	Capabilities.bSupportsVoiceChat = true;
	Capabilities.bSupportsVoiceOutputDevice = true;
	return Capabilities;
}

TSubclassOf<UMKHAuthProvider> UMKHOnlineBackend_Null::GetAuthProviderClass() const
{
	return UMKHAuthProvider_Null::StaticClass();
}

TSubclassOf<UMKHSessionProvider> UMKHOnlineBackend_Null::GetSessionProviderClass() const
{
	return UMKHSessionProvider_Null::StaticClass();
}

TSubclassOf<UMKHVoiceProvider> UMKHOnlineBackend_Null::GetVoiceProviderClass() const
{
	return UMKHVoiceProvider_OSS::StaticClass();
}

// ============================================================
// UMKHSessionProvider_Null
// ============================================================

FName UMKHSessionProvider_Null::GetOnlineSubsystemName() const
{
	static const FName Name(TEXT("NULL"));
	return Name;
}

void UMKHSessionProvider_Null::ConfigureSessionSettings(const FMKHLobbySessionSettings& Settings, FOnlineSessionSettings& OutSettings) const
{
	Super::ConfigureSessionSettings(Settings, OutSettings);

	// The LAN beacon only starts for an advertised session at creation, and an update never restarts it.
	OutSettings.bIsLANMatch = true;
	OutSettings.bShouldAdvertise = true;
	OutSettings.bUseLobbiesIfAvailable = false;
}

void UMKHSessionProvider_Null::ConfigureSearch(const FMKHLobbySearchQuery& Query, FOnlineSessionSearch& OutSearch) const
{
	Super::ConfigureSearch(Query, OutSearch);
	OutSearch.bIsLanQuery = true;
}

void UMKHSessionProvider_Null::ReadSearchResult(const FOnlineSessionSearchResult& Result, FMKHLobbySearchResult& OutLobby) const
{
	Super::ReadSearchResult(Result, OutLobby);
	OutLobby.CurrentPlayers = FMath::Max(OutLobby.CurrentPlayers, 1);
}
