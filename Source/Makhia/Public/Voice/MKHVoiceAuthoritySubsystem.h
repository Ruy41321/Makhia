// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "MKHVoiceAuthoritySubsystem.generated.h"

class AGameModeBase;
class APlayerController;
class UMKHVoicePlayerComponent;
struct FMKHVoicePeerView;

/**
 * Server side of the voice chat, in every networked world (lobby and arena): applies the voice permission rule
 * (FMKHVoicePermissionPolicy) to every pair of players and enforces it with the player controllers' mute lists
 * (GameplayMutePlayer / GameplayUnmutePlayer), so the server never relays voice between players who may not hear
 * each other, and the host never plays it. Hiding a player on the client would not be enough.
 *
 * Decides again when a player arrives or leaves, when a machine sends new choices, and once a second (platform
 * friend list, late player state ids). Accepts the party claim of a player joining the lobby (invite or friend's
 * "Join") only while the host lobby is not matchmaking, and keeps the party in UMKHLobbySubsystem across travels.
 * Full model: see Documentation/VoiceChat.md.
 */
UCLASS()
class MAKHIA_API UMKHVoiceAuthoritySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin USubsystem Interface
	/** Only game worlds (game and PIE). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Follows the logins of the game mode. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Stops following the logins and the periodic decision. */
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//~ Begin UWorldSubsystem Interface
	/** Starts the periodic decision on the server. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the voice authority of the world of an object.
	 *
	 * @param WorldContextObject  Any object living in a world.
	 * @return The subsystem, or null outside a game world.
	 */
	static UMKHVoiceAuthoritySubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Server: a player controller with a voice component began play; its voice stays closed until it sends choices.
	 *
	 * @param Component  Voice component of the player controller.
	 */
	void RegisterVoiceComponent(UMKHVoicePlayerComponent* Component);

	/**
	 * Server: a player controller left (logout, travel).
	 *
	 * @param Component  Voice component of the player controller.
	 */
	void UnregisterVoiceComponent(UMKHVoicePlayerComponent* Component);

	/**
	 * Server: a machine sent new voice choices.
	 *
	 * @param Component  Voice component that received them.
	 */
	void HandlePreferencesChanged(UMKHVoicePlayerComponent* Component);

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns true when this world is the server of a networked game (listen server). */
	bool IsNetworkAuthority() const;

	/** Game mode notification: a player logged in (its player state now has its unique id). */
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

	/** Decides every pair now and applies the mutes. */
	void EvaluateAllPairs();

	/** Records the party claims that the server accepts (player arrived while the host lobby was passive). */
	void AcceptPartyClaims() const;

	/** Returns true when the lobby level is loaded and the host lobby is not matchmaking (joinable only by invite). */
	bool IsPassiveLobby() const;

	/**
	 * Builds what the rule knows about a player.
	 *
	 * @param Component  Voice component of the player.
	 * @return The view, with PlayerId empty when the player has no unique id yet.
	 */
	static FMKHVoicePeerView MakePeerView(const UMKHVoicePlayerComponent& Component);

	/**
	 * Returns true when two players are in the same party or the host is a platform friend of the other.
	 *
	 * @param A  First player.
	 * @param B  Second player.
	 */
	bool IsKnownPair(const UMKHVoicePlayerComponent& A, const UMKHVoicePlayerComponent& B) const;

	/**
	 * Opens or closes the voice of one player for another on the server mute list of the listener.
	 *
	 * @param Listener   Player controller of the player who hears.
	 * @param Other      Voice component of the player heard.
	 * @param bAllowed   True to let the voice through.
	 */
	void ApplyMute(APlayerController& Listener, const UMKHVoicePlayerComponent& Other, bool bAllowed);

	/** Drops destroyed components. */
	void PruneComponents();

	// ============================================================
	// Properties
	// ============================================================

	/** Voice components of the player controllers of this world (server). */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UMKHVoicePlayerComponent>> Components;

private:
	/** Last decision per ordered pair "Listener|Other", to log only the changes. */
	TMap<FString, bool> AppliedPairs;

	/** Handle of the game mode login notification. */
	FDelegateHandle PostLoginHandle;

	/** Periodic decision. */
	FTimerHandle EvaluateTimerHandle;

	/** Seconds between two periodic decisions. */
	static constexpr float EvaluateInterval = 1.f;
};
