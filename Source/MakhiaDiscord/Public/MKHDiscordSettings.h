// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MKHDiscordSettings.generated.h"

/**
 * Discord settings exposed in Project Settings -> Game -> Makhia Discord: rich presence switch and the art assets
 * shown on the player's Discord profile. The application id stays in the plugin settings
 * (Project Settings -> Plugins -> Discord Social SDK).
 *
 * Values are stored in Config/DefaultGame.ini under [/Script/MakhiaDiscord.MKHDiscordSettings].
 * Full model: see Documentation/DiscordIntegration.md.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Makhia Discord"))
class MAKHIADISCORD_API UMKHDiscordSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Places the settings under Project Settings -> Game -> Makhia Discord. */
	UMKHDiscordSettings();

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Convenience accessor for the Discord settings.
	 *
	 * @return The CDO of UMKHDiscordSettings.
	 */
	static const UMKHDiscordSettings* Get();

	// ============================================================
	// Properties
	// ============================================================

	// --- Rich Presence ---

	/** Publishes what the player is doing on their Discord profile (needs the Discord desktop app, no sign-in). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rich Presence")
	bool bEnableRichPresence = true;

	/** Seconds between two checks of the player's activity (only changes are sent to Discord). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rich Presence", meta = (ClampMin = "0.5", Units = "s"))
	float RefreshInterval = 2.f;

	/** Seconds before sending again an update Discord refused (Discord closed or not installed). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rich Presence", meta = (ClampMin = "1", Units = "s"))
	float RetryInterval = 15.f;

	// --- Art Assets (keys of Developer Portal -> Rich Presence -> Art Assets) ---

	/** Large picture of every activity (game logo). Empty: none. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Art Assets")
	FString LargeImageKey = TEXT("makhia_logo");

	/** Tooltip of the large picture: the game title, the same in every language. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Art Assets")
	FString LargeImageText = TEXT("Makhìa");

	/** Small picture shown in the lobby. Empty: none. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Art Assets")
	FString LobbyImageKey;

	/** Small picture shown in a match against the bot. Empty: none. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Art Assets")
	FString BotMatchImageKey;

	/** Small picture shown in a match against another player. Empty: none. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Art Assets")
	FString OnlineMatchImageKey;
};
