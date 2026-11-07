// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Backend/MKHOnlineTypes.h"
#include "UObject/Object.h"
#include "MKHOnlineBackend.generated.h"

class UMKHAuthProvider;
class UMKHNetTransportProvider;
class UMKHSessionProvider;
class UMKHSocialProvider;
class UMKHVoiceProvider;

/**
 * One online backend (EOS, Steam, Null...): names the provider classes the game subsystems use and describes the
 * backend features (FMKHOnlineCapabilities). The game picks one in Project Settings -> Game -> Makhia Online
 * (UMKHOnlineSettings::BackendClass); UMKHOnlineServicesSubsystem creates it and the providers.
 *
 * Nothing outside a backend and its providers talks to an online subsystem or a platform SDK.
 * Full model: see Documentation/OnlineBackends.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHOnlineBackend : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Returns the short name of the backend, also accepted by the -MKHOnline= command line switch (e.g. "Steam"). */
	virtual FName GetBackendName() const PURE_VIRTUAL(UMKHOnlineBackend::GetBackendName, return NAME_None;);

	/** Returns the features of the backend. */
	virtual FMKHOnlineCapabilities GetCapabilities() const { return FMKHOnlineCapabilities(); }

	/** Returns the platform name shown to the player (e.g. "Steam" in "Respond on Steam"); empty without a platform. */
	virtual FText GetPlatformDisplayName() const { return FText::GetEmpty(); }

	/** Returns true when the platform reports a handheld PC (Steam Deck): the game then uses its handheld profile. */
	virtual bool IsRunningOnHandheld() const { return false; }

	/** Returns the auth provider class used by UMKHAuthSubsystem. */
	virtual TSubclassOf<UMKHAuthProvider> GetAuthProviderClass() const PURE_VIRTUAL(UMKHOnlineBackend::GetAuthProviderClass, return nullptr;);

	/** Returns the session provider class used by UMKHSessionSubsystem. */
	virtual TSubclassOf<UMKHSessionProvider> GetSessionProviderClass() const PURE_VIRTUAL(UMKHOnlineBackend::GetSessionProviderClass, return nullptr;);

	/** Returns the social provider class used by UMKHFriendsSubsystem; null for a backend without friends. */
	virtual TSubclassOf<UMKHSocialProvider> GetSocialProviderClass() const { return nullptr; }

	/** Returns the transport provider class used by UMKHNetDiagnosticsSubsystem (default: plain IP). */
	virtual TSubclassOf<UMKHNetTransportProvider> GetTransportProviderClass() const;

	/** Returns the voice provider class used by UMKHVoiceChatSubsystem (default: no voice chat). */
	virtual TSubclassOf<UMKHVoiceProvider> GetVoiceProviderClass() const;
};
