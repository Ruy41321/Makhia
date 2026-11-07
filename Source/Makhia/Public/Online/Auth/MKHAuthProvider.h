// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Auth/MKHAuthTypes.h"
#include "UObject/Object.h"
#include "MKHAuthProvider.generated.h"

/**
 * Backend behind UMKHAuthSubsystem: turns the abstract auth methods (EMKHAuthMethod) into backend calls.
 * The game never talks to a provider directly; the subsystem owns one instance of the class chosen in
 * Project Settings -> Game -> Makhia Online (UMKHOnlineBackend::GetAuthProviderClass of UMKHOnlineSettings::BackendClass).
 *
 * A provider runs one login or logout at a time (IsBusy) and reports every login through the callback
 * given to Login, synchronously or later. Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHAuthProvider : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Called once by UMKHAuthSubsystem right after creation: binds the backend notifications. */
	virtual void InitializeProvider() {}

	/** Called once by UMKHAuthSubsystem before release: unbinds everything; pending callbacks are dropped. */
	virtual void ShutdownProvider() {}

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Broadcast when the backend drops the signed-in identity on its own (token revoked, connection lost). */
	FSimpleMulticastDelegate OnIdentityLost;

	/** Broadcast when the backend is reachable again after OnIdentityLost (e.g. the connection came back). */
	FSimpleMulticastDelegate OnIdentityRestored;

	/** Returns true when the backend is enabled and reachable enough to attempt a login. */
	virtual bool IsBackendAvailable() const PURE_VIRTUAL(UMKHAuthProvider::IsBackendAvailable, return false;);

	/**
	 * Returns true when the provider can sign in with the given method.
	 *
	 * @param Method  Method to check.
	 */
	virtual bool SupportsMethod(EMKHAuthMethod Method) const PURE_VIRTUAL(UMKHAuthProvider::SupportsMethod, return false;);

	/**
	 * Returns the name of a method as the UI shows it (e.g. "Steam" for the platform account).
	 * The base version returns generic names.
	 *
	 * @param Method  Method to name.
	 */
	virtual FText GetMethodDisplayName(EMKHAuthMethod Method) const;

	/**
	 * Returns true when the platform session already exists outside the game (e.g. the Steam client): the start-up
	 * login then signs the platform account in even when nothing is saved yet. False by default.
	 */
	virtual bool SignsInByDefault() const { return false; }

	/**
	 * Returns what the player can do while IsBackendAvailable is false (e.g. "start Steam"). The base version
	 * returns the generic "online services are not available" text.
	 */
	virtual FText GetUnavailableText() const;

	/**
	 * Returns true when the backend servers can be reached now. False when the platform client runs but has no
	 * network (e.g. Steam without internet): a login would fail with NoNetwork, the UI then offers no sign-in and
	 * OnIdentityRestored reports the connection coming back. True by default.
	 */
	virtual bool IsBackendReachable() const { return true; }

	/**
	 * Returns true when signing out of the game makes sense with this backend. False for a platform account owned by
	 * a client outside the game (Steam): the UI then offers no Sign out. True by default.
	 */
	virtual bool SupportsSignOut() const { return true; }

	/** Returns true while a login or a logout is in flight. */
	virtual bool IsBusy() const PURE_VIRTUAL(UMKHAuthProvider::IsBusy, return false;);

	/**
	 * Signs the local player in. OnComplete always runs exactly once, possibly before this call returns.
	 *
	 * @param Method      Method to sign in with.
	 * @param Params      Method-specific input (guest name, developer credential).
	 * @param bSilent     True for the start-up login: never open a login window or ask the player anything.
	 * @param OnComplete  Receives the outcome.
	 */
	virtual void Login(EMKHAuthMethod Method, const FMKHAuthParams& Params, bool bSilent, const FMKHAuthLoginCompleteDelegate& OnComplete)
		PURE_VIRTUAL(UMKHAuthProvider::Login, );

	/**
	 * Signs the local player out and forgets the credentials the backend keeps for the next start-up.
	 * OnComplete always runs exactly once, possibly before this call returns.
	 *
	 * @param OnComplete  Runs when the backend is done.
	 */
	virtual void Logout(const FSimpleDelegate& OnComplete) PURE_VIRTUAL(UMKHAuthProvider::Logout, );
};
