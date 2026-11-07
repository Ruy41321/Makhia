// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameMode/MKHMatchMode.h"
#include "MKHGameInstance.generated.h"

class UGenericClassReference;
class UMKHLoadingScreenSubsystem;
class UMKHLobbySubsystem;
enum class EMKHAuthState : uint8;

/** Identifies which named level to travel to, resolved to a level name via UMKHGameInstance's level name properties. */
UENUM(BlueprintType)
enum class EMKHLevelType : uint8
{
	Lobby,
	Game
};

/**
 * Start-up of the game session, followed by the boot overlay (UMKHBootOverlayWidget) that covers the lobby
 * until the start-up login is settled. Full model: see Documentation/OfflineModeArchitecture.md.
 */
UENUM(BlueprintType)
enum class EMKHBootPhase : uint8
{
	/** The first map is loaded, the start-up login has not run yet. */
	Pending,
	/** The start-up login is in flight. */
	Connecting,
	/** The start-up login succeeded and the lobby is being reopened as host. */
	Travelling,
	/** The start-up is over: the lobby is shown. */
	Done
};

DECLARE_MULTICAST_DELEGATE_OneParam(FMKHBootPhaseChangedSignature, EMKHBootPhase /*NewPhase*/);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUsernameChangedDelegate, const FString&, NewUsername);

/**
 * Custom GameInstance: session state (host, session name, username), travels between the lobby and the matches,
 * the start-up boot and the reaction to sign-in changes. The lobby room state and the ways a match starts live in
 * UMKHLobbySubsystem. Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Follows the sign-in state of UMKHAuthSubsystem to keep the session state and the lobby mode in sync. */
	virtual void Init() override;

	/** Runs the start-up login (UMKHAuthSubsystem::TryAutoLogin) once the first map is loaded. */
	virtual void OnStart() override;

	/** Leaves the current online session without waiting for the answer, then shuts the game instance down. */
	virtual void Shutdown() override;

	/**
	 * Called by the engine once a map finished loading.
	 * Clears the re-entrancy guards raised by BackToLobby and StartOfflineMatch so a later return can run, ends the
	 * boot travel, and
	 * reopens the lobby as host when a signed-in player landed in the offline one (a login that finished
	 * while the lobby was travelling).
	 *
	 * @param LoadTime  Seconds the map load took.
	 * @param MapName   Name of the map that finished loading.
	 */
	virtual void LoadComplete(const float LoadTime, const FString& MapName) override;

	/**
	 * Supplies the online session class the engine instantiates for this game instance.
	 * Returns UMKHOnlineSession so a lost connection routes through BackToLobby instead of
	 * the engine's default browse to the default map.
	 *
	 * @return The online session class to use.
	 */
	virtual TSubclassOf<UOnlineSession> GetOnlineSessionClass() override;

	// ============================================================
	// Delegates
	// ============================================================

	/** Triggered when the player's username changes. */
	UPROPERTY(BlueprintAssignable, Category = "GameInstance|Session")
	FOnUsernameChangedDelegate OnUsernameChanged;

	/** Broadcast when the start-up phase changes (boot overlay). */
	FMKHBootPhaseChangedSignature OnBootPhaseChanged;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Retrieves the current instance of UMKHGameInstance.
	 * 
	 * @param WorldContextObject Object to retrieve the world from.
	 * @return A pointer to the current game instance, or nullptr if cast fails.
	 */
	UFUNCTION(BlueprintPure, Category = "GameInstance", meta = (WorldContext = "WorldContextObject"))
	static UMKHGameInstance* GetMKHGameInstance(const UObject* WorldContextObject);

	/**
	 * Getter for the data asset containing references to blueprint class needed in c++
	 * @return UGenericClassReference
	 */
	TObjectPtr<UGenericClassReference> GetGenericClassReference() {return GenericClassReference;}
	
	/**
	 * Marks the player as currently in a session.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Session")
	void OnJoinSession();

	/**
	 * Marks the player as no longer in a session.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Session")
	void OnLeaveSession();

	/** Gets the current session name. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Session")
	FName GetCurrentSessionName() const { return CurrentSessionName; }

	/** Sets the current session name manually. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Session")
	void SetCurrentSessionName(FName NewSessionName) { CurrentSessionName = NewSessionName; }

	/** Checks if the player is currently logged in (read from UMKHAuthSubsystem). */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Session")
	bool GetIsLoggedIn() const;

	/** Returns the start-up phase of the game session. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Boot")
	EMKHBootPhase GetBootPhase() const { return BootPhase; }

	/** Returns true until the start-up login is settled (the boot overlay covers the lobby meanwhile). */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Boot")
	bool IsBootInProgress() const { return BootPhase != EMKHBootPhase::Done; }

	/** Checks if the player is in an active session. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Session")
	bool GetIsInSession() const { return bIsInSession; }

	/** Sets the in-session state manually. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Session")
	void SetIsInSession(bool bNewInSession) { bIsInSession = bNewInSession; }

	/** Checks if the player is hosting a session. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Session")
	bool GetIsHost() const { return bIsHost; }

	/** Sets the host status. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Session")
	void SetIsHost(bool bNewHost) { bIsHost = bNewHost; }

	/** Checks if the player is a client. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Session")
	bool IsClient() const;

	/** Gets the player's username. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Session")
	FString GetUsername() const { return Username; }

	/** Sets the player's username. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Session")
	void SetUsername(const FString& NewUsername);

	/** Checks if the level is open as Listen Server. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Lobby")
	bool GetIsListenServer() const { return bIsListenServer; }
	
	/** Sets if the level is open as Listen Server. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Lobby")
	void SetIsListenServer(bool bNewListenServer) { bIsListenServer = bNewListenServer; }
	
	/** Called when a lobby is successfully created: the local player hosts it, alone for now. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Lobby")
	void OnLobbyCreated();

	/**
	 * Returns to the lobby and restores the lobby state to a clean, coherent one.
	 *
	 * The online session is torn down first so the lobby is re-created from scratch on arrival
	 * (see UMKHGameInstance::TryDestroyCurrentSession); the level travel is deferred until the
	 * destroy completes, or until ReturnToLobbyTimeout elapses if the backend never answers.
	 * Repeated calls while a return is already in flight are ignored: AGameModeBase::Logout
	 * fires once per controller, so a disconnect triggers this several times in a row. Calls while a match
	 * against the bot is opening are ignored too (see bOpeningOfflineMatch).
	 *
	 * @param bAsHost If true, the player will return to the lobby with server travel as listen.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Lobby")
	void BackToLobby(bool bAsHost = false);
	
	/**
	 * Leaves the current lobby or match and opens a fresh lobby where the player is alone:
	 * as host when logged in, offline (not listening) otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Lobby")
	void LeaveToNewLobby();

	/** Returns true when the loaded map is the lobby level. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Lobby")
	bool IsInLobbyLevel() const;

	/** Executes a server travel to the level identified by the given level type, once the loading screen covers the game. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Session")
	void ServerTravel(EMKHLevelType LevelType);

	/** Opens the level identified by the given level type, once the loading screen covers the game. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Session")
	void OpenLevel(EMKHLevelType LevelType, bool bAsListen = false);

	/**
	 * Starts a match against the bot: opens the game level standalone (no listen server, no online session) with
	 * the OfflineBot match mode. A lobby session hosted alone is left first, so it stops being joinable; the login is
	 * kept, and returning to the lobby hosts it again. Refused while matchmaking or with other players in the lobby
	 * (UMKHLobbySubsystem::IsOfflineMatchRefused). From the lobby, use UMKHLobbySubsystem::RequestOfflineMatch.
	 *
	 * @param Request  Offline mode and career level to play.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Match")
	void StartOfflineMatch(const FMKHOfflineMatchRequest& Request);

	/**
	 * Tells whether the career match just played leads to another level: the loaded level plays the career and the next
	 * level is unlocked.
	 *
	 * @return True when StartNextCareerLevel can run.
	 */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Match")
	bool CanStartNextCareerLevel() const;

	/** Opens the next level of the career from the current one. Ignored when CanStartNextCareerLevel is false. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Match")
	void StartNextCareerLevel();

	/**
	 * Restarts the current match against the bot from scratch, as if the player had left it and started it again
	 * from the lobby: the game level is reopened with the same offline mode and career level (new inventory, pickups and bot).
	 * Ignored outside a match against the bot.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameInstance|Match")
	void RestartOfflineMatch();

	/** Returns true when the loaded level runs a match against the bot (EMKHMatchMode::OfflineBot). */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Match")
	bool IsInOfflineMatch() const;

	/** Returns true while a return to the lobby requested by this machine (BackToLobby) is in flight. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Lobby")
	bool IsReturningToLobby() const { return bReturningToLobby; }

	/**
	 * Closes the connections of the current level on the next tick, ahead of leaving it: the other player learns at
	 * once that this machine left, and the next travel waits for the transport to free its ports (SteamSockets
	 * closes its sockets one tick late). Safe from network failure handlers: the shutdown never runs inside the
	 * engine's net code or while a level loads.
	 */
	void ReleaseWorldNetDriver();

	/** Console command for development and tests: starts a match against the bot (see StartOfflineMatch). */
	UFUNCTION(Exec)
	void MKHPlayVsBot();

	/** Gets the name of the lobby level. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Session")
	FString GetLobbyLevelName() const { return LobbyLevelName; }

	/** Gets the name of the game level. */
	UFUNCTION(BlueprintPure, Category = "GameInstance|Session")
	FString GetGameLevelName() const { return GameLevelName; }

private:

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Resolves a level type to the corresponding level name set on this instance.
	 *
	 * @param LevelType The level type to resolve.
	 * @return The level name associated with the given type.
	 */
	FString ResolveLevelName(EMKHLevelType LevelType) const;

	/**
	 * Opens the level identified by the given level type with extra URL options, once the loading screen covers
	 * the game (see UMKHLoadingScreenSubsystem::RunWhenCovered).
	 *
	 * @param LevelType  The level to open.
	 * @param bAsListen  True to open it as a listen server.
	 * @param Options    URL options appended to the level name, e.g. "?Mode=Bot"; may be empty.
	 */
	void OpenLevelWithOptions(EMKHLevelType LevelType, bool bAsListen, const FString& Options);

	/**
	 * Shuts down the net driver released by ReleaseWorldNetDriver, if its level is still the current, running one.
	 *
	 * @param ReleasedWorld  Level whose connections were released.
	 */
	void ShutdownReleasedNetDriver(TWeakObjectPtr<UWorld> ReleasedWorld);

	/** Returns the seconds a travel must still wait for a released net driver to free its ports (0 when none). */
	float GetNetworkReleaseWait() const;

	/**
	 * Opens a level URL now, or once a released net driver had time to free its ports. A delayed travel is dropped
	 * when another travel left the level meanwhile (e.g. a network failure handled on its own path).
	 *
	 * @param Url  Level URL with its options.
	 */
	void TravelAfterNetworkRelease(const FString& Url);

	/**
	 * Asks UMKHSessionSubsystem to destroy the session named CurrentSessionName.
	 * Leaving the session registered would make the lobby re-creation fail on arrival and
	 * would strand an orphan lobby on the backend.
	 *
	 * @return True if the destroy request was accepted and HandleDestroySessionComplete will run (possibly already ran).
	 */
	bool TryDestroyCurrentSession();

	/**
	 * Completion callback for TryDestroyCurrentSession. Resumes the pending return to the lobby, unless the timeout
	 * already did.
	 *
	 * @param bWasSuccessful  True if the backend reported a clean destroy.
	 */
	void HandleDestroySessionComplete(bool bWasSuccessful);

	/** Clears the local session state and travels to the lobby using the pending return mode. */
	void FinishBackToLobby();

	/** Returns the lobby subsystem of this game instance (null only while shutting down). */
	UMKHLobbySubsystem* GetLobby() const;

	/**
	 * Shows the loading screen for a travel to a level, unless one is already up (it keeps its own reason), and
	 * returns the loading screen subsystem (null only while shutting down).
	 *
	 * @param LevelType  Level the travel goes to.
	 */
	UMKHLoadingScreenSubsystem* ShowLoadingScreenForTravel(EMKHLevelType LevelType) const;

	/**
	 * UMKHAuthSubsystem callback: applies a sign-in or a sign-out to the session state and the lobby.
	 *
	 * @param NewState  Sign-in state entered.
	 * @param OldState  Sign-in state left.
	 */
	void HandleAuthStateChanged(EMKHAuthState NewState, EMKHAuthState OldState);

	/**
	 * Takes the signed-in player's session name and username; from the offline lobby, reopens it as host.
	 * During a match nothing travels: the next return to the lobby hosts it.
	 *
	 * @return True when the lobby is being reopened as host.
	 */
	bool ApplySignedInUser();

	/**
	 * Moves the start-up phase forward after a sign-in state change during the boot.
	 *
	 * @param NewState    Sign-in state entered.
	 * @param bTravelling True when the lobby is being reopened as host.
	 */
	void AdvanceBootPhase(EMKHAuthState NewState, bool bTravelling);

	/**
	 * Changes the start-up phase and broadcasts OnBootPhaseChanged.
	 *
	 * @param NewPhase  Phase to enter.
	 */
	void SetBootPhase(EMKHBootPhase NewPhase);

	/**
	 * Handles the loss of the sign-in: leaves an online lobby for an offline one, or just forgets the
	 * session name when not in a session (an online match ends through its own return to the lobby).
	 * While in a session the backend logout is held until the session is left (see HoldAuthLogout).
	 */
	void HandleSignedOut();

	/**
	 * Holds the backend logout of UMKHAuthSubsystem until FinishBackToLobby: EIK needs the signed-in user
	 * to leave the online session, and crashes if the user is gone when the leave completes.
	 */
	void HoldAuthLogout();

	/** Releases the hold taken by HoldAuthLogout, if any. */
	void ReleaseAuthLogoutHold();

	/** Returns true when the loaded map is the lobby and it is not hosted nor joined (standalone). */
	bool IsInOfflineLobby() const;

	// ============================================================
	// Properties
	// ============================================================
	
	/** Data Asset to allow c++ code to take references of Blueprint Classes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "GameInstance|ClassDataAsset")
	TObjectPtr<UGenericClassReference> GenericClassReference;
		
	/** Name of the current active session. */
	UPROPERTY(VisibleAnywhere, Category = "GameInstance|Session", meta = (AllowPrivateAccess = "true"))
	FName CurrentSessionName = NAME_None;
	
	/** Indicates if the player has joined an active game session. */
	UPROPERTY(VisibleAnywhere, Category = "GameInstance|Session", meta = (AllowPrivateAccess = "true"))
	bool bIsInSession = false;

	/** Indicates if the player is the host of the current session. */
	UPROPERTY(VisibleAnywhere, Category = "GameInstance|Session", meta = (AllowPrivateAccess = "true"))
	bool bIsHost = false;
	
	/** The username of the player. */
	UPROPERTY(VisibleAnywhere, Category = "GameInstance|Session", meta = (AllowPrivateAccess = "true"))
	FString Username = TEXT("");

	/** Indicates if the level is open as Listen Server. */
	UPROPERTY(VisibleAnywhere, Category = "GameInstance|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bIsListenServer = false;
	
	/** Name of the lobby level to travel to. */
	UPROPERTY(EditDefaultsOnly, Category = "GameInstance|Session", meta = (AllowPrivateAccess = "true"))
	FString LobbyLevelName;

	/** Name of the game level to travel to. */
	UPROPERTY(EditDefaultsOnly, Category = "GameInstance|Session", meta = (AllowPrivateAccess = "true"))
	FString GameLevelName;

	/** Seconds to wait for the session destroy callback before travelling to the lobby anyway. */
	UPROPERTY(EditDefaultsOnly, Category = "GameInstance|Lobby", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ReturnToLobbyTimeout = 5.f;

	/** True between ReleaseWorldNetDriver and the shutdown it scheduled. */
	bool bNetDriverReleasePending = false;

	/** Platform time of the last net driver shutdown done by ShutdownReleasedNetDriver. */
	double LastNetDriverReleaseTime = 0.0;

	/** True while a return to the lobby is in flight; used to ignore duplicate requests. */
	UPROPERTY(VisibleAnywhere, Category = "GameInstance|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bReturningToLobby = false;

	/**
	 * True while the game level of a match against the bot is opening, until LoadComplete. BackToLobby is ignored
	 * meanwhile: the old level's game mode logs the local controller out on teardown (BP_MKHGameMode::OnLogout),
	 * which must not divert the travel to the lobby.
	 */
	UPROPERTY(VisibleAnywhere, Category = "GameInstance|Match", meta = (AllowPrivateAccess = "true"))
	bool bOpeningOfflineMatch = false;

	/** Return mode requested by the in-flight BackToLobby call. */
	UPROPERTY(VisibleAnywhere, Category = "GameInstance|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bPendingReturnAsHost = false;

	/** True while BackToLobby waits for the answer of TryDestroyCurrentSession. */
	bool bAwaitingSessionDestroy = false;

	/** True while this instance holds the backend logout of UMKHAuthSubsystem (see HoldAuthLogout). */
	bool bHoldingAuthLogout = false;

	/** Start-up phase of the game session (boot overlay). */
	EMKHBootPhase BootPhase = EMKHBootPhase::Pending;

	/** Timer guarding against a session destroy callback that never fires. */
	FTimerHandle ReturnToLobbyTimeoutHandle;

};
