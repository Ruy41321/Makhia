// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Online/Auth/MKHAuthTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHAuthSubsystem.generated.h"

class UMKHAuthProvider;
class UMKHAuthSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMKHAuthStateChangedSignature, EMKHAuthState, NewState, EMKHAuthState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMKHAuthLoginFailedSignature, EMKHAuthMethod, Method, EMKHAuthError, Error);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMKHAuthStateChangedNativeSignature, EMKHAuthState /*NewState*/, EMKHAuthState /*OldState*/);

/**
 * Single entry point of the game for signing in and out. Widgets, controllers and the game instance only
 * talk to this subsystem; the backend sits behind a UMKHAuthProvider whose class comes from the online
 * backend chosen in Project Settings -> Game -> Makhia Online (UMKHOnlineServicesSubsystem).
 *
 * At start-up TryAutoLogin silently retries the last successful method (saved in the "Auth" slot); an explicit
 * Logout disables that until the player signs in again. Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Broadcast when the sign-in state changes, after the game instance has applied it. */
	UPROPERTY(BlueprintAssignable, Category = "Auth")
	FMKHAuthStateChangedSignature OnAuthStateChanged;

	/** Broadcast when a login requested by the player fails or is refused (never for the silent start-up login). */
	UPROPERTY(BlueprintAssignable, Category = "Auth")
	FMKHAuthLoginFailedSignature OnLoginFailed;

	/** Native twin of OnAuthStateChanged, broadcast first: the game instance updates its session state from it. */
	FMKHAuthStateChangedNativeSignature OnAuthStateChangedNative;

	/**
	 * Returns the auth subsystem of the game instance that owns the given object.
	 *
	 * @param WorldContextObject  Any object living in a game world.
	 * @return The subsystem, or null outside a game instance.
	 */
	static UMKHAuthSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Start-up login, run once per game session: a -AuthToken=<name> command line signs in with that Dev Auth
	 * Tool credential (not in Shipping); otherwise the last successful method is retried silently, with
	 * UMKHOnlineSettings::AutoLoginTimeout. Nothing to retry, auto-login disabled or Play In Editor: Offline.
	 */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	void TryAutoLogin();

	/**
	 * Starts an interactive login. Failures and refusals are broadcast by OnLoginFailed.
	 *
	 * @param Method  Method to sign in with.
	 * @param Params  Method-specific input (guest name, developer credential).
	 * @return True if the login started, false if it was refused (see GetLastError).
	 */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	bool Login(EMKHAuthMethod Method, const FMKHAuthParams& Params);

	/** Starts an interactive login with the backend's own account (see Login). */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	bool LoginWithPlatformAccount();

	/**
	 * Starts a guest login (see Login).
	 *
	 * @param DisplayName  Name chosen by the player; trimmed and validated with ValidateGuestName.
	 */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	bool LoginAsGuest(const FString& DisplayName);

	/**
	 * Starts a Dev Auth Tool login with one of UMKHOnlineSettings::DevCredentialNames (see Login).
	 *
	 * @param ProfileIndex  Index in DevCredentialNames.
	 */
	UFUNCTION(BlueprintCallable, Category = "Auth|Development")
	bool LoginWithDevProfile(int32 ProfileIndex);

	/**
	 * Signs the player out and disables the start-up login until the player signs in again. The state goes
	 * Offline at once; the backend logout waits for the holds taken meanwhile (see AddLogoutHold).
	 */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	void Logout();

	/**
	 * Moves a guest to the platform account: logs the guest out, then starts the interactive platform login
	 * once the backend logout is done. Account linking is not supported: the account is a different user.
	 *
	 * @return True if the switch started (signed in as a guest and the provider supports the platform account).
	 */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	bool SwitchToPlatformAccount();

	/**
	 * Delays the backend part of a Logout: some backends (EOS) need the signed-in user to leave an online session, so the
	 * game instance holds the logout while it leaves one. Call it from an OnAuthStateChangedNative handler;
	 * every hold must be released with ReleaseLogoutHold.
	 */
	void AddLogoutHold();

	/** Releases a hold taken with AddLogoutHold; the last one lets a pending backend logout run. */
	void ReleaseLogoutHold();

	/** Returns the current sign-in state. */
	UFUNCTION(BlueprintPure, Category = "Auth")
	EMKHAuthState GetAuthState() const { return State; }

	/** Returns true while the player is signed in. */
	UFUNCTION(BlueprintPure, Category = "Auth")
	bool IsLoggedIn() const { return State == EMKHAuthState::LoggedIn; }

	/** Returns the signed-in player; empty when not signed in. */
	UFUNCTION(BlueprintPure, Category = "Auth")
	FMKHAuthUser GetUser() const { return User; }

	/**
	 * Returns true when a login can be started now: online services available, offline and nothing in flight.
	 * A start-up login that timed out does not count: a login started meanwhile waits for it (see QueueLogin).
	 */
	UFUNCTION(BlueprintPure, Category = "Auth")
	bool CanStartLogin() const;

	/** Returns true when the online services are available in this build and session. */
	UFUNCTION(BlueprintPure, Category = "Auth")
	bool IsOnlineAvailable() const;

	/** Returns what the player can do while the online services are not available (e.g. "start Steam"). */
	UFUNCTION(BlueprintPure, Category = "Auth")
	FText GetOnlineUnavailableText() const;

	/**
	 * Returns true when the online services are available and their servers can be reached now. False without
	 * network: signing in cannot work, and the game signs back in by itself when the connection returns.
	 */
	UFUNCTION(BlueprintPure, Category = "Auth")
	bool IsOnlineReachable() const;

	/** Returns true when the signed-in player can sign out of the game with this backend (not with Steam). */
	UFUNCTION(BlueprintPure, Category = "Auth")
	bool CanSignOut() const;

	/**
	 * Returns true when signing in offers a choice (guest name, development profiles) and needs the login panel;
	 * false when the platform account is the only way in and the sign-in can start at once.
	 */
	UFUNCTION(BlueprintPure, Category = "Auth")
	bool HasSignInChoice() const;

	/**
	 * Returns true when the player can sign in with the given method here (provider support and build type).
	 *
	 * @param Method  Method to check.
	 */
	UFUNCTION(BlueprintPure, Category = "Auth")
	bool SupportsMethod(EMKHAuthMethod Method) const;

	/**
	 * Returns the name of a method as the UI shows it; the provider names its own platform account.
	 *
	 * @param Method  Method to name.
	 */
	UFUNCTION(BlueprintPure, Category = "Auth")
	FText GetMethodDisplayName(EMKHAuthMethod Method) const;

	/** Returns the error of the last failed or refused login; None after a success. */
	UFUNCTION(BlueprintPure, Category = "Auth")
	EMKHAuthError GetLastError() const { return LastError; }

	/** Returns the last guest name used on this machine, to fill the name field in advance. */
	UFUNCTION(BlueprintPure, Category = "Auth|Guest")
	FString GetSavedGuestName() const;

	/**
	 * Checks a guest display name: once trimmed, 3 to 16 characters among ASCII letters, digits, space, '_' and '-'.
	 *
	 * @param Name            Name typed by the player.
	 * @param OutTrimmedName  The name without leading and trailing spaces, the one actually used.
	 * @return None when the name is valid, otherwise the first problem found.
	 */
	UFUNCTION(BlueprintPure, Category = "Auth|Guest")
	static EMKHGuestNameError ValidateGuestName(const FString& Name, FString& OutTrimmedName);

	/** Returns the maximum length of a guest name, for the character counter of the name field. */
	UFUNCTION(BlueprintPure, Category = "Auth|Guest")
	static int32 GetGuestNameMaxLength();

	/**
	 * Returns the message that explains a guest name problem to the player.
	 *
	 * @param Error  Result of ValidateGuestName.
	 */
	UFUNCTION(BlueprintPure, Category = "Auth|Guest")
	static FText GetGuestNameErrorText(EMKHGuestNameError Error);

	/**
	 * Returns the message that explains a login error to the player (never the raw backend message).
	 *
	 * @param Error  Error to explain.
	 */
	UFUNCTION(BlueprintPure, Category = "Auth")
	static FText GetAuthErrorText(EMKHAuthError Error);

	/** Returns true when the development login buttons must be shown (project flag, never in Shipping). */
	UFUNCTION(BlueprintPure, Category = "Auth|Development")
	static bool ShouldShowDevLoginButtons();

	/** Returns the Dev Auth Tool credential names of the development login buttons. */
	UFUNCTION(BlueprintPure, Category = "Auth|Development")
	static TArray<FString> GetDevCredentialNames();

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Creates the auth provider of the online backend (Null when the backend has none). */
	void CreateProvider();

	/** Shuts the provider down and releases it. */
	void DestroyProvider();

	/** Loads the saved login preferences, or creates empty ones. */
	void LoadSaveData();

	/** Writes the login preferences to the "Auth" save slot. */
	void WriteSaveData() const;

	/**
	 * Picks what the start-up login should do from the command line, the settings and the saved preferences.
	 *
	 * @param OutParams  Input of the login to start.
	 * @return The method to sign in with, or None to stay offline.
	 */
	EMKHAuthMethod ResolveAutoLoginMethod(FMKHAuthParams& OutParams) const;

	/**
	 * Returns the method saved for the start-up login when the provider still supports it.
	 *
	 * @param OutExplicitLogout  True when the player signed out on purpose (the start-up login stays off).
	 * @return The saved method, or None.
	 */
	EMKHAuthMethod GetSavedAutoLoginMethod(bool& OutExplicitLogout) const;

	/**
	 * Reads a -AuthToken=<name> development credential from the command line (never in Shipping).
	 *
	 * @param OutCredentialName  Credential name found.
	 * @return True when the command line carries one.
	 */
	static bool TryGetCommandLineDevCredential(FString& OutCredentialName);

	/** Returns true when this game instance runs in Play In Editor, where platform logins do not work. */
	bool IsPlayInEditor() const;

	/**
	 * Checks whether a login may start and normalizes its input.
	 *
	 * @param Method     Method to sign in with.
	 * @param Params     Input received.
	 * @param OutParams  Input to hand to the provider (trimmed guest name).
	 * @return None when the login may start, otherwise the reason of the refusal.
	 */
	EMKHAuthError CheckLoginAllowed(EMKHAuthMethod Method, const FMKHAuthParams& Params, FMKHAuthParams& OutParams) const;

	/**
	 * Checks a login, then hands it to the provider, or queues it behind a timed-out start-up login.
	 *
	 * @param Method   Method to sign in with.
	 * @param Params   Method-specific input.
	 * @param bSilent  True for the start-up login: no login window, timeout, no OnLoginFailed.
	 * @return True if the login started.
	 */
	bool StartLogin(EMKHAuthMethod Method, const FMKHAuthParams& Params, bool bSilent);

	/**
	 * Hands an accepted login to the provider under a new request id and goes LoggingIn.
	 *
	 * @param Method   Method to sign in with.
	 * @param Params   Normalized method-specific input.
	 * @param bSilent  True for the start-up login (see StartLogin).
	 */
	void BeginProviderLogin(EMKHAuthMethod Method, const FMKHAuthParams& Params, bool bSilent);

	/** Returns true while the provider still runs a start-up login that outlived its timeout. */
	bool IsWaitingForTimedOutLogin() const;

	/**
	 * Keeps a player login until the timed-out start-up login answers (the provider runs one login at a time),
	 * and goes LoggingIn meanwhile. RunQueuedLogin then adopts that answer or starts the queued login.
	 *
	 * @param Method  Method to sign in with.
	 * @param Params  Normalized method-specific input.
	 */
	void QueueLogin(EMKHAuthMethod Method, const FMKHAuthParams& Params);

	/**
	 * Resolves the queued login once the timed-out start-up login has answered: its success is adopted when it
	 * signed in the identity asked for; another identity is signed out first; after a failure the queued login
	 * runs on its own.
	 *
	 * @param TimedOutResult  Answer of the timed-out start-up login.
	 */
	void RunQueuedLogin(const FMKHAuthLoginResult& TimedOutResult);

	/**
	 * Returns true when two logins sign in the same identity (same method, same guest name or dev credential).
	 *
	 * @param MethodA  Method of the first login.
	 * @param ParamsA  Normalized input of the first login.
	 * @param MethodB  Method of the second login.
	 * @param ParamsB  Normalized input of the second login.
	 */
	static bool IsSameLogin(EMKHAuthMethod MethodA, const FMKHAuthParams& ParamsA, EMKHAuthMethod MethodB, const FMKHAuthParams& ParamsB);

	/**
	 * Provider completion of the login identified by RequestId.
	 *
	 * @param Result     Outcome reported by the provider.
	 * @param RequestId  Id given to the login by BeginProviderLogin.
	 */
	void HandleProviderLoginComplete(const FMKHAuthLoginResult& Result, uint32 RequestId);

	/**
	 * Applies a successful login: stores the user, saves the method for the next start-up and goes LoggedIn.
	 *
	 * @param SignedInUser  User reported by the provider.
	 */
	void CompleteLoginSuccess(const FMKHAuthUser& SignedInUser);

	/**
	 * Records a failed or refused login and returns to Offline when it was pending.
	 *
	 * @param Method   Method of the login.
	 * @param Error    Reason of the failure.
	 * @param bNotify  True to broadcast OnLoginFailed.
	 */
	void FailLogin(EMKHAuthMethod Method, EMKHAuthError Error, bool bNotify);

	/**
	 * Core ticker callback, every frame while the silent start-up login runs: past AutoLoginDeadline (wall
	 * clock) the game goes on offline.
	 *
	 * @param DeltaTime  Unused: engine delta times skip the start-up and loading stalls.
	 * @return True to keep waiting, false once the timeout is handled.
	 */
	bool HandleAutoLoginTimeout(float DeltaTime);

	/** Provider notification: the backend dropped the signed-in identity. */
	void HandleProviderIdentityLost();

	/**
	 * Provider notification: the backend is reachable again after a loss. Signs the lost identity back in silently,
	 * unless the player signed out or signed in again meanwhile.
	 */
	void HandleProviderIdentityRestored();

	/** Runs the backend logout of a pending Logout once no hold is left. */
	void TryFinishBackendLogout();

	/** Provider completion of Logout. */
	void HandleProviderLogoutComplete();

	/**
	 * Changes the state and broadcasts OnAuthStateChangedNative, then OnAuthStateChanged.
	 *
	 * @param NewState  State to enter.
	 */
	void SetState(EMKHAuthState NewState);

	/** Stops the start-up login timeout, if it is running. */
	void ClearAutoLoginTimer();

	// ============================================================
	// Properties
	// ============================================================

	/** Backend used to sign in, created from the auth provider class of the online backend. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHAuthProvider> Provider;

	/** Login preferences saved on this machine. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHAuthSaveGame> SaveData;

private:
	/** Current sign-in state. */
	EMKHAuthState State = EMKHAuthState::Unknown;

	/** Signed-in player; empty when not signed in. */
	FMKHAuthUser User;

	/** Error of the last failed or refused login. */
	EMKHAuthError LastError = EMKHAuthError::None;

	/** Id of the latest login handed to the provider; results of older logins are ignored. */
	uint32 LoginRequestId = 0;

	/** Method of the latest login handed to the provider. */
	EMKHAuthMethod PendingMethod = EMKHAuthMethod::None;

	/** Normalized input of the latest login handed to the provider. */
	FMKHAuthParams PendingParams;

	/** True when the latest login is the silent start-up one. */
	bool bPendingSilent = false;

	/** True when the latest login outlived the start-up timeout: a late success is still adopted. */
	bool bPendingTimedOut = false;

	/** True when a player login waits for the timed-out start-up login (see QueueLogin). */
	bool bHasQueuedLogin = false;

	/** Method of the queued login. */
	EMKHAuthMethod QueuedMethod = EMKHAuthMethod::None;

	/** Normalized input of the queued login. */
	FMKHAuthParams QueuedParams;

	/** True once TryAutoLogin ran in this game session. */
	bool bAutoLoginAttempted = false;

	/** True between a Logout and its backend logout (held by AddLogoutHold). */
	bool bBackendLogoutPending = false;

	/** True when the pending logout comes from SwitchToPlatformAccount: the platform login follows it. */
	bool bPlatformLoginAfterLogout = false;

	/** Method of the identity the backend dropped; None when nothing waits to be restored. */
	EMKHAuthMethod LostMethod = EMKHAuthMethod::None;

	/** Normalized input of the identity the backend dropped. */
	FMKHAuthParams LostParams;

	/** Number of holds delaying the backend logout. */
	int32 LogoutHoldCount = 0;

	/** Core ticker running the start-up login timeout. */
	FTSTicker::FDelegateHandle AutoLoginTimeoutHandle;

	/** Wall-clock time (FPlatformTime::Seconds) past which the start-up login times out. */
	double AutoLoginDeadline = 0.0;
};
