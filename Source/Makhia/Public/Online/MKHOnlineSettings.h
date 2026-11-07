// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MKHOnlineSettings.generated.h"

class UInputAction;
class UMKHOnlineBackend;

/**
 * Online settings exposed in Project Settings -> Game -> Makhia Online: which online backend the game
 * uses, the automatic login at start-up and the development login buttons.
 *
 * Values are stored in Config/DefaultGame.ini under [/Script/Makhia.MKHOnlineSettings].
 * Full model: see Documentation/OnlineBackends.md and Documentation/OfflineModeArchitecture.md.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Makhia Online"))
class MAKHIA_API UMKHOnlineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Places the settings under Project Settings -> Game -> Makhia Online. */
	UMKHOnlineSettings();

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Convenience accessor for the online settings.
	 *
	 * @return The CDO of UMKHOnlineSettings.
	 */
	static const UMKHOnlineSettings* Get();

	/**
	 * Returns true when the development login buttons may be shown: the flag is set and the build is
	 * not Shipping (Shipping never shows them, whatever the flag says).
	 */
	bool ShouldShowDevLoginButtons() const;

	// ============================================================
	// Properties
	// ============================================================

	// --- Backend ---

	/**
	 * Online backend of the game (sign-in, lobbies, friends, transport). Read once, when the game instance starts;
	 * -MKHOnline=<Name> overrides it outside Shipping. A class that cannot be loaded falls back to the Null backend.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Backend")
	TSoftClassPtr<UMKHOnlineBackend> BackendClass;

	// --- Authentication ---

	/** When true, the last successful login is retried silently at start-up (a Developer login never is). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Authentication")
	bool bAutoLoginOnBoot = true;

	/** Seconds the silent start-up login may take before the game gives up and stays offline. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Authentication", meta = (ClampMin = "1.0", Units = "s"))
	float AutoLoginTimeout = 10.f;

	// --- Voice chat ---

	/**
	 * Push-to-talk action (IA_PushToTalk), bound by the voice component of every player controller and mapped in the
	 * lobby and match contexts (remappable on the Keyboard and Controller pages). See Documentation/VoiceChat.md.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Voice Chat")
	TSoftObjectPtr<UInputAction> PushToTalkAction;

	// --- Development ---

	/** Shows the development login buttons (Dev Auth Tool profiles). Ignored in Shipping builds. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Development")
	bool bShowDevLoginButtons = false;

	/** Host and port of the EOS Dev Auth Tool. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Development")
	FString DevAuthHost = TEXT("127.0.0.1:8081");

	/** Credential names registered in the Dev Auth Tool, one per development login button. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Development")
	TArray<FString> DevCredentialNames;
};
