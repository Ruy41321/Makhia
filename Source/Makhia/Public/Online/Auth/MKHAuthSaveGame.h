// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Online/Auth/MKHAuthTypes.h"
#include "MKHAuthSaveGame.generated.h"

/**
 * Login preferences kept on this machine by UMKHAuthSubsystem (save slot "Auth"): which method to retry
 * silently at the next start-up and the last guest name. Credentials themselves stay with the backend.
 */
UCLASS()
class MAKHIA_API UMKHAuthSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Method of the last successful login. */
	UPROPERTY()
	EMKHAuthMethod LastMethod = EMKHAuthMethod::None;

	/** Last guest name used, retried by the guest auto-login and proposed again by the login panel. */
	UPROPERTY()
	FString GuestDisplayName;

	/** False after an explicit logout: the next start-up stays offline until the player signs in again. */
	UPROPERTY()
	bool bAutoLogin = false;
};
