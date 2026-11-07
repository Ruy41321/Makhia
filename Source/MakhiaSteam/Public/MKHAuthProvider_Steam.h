// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Auth/MKHAuthProvider.h"
#include "OnlineSubsystemTypes.h"
#include "MKHAuthProvider_Steam.generated.h"

/**
 * Auth provider on the Steam online subsystem. The player is the account of the running Steam client: there is no
 * login window and no credential, so the only method is PlatformAccount and it is signed in silently at the first
 * start-up too (SignsInByDefault). UserId is the SteamID64, the display name the Steam persona name.
 *
 * Signing out of Steam from the game makes no sense (SupportsSignOut is false: the UI hides it); Logout only takes the
 * game offline. Without network (Steam client offline) the backend is unreachable: a login fails with NoNetwork, the
 * identity of a signed-in player is reported lost (OnIdentityLost), and OnIdentityRestored reports every return of
 * the Steam servers connection.
 * Steam must run when the game starts: the Steam subsystem and the SteamSockets transport are created at start-up
 * only. Full model: see Documentation/OnlineBackends.md.
 */
UCLASS()
class MAKHIASTEAM_API UMKHAuthProvider_Steam : public UMKHAuthProvider
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Binds the connection status notification of the Steam online subsystem. */
	virtual void InitializeProvider() override;

	/** Unbinds the connection status notification. */
	virtual void ShutdownProvider() override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Returns true when the Steam online subsystem runs (Steam client started before the game). */
	virtual bool IsBackendAvailable() const override;

	/** Supports PlatformAccount only. */
	virtual bool SupportsMethod(EMKHAuthMethod Method) const override;

	/** Names the platform account "Steam". */
	virtual FText GetMethodDisplayName(EMKHAuthMethod Method) const override;

	/** True: the Steam client is already signed in. */
	virtual bool SignsInByDefault() const override { return true; }

	/** Asks the player to start Steam and restart the game. */
	virtual FText GetUnavailableText() const override;

	/** True while the Steam client is connected to the Steam servers. */
	virtual bool IsBackendReachable() const override;

	/** False: the Steam account belongs to the Steam client. */
	virtual bool SupportsSignOut() const override { return false; }

	/** Never busy: the Steam account is read at once. */
	virtual bool IsBusy() const override { return false; }

	/** Reads the account of the Steam client; fails with NoNetwork while the client is offline. */
	virtual void Login(EMKHAuthMethod Method, const FMKHAuthParams& Params, bool bSilent, const FMKHAuthLoginCompleteDelegate& OnComplete) override;

	/** Forgets the signed-in state (Steam itself stays signed in). */
	virtual void Logout(const FSimpleDelegate& OnComplete) override;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Steam servers connection notification: reports the loss of the identity while signed in, and every return. */
	void HandleConnectionStatusChanged(const FString& ServiceName, EOnlineServerConnectionStatus::Type LastStatus,
		EOnlineServerConnectionStatus::Type NewStatus);

	// ============================================================
	// Properties
	// ============================================================

private:
	/** True between a successful login and the next logout or identity loss. */
	bool bHasSignedInUser = false;

	/** Last known state of the Steam servers connection, to report each return once. */
	bool bServersReachable = false;

	/** Connection status delegate handle, removed in ShutdownProvider. */
	FDelegateHandle ConnectionStatusHandle;
};
