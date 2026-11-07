// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHAuthTypes.generated.h"

/**
 * Backend-independent ways a player can sign in. Each auth provider maps them to its own backend
 * (see UMKHAuthProvider). Full model: see Documentation/OfflineModeArchitecture.md.
 */
UENUM(BlueprintType)
enum class EMKHAuthMethod : uint8
{
	/** No method: never signed in, or nothing saved. */
	None,
	/** The backend's own account (Steam account, Epic account...). Gives friends and presence. */
	PlatformAccount,
	/** Device login with a chosen display name: online play without an account and without friends. */
	Guest,
	/** Development credentials (EOS Dev Auth Tool). Never retried at start-up, refused in Shipping builds. */
	Developer
};

/** Sign-in state of the local player, owned by UMKHAuthSubsystem. */
UENUM(BlueprintType)
enum class EMKHAuthState : uint8
{
	/** The start-up login has not run yet. */
	Unknown,
	/** A login is in flight. */
	LoggingIn,
	/** Signed in: online play is available. */
	LoggedIn,
	/** Not signed in (never, logged out, failed or backend unavailable): offline play only. */
	Offline
};

/** Reason a login failed or was refused; UMKHAuthSubsystem::GetAuthErrorText turns it into a readable text. */
UENUM(BlueprintType)
enum class EMKHAuthError : uint8
{
	/** No error. */
	None,
	/** Online services are disabled or not configured in this build. */
	BackendUnavailable,
	/** The backend could not be reached. */
	NoNetwork,
	/** The player closed the login window. */
	Cancelled,
	/** The backend rejected the credentials, or the saved session expired. */
	InvalidCredentials,
	/** The guest display name does not pass UMKHAuthSubsystem::ValidateGuestName. */
	InvalidName,
	/** The provider does not support this method, or this build does not allow it. */
	MethodNotSupported,
	/** Another login or logout is in flight, or the player is already signed in. */
	Busy,
	/** The start-up login did not answer within the configured timeout. */
	Timeout,
	/** Any other backend failure; the raw backend message is only written to the LogMKHAuth log. */
	Unknown
};

/** Result of UMKHAuthSubsystem::ValidateGuestName. */
UENUM(BlueprintType)
enum class EMKHGuestNameError : uint8
{
	/** The name is valid. */
	None,
	/** Fewer characters than the minimum, once trimmed. */
	TooShort,
	/** More characters than the maximum, once trimmed. */
	TooLong,
	/** Contains a character other than ASCII letters, digits, space, '_' and '-'. */
	InvalidCharacter
};

/** The signed-in player as the game sees it, independent of the backend. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAuthUser
{
	GENERATED_BODY()

	/** Unique id of the player on the backend (EOS: product user id); names the lobby session the player hosts. */
	UPROPERTY(BlueprintReadOnly, Category = "Auth")
	FString UserId;

	/** Name shown to the other players. */
	UPROPERTY(BlueprintReadOnly, Category = "Auth")
	FString DisplayName;

	/** Method the player signed in with. */
	UPROPERTY(BlueprintReadOnly, Category = "Auth")
	EMKHAuthMethod Method = EMKHAuthMethod::None;

	/** True when the account has friends and presence (platform or developer account), false for a guest. */
	UPROPERTY(BlueprintReadOnly, Category = "Auth")
	bool bHasSocialAccount = false;
};

/** Method-specific input of a login request; fields that do not concern the method are ignored. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAuthParams
{
	GENERATED_BODY()

	/** Guest: display name chosen by the player, validated with UMKHAuthSubsystem::ValidateGuestName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
	FString GuestDisplayName;

	/** Developer: credential name registered in the Dev Auth Tool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
	FString DevCredentialName;
};

/** Outcome of a login handed by an auth provider to UMKHAuthSubsystem. */
struct FMKHAuthLoginResult
{
	/** True when the player is now signed in on the backend. */
	bool bSuccess = false;

	/** The signed-in player; only meaningful on success. */
	FMKHAuthUser User;

	/** Why the login failed; None on success. */
	EMKHAuthError Error = EMKHAuthError::None;

	/** Raw backend message, for the log only (never shown to the player). */
	FString RawError;
};

/** Completion callback of UMKHAuthProvider::Login. */
DECLARE_DELEGATE_OneParam(FMKHAuthLoginCompleteDelegate, const FMKHAuthLoginResult& /*Result*/);
