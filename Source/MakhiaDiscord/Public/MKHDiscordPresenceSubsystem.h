// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Online/MKHFriendTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHDiscordPresenceSubsystem.generated.h"

class UDiscordActivity;
class UDiscordClient;
class UDiscordClientResult;

/**
 * Discord rich presence of the local player: "Playing Makhìa", what they are doing (lobby, duel against the bot,
 * online duel), round and score, time spent in the activity.
 *
 * Reads the same activity the friends subsystem publishes on the platform (UMKHFriendsSubsystem::BuildLocalActivity)
 * and sends it through the Discord client of the DiscordPartnerSDK plugin (UDiscordLocalPlayerSubsystem) to the
 * Discord desktop app, without any Discord sign-in. Without Discord the updates fail quietly and are retried.
 * Full model: see Documentation/DiscordIntegration.md.
 */
UCLASS()
class MAKHIADISCORD_API UMKHDiscordPresenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin USubsystem Interface
	/** Created on game clients and listen servers when rich presence is enabled and the application id is set. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Starts the activity ticker. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Stops the ticker and clears the presence. */
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/** Returns the activity last read from the game (published or about to be). */
	UFUNCTION(BlueprintPure, Category = "Discord|Presence")
	FMKHPlayerActivity GetCurrentActivity() const { return CurrentActivity; }

	/** Returns true when Discord accepted the last presence update. */
	UFUNCTION(BlueprintPure, Category = "Discord|Presence")
	bool IsPresencePublished() const { return bPresencePublished; }

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Ticker: binds the Discord client, reads the activity and sends it when it changed. */
	bool HandleTick(float DeltaTime);

	/**
	 * Returns the Discord client of the first local player, giving it the application id the first time it is seen.
	 *
	 * @return The client, or null before the local player exists.
	 */
	UDiscordClient* ResolveClient();

	/** Reads the activity of the game; on a change resets the timer and the party when needed and marks a publish. */
	void RefreshActivity();

	/**
	 * Sends the current activity to Discord.
	 *
	 * @param Client  Bound Discord client.
	 */
	void PublishPresence(UDiscordClient& Client);

	/**
	 * Builds the Discord activity (texts, pictures, party, timer) of the current activity.
	 *
	 * @return A new initialised activity object.
	 */
	UDiscordActivity* BuildDiscordActivity();

	/** Completion of UpdateRichPresence: schedules a retry when Discord refused it. */
	void HandlePublishResult(UDiscordClientResult* Result);

	/** Returns the first presence line (what the player is doing), empty for no activity. */
	static FText GetDetailsText(const FMKHPlayerActivity& Activity);

	/** Returns the second presence line (lobby state, or round and score), empty for no activity. */
	static FText GetStateText(const FMKHPlayerActivity& Activity);

	/** Returns the small picture key of an activity from the settings (empty: none). */
	static FString GetSmallImageKey(const FMKHPlayerActivity& Activity);

	/** Returns the current time in milliseconds since the Unix epoch. */
	static int64 GetUnixTimeMs();

	// ============================================================
	// Properties
	// ============================================================

private:
	/** Discord client the application id was given to; a new client is bound again. */
	TWeakObjectPtr<UDiscordClient> BoundClient;

	/** Activity last read from the game. */
	FMKHPlayerActivity CurrentActivity;

	/** Start of the current activity (lobby or match), in milliseconds since the Unix epoch. */
	int64 ActivityStartMs = 0;

	/** Party id of the current lobby, new for every lobby visit (Discord needs one to show the player count). */
	FString PartyId;

	/** Culture the presence texts were written in; a language change publishes them again. */
	FString PublishedCulture;

	/** True when the current activity has not been accepted by Discord yet. */
	bool bPublishPending = false;

	/** True while an UpdateRichPresence call waits for its completion. */
	bool bPublishInFlight = false;

	/** True when Discord accepted the last update. */
	bool bPresencePublished = false;

	/** True once a refused update has been logged (the next ones are Verbose until one succeeds). */
	bool bFailureLogged = false;

	/** Platform time before which no update is sent again after a refusal. */
	double NextRetryTime = 0.0;

	/** Handle of the ticker, running for the whole life of the subsystem. */
	FTSTicker::FDelegateHandle TickHandle;
};
