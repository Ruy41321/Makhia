// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/Loading/MKHTravelError.h"
#include "MKHLoadingScreenSubsystem.generated.h"

class SMKHLoadingScreen;
class SWidget;
class UMaterialInterface;
class UMKHGameInstance;
class UMKHTextStyle;
class UNetDriver;

/** Why the loading screen is up; picks the status line. */
UENUM(BlueprintType)
enum class EMKHLoadingReason : uint8
{
	/** Any other travel ("Loading..."). */
	Loading,
	/** An opponent was found by the matchmaking. */
	MatchFound,
	/** A lobby match starts (everyone ready). */
	MatchStarting,
	/** A match against the bot starts. */
	OfflineMatch,
	/** Back to the lobby (end of a match, leaving, an error). */
	ReturningToLobby,
	/** The lobby is reopened (e.g. hosted after a sign-in). */
	OpeningLobby
};

/** Where the travel covered by the loading screen should end; the screen stays up until it is reached. */
UENUM(BlueprintType)
enum class EMKHLoadingDestination : uint8
{
	/** Whatever level loads next. */
	Any,
	/** The lobby level, with the local lineup built. */
	Lobby,
	/** The game level, with the local pawn possessed (a lobby on the way does not count). */
	Game
};

DECLARE_MULTICAST_DELEGATE(FMKHLoadingScreenHiddenSignature);
DECLARE_MULTICAST_DELEGATE(FMKHTravelErrorReportedSignature);

/**
 * Loading screen that covers every travel, so the player never sees a level "in passing" (the opponent's lobby,
 * the lobby before a match starts) nor the freeze of a blocking load.
 *
 * Lives as long as the game instance: the screen is a Slate widget added to the game viewport (it survives map
 * changes) and, during a blocking load, an identical widget played by the MoviePlayer on the loading thread.
 * Shown by the travel entry points (UMKHGameInstance travels, matchmaking join, match start, the lobby countdown)
 * or, as a safety net, by any map load; hidden once the destination is ready (EMKHLoadingDestination).
 * Also collects the errors that send the player back to the lobby (network, travel, matchmaking, timeouts), which
 * AMKHLobbyHUD shows once the screen is gone. Log: LogMKHLoading. Full model: see Documentation/LoadingScreen.md.
 */
UCLASS()
class MAKHIA_API UMKHLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the loading screen subsystem of the game instance of a world context object.
	 *
	 * @param WorldContextObject  Any object living in a world.
	 * @return The subsystem, or null without a game instance.
	 */
	static UMKHLoadingScreenSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Shows the loading screen (fading in) or, when it is already up, changes its reason and destination.
	 * Ignored during the start-up boot (the boot overlay covers it) and on a dedicated server.
	 *
	 * @param Reason       Why the screen is up (status line).
	 * @param Destination  Where the travel should end; the screen stays up until it is reached.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void ShowLoadingScreen(EMKHLoadingReason Reason, EMKHLoadingDestination Destination);

	/** Shows the loading screen for a lobby match about to start: "opponent found" while matchmaking, else "match starting". */
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void ShowMatchLoadingScreen();

	/** Hides the loading screen (fading out) without waiting for a destination, e.g. when a join failed. */
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void HideLoadingScreen();

	/** Returns true while the loading screen is up (fading out included). */
	UFUNCTION(BlueprintPure, Category = "Loading")
	bool IsLoadingScreenActive() const { return bActive; }

	/** Returns where the travel covered by the screen should end (meaningful only while the screen is up). */
	UFUNCTION(BlueprintPure, Category = "Loading")
	EMKHLoadingDestination GetLoadingDestination() const { return Destination; }

	/**
	 * Runs an action (a travel) once the screen fully covers the game; at once when it already does or cannot be
	 * shown. A pending action still runs if the screen is hidden first.
	 *
	 * @param Action  Action to run.
	 */
	void RunWhenCovered(TFunction<void()>&& Action);

	/**
	 * Records an error that sends the player back to the lobby, logs it and, while the screen is up, turns it into
	 * a return to the lobby. Shown by AMKHLobbyHUD (W_TravelError) once the screen is gone.
	 *
	 * @param Error  What went wrong.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loading|Error")
	void ReportTravelError(EMKHTravelError Error);

	/**
	 * ReportTravelError with an engine detail for the log.
	 *
	 * @param Error   What went wrong.
	 * @param Detail  Engine detail (failure type, map, message), logged only.
	 */
	void ReportTravelErrorWithDetail(EMKHTravelError Error, const FString& Detail);

	/** Returns true when an error waits to be shown. */
	UFUNCTION(BlueprintPure, Category = "Loading|Error")
	bool HasPendingError() const { return PendingError.IsSet(); }

	/** Returns the waiting error and forgets it. */
	FMKHTravelErrorInfo ConsumePendingError();

	/** Broadcast when the screen is gone (fade-out over). */
	FMKHLoadingScreenHiddenSignature OnLoadingScreenHidden;

	/** Broadcast when an error is recorded (shown at once when no loading screen is up). */
	FMKHTravelErrorReportedSignature OnTravelErrorReported;

protected:
	// ============================================================
	// Protected / Internal Logic - Display
	// ============================================================

	/** Returns true when a screen can be shown now (game viewport, not a server-only instance, boot over). */
	bool CanShowLoadingScreen() const;

	/** Loads the style assets of the settings and builds the background brush (once). */
	void LoadStyleAssets();

	/** Creates a new screen widget (look from the loaded assets) with the current status line. */
	TSharedRef<SMKHLoadingScreen> CreateScreenWidget() const;

	/** Adds the game viewport screen if it is not there yet. */
	void AddOverlayToViewport();

	/** Removes the game viewport screen. */
	void RemoveOverlayFromViewport();

	/** Pushes the current reason into the status line. */
	void RefreshStatusText() const;

	/**
	 * Returns the status line of a reason.
	 *
	 * @param InReason  Reason to describe.
	 */
	static FText GetReasonText(EMKHLoadingReason InReason);

	// ============================================================
	// Protected / Internal Logic - Progress
	// ============================================================

	/** Core ticker: fades, pending actions, destination check, timeouts. Always returns true. */
	bool Tick(float DeltaTime);

	/** Advances the fade and applies the opacity. */
	void UpdateFade(float DeltaTime);

	/** Runs the pending actions. */
	void FlushPendingActions();

	/** Starts the fade-out once the destination has been ready long enough. */
	void UpdateArrival();

	/** Returns true when the travel reached its destination and the level is ready to be seen. */
	bool IsDestinationReached() const;

	/** Turns a stuck travel into an error (MATCH-START-TIMEOUT, TRAVEL-TIMEOUT). */
	void CheckTimeouts();

	/**
	 * Client side of the lobby countdown (AMKHLobbyGameState): shows the screen when it runs out, hides it when the
	 * host cancelled it before travelling.
	 */
	void UpdateLobbyCountdown();

	/** Returns true when the world is loaded and playing, outside a seamless travel. */
	bool IsWorldSettled() const;

	/** Starts the fade-out. */
	void BeginFadeOut();

	/** End of the fade-out: removes the screen and broadcasts OnLoadingScreenHidden. */
	void FinishHide();

	/** Returns the game instance as UMKHGameInstance (null for another class). */
	UMKHGameInstance* GetMKHGameInstance() const;

	// ============================================================
	// Protected / Internal Logic - Engine Hooks
	// ============================================================

	/** MoviePlayer: a blocking load starts and needs a loading screen. */
	void HandlePrepareLoadingScreen();

	/** A map load starts: safety net for travels nobody announced. */
	void HandlePreLoadMap(const FString& MapName);

	/**
	 * A map (blocking or seamless) finished loading.
	 *
	 * @param LoadedWorld  World that was loaded (may be null).
	 */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** A seamless travel starts: safety net, like HandlePreLoadMap. */
	void HandleSeamlessTravelStart(UWorld* CurrentWorld, const FString& MapName);

	/**
	 * Engine network failure: recorded as a travel error (not at the end of a match). Repeats of the failure are
	 * ignored once an error is recorded and the return to the lobby is in progress.
	 */
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	/** Engine travel failure: recorded as a travel error. */
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/**
	 * Returns the error of an engine network failure.
	 *
	 * @param FailureType  Engine failure.
	 */
	static EMKHTravelError MapNetworkFailure(ENetworkFailure::Type FailureType);

	/** Returns true when this game instance's world belongs to it (PIE runs several game instances). */
	bool IsOwnWorld(const UWorld* World) const;

	/** Returns true when the current match is over (the opponent leaving then is not an error). */
	bool IsMatchOver() const;

	/**
	 * Returns true when a network failure only means that the lobby host left: in a lobby, not on the way to a
	 * match. It becomes a lobby notice instead of an error.
	 *
	 * @param FailureType  Engine failure.
	 */
	bool IsLobbyHostLeaving(ENetworkFailure::Type FailureType) const;

	/** Posts "<host> left the lobby" with the name of the lobby host (the other player with the lowest PlayerId). */
	void PostHostLeftNotice() const;

	/**
	 * Returns true when a join failure is part of the matchmaking retries (the search found a lobby that is already
	 * gone and goes on by itself), so it is not shown as an error.
	 *
	 * @param Error  Error being reported.
	 */
	bool IsMatchmakingRetry(EMKHTravelError Error) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Background material (kept loaded: the loading thread draws it). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BackgroundMaterial;

	/** Style of the game name (kept loaded for its font). */
	UPROPERTY(Transient)
	TObjectPtr<UMKHTextStyle> TitleStyle;

	/** Style of the status line (kept loaded for its font). */
	UPROPERTY(Transient)
	TObjectPtr<UMKHTextStyle> StatusStyle;

	/** Brush of the background material. */
	FSlateBrush BackgroundBrush;

	/** Screen shown in the game viewport. */
	TSharedPtr<SMKHLoadingScreen> OverlayWidget;

	/** Error waiting to be shown in the lobby. */
	FMKHTravelErrorInfo PendingError;

private:
	/** True while the screen is up. */
	bool bActive = false;

	/** True while OverlayWidget is in the game viewport. */
	bool bOverlayInViewport = false;

	/** True while the screen fades out. */
	bool bFadingOut = false;

	/** True once a map finished loading since the destination was set. */
	bool bMapLoadedSinceShow = false;

	/** Current opacity of the screen (0..1). */
	float Opacity = 0.f;

	/** Why the screen is up. */
	EMKHLoadingReason Reason = EMKHLoadingReason::Loading;

	/** Where the travel should end. */
	EMKHLoadingDestination Destination = EMKHLoadingDestination::Any;

	/** Wall-clock time the screen was shown. */
	double ShowTime = 0.0;

	/** Wall-clock time the destination became ready, negative while it is not. */
	double ReadySince = -1.0;

	/** Wall-clock time a loaded level started waiting for the match to start, negative while it does not. */
	double WaitSince = -1.0;

	/** True when this machine showed the screen for the end of the lobby countdown (client side). */
	bool bShownForCountdown = false;

	/** Actions waiting for the screen to cover the game. */
	TArray<TFunction<void()>> PendingActions;

	/** Core ticker registration. */
	FTSTicker::FDelegateHandle TickHandle;

	/** Engine delegate registrations. */
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle SeamlessTravelHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PrepareLoadingScreenHandle;
};
