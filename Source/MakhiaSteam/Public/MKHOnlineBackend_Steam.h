// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Backend/MKHOnlineBackend.h"
#include "Online/Backend/MKHSessionProvider_OSS.h"
#include "MKHOnlineBackend_Steam.generated.h"

/**
 * Steamworks backend through the engine's Steam online subsystem ("STEAM"): the Steam account of the running client,
 * Steam lobbies, P2P game connections over SteamSockets (direct through ICE when possible, Valve relay otherwise).
 * Needs the Steam client running when the game starts; in editor processes Steam is off (Play In Editor stays
 * offline). Full model: see Documentation/OnlineBackends.md.
 */
UCLASS()
class MAKHIASTEAM_API UMKHOnlineBackend_Steam : public UMKHOnlineBackend
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Returns "Steam". */
	virtual FName GetBackendName() const override;

	/**
	 * Steam overlay, avatars and recent players; no region selector (Steam filters lobbies by distance), no in-game
	 * friend requests (Steam has no API for them: they go through the overlay), no NAT type; voice chat on the
	 * game connection (native engine VoIP over SteamSockets), voice output device, no microphone choice.
	 */
	virtual FMKHOnlineCapabilities GetCapabilities() const override;

	/** Returns "Steam" (a brand name, the same in every language). */
	virtual FText GetPlatformDisplayName() const override;

	/**
	 * Returns true when the Steam client's SteamDeck=1 environment variable is set, else
	 * ISteamUtils::IsSteamRunningOnSteamDeck after starting the Steam subsystem (false without Steam). Works under Proton.
	 */
	virtual bool IsRunningOnHandheld() const override;

	/** Returns UMKHAuthProvider_Steam. */
	virtual TSubclassOf<UMKHAuthProvider> GetAuthProviderClass() const override;

	/** Returns UMKHSessionProvider_Steam. */
	virtual TSubclassOf<UMKHSessionProvider> GetSessionProviderClass() const override;

	/** Returns UMKHSocialProvider_Steam. */
	virtual TSubclassOf<UMKHSocialProvider> GetSocialProviderClass() const override;

	/** Returns UMKHNetTransportProvider_Steam. */
	virtual TSubclassOf<UMKHNetTransportProvider> GetTransportProviderClass() const override;

	/** Returns UMKHVoiceProvider_OSS: FVoiceEngineSteam derives from the engine voice, nothing Steam-specific is needed. */
	virtual TSubclassOf<UMKHVoiceProvider> GetVoiceProviderClass() const override;
};

/**
 * Session provider on the Steam online subsystem: Steam lobbies (friends-only while passive, public while matchmaking).
 * The development AppID (480) is shared by every Steamworks developer, so each lobby carries the game name and the
 * network version and searches only return lobbies with the same values. The connect string of a joined lobby is
 * the SteamSockets P2P address of its owner ("steam.<SteamID64>:<Port>").
 */
UCLASS()
class MAKHIASTEAM_API UMKHSessionProvider_Steam : public UMKHSessionProvider_OSS
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns "STEAM". */
	virtual FName GetOnlineSubsystemName() const override;

	/** Adds the game name and the network version, friends-only passive lobbies, no lobby voice chat. */
	virtual void ConfigureSessionSettings(const FMKHLobbySessionSettings& Settings, FOnlineSessionSettings& OutSettings) const override;

	/** Asks Steam for the lobbies with the same game name and network version only. */
	virtual void ConfigureSearch(const FMKHLobbySearchQuery& Query, FOnlineSessionSearch& OutSearch) const override;

private:
	/** Returns the network version advertised by the lobbies (FNetworkVersion::GetLocalNetworkVersion). */
	static FString GetNetVersionValue();
};
