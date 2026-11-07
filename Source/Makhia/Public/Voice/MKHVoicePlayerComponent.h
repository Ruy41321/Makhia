// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Voice/MKHVoiceTypes.h"
#include "MKHVoicePlayerComponent.generated.h"

class APlayerController;
class UEnhancedInputComponent;
struct FInputActionValue;
class UMKHVoicePlayerComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FMKHVoicePeerStatusesChangedSignature, UMKHVoicePlayerComponent* /*Component*/);

/**
 * Voice link between a player's machine and the server, on every player controller of the lobby and of the arena
 * (AMKHLobbyPlayerController, AMKHPlayerController).
 *
 * The owning machine sends its voice choices (FMKHVoicePreferences) with a validated Server RPC; the server keeps
 * them here, never replicated to other players, and UMKHVoiceAuthoritySubsystem writes back the decision for every
 * other player (PeerStatuses), which only the owner receives (a player controller replicates to its owner alone).
 * Full model: see Documentation/VoiceChat.md.
 */
UCLASS(ClassGroup = (Voice), meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHVoicePlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Replicated by default. */
	UMKHVoicePlayerComponent();

	//~ Begin UActorComponent Interface
	/** Registers with the server rule (authority) and with the local voice state (owning machine). */
	virtual void BeginPlay() override;

	/** Unregisters from both. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Registers PeerStatuses. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/** Broadcast on the owning machine (and on the server) when PeerStatuses changed. */
	FMKHVoicePeerStatusesChangedSignature OnPeerStatusesChanged;

	/**
	 * Binds the push-to-talk action (UMKHOnlineSettings::PushToTalkAction) on the owning local player controller.
	 *
	 * @param EnhancedInput  Input component of the player controller.
	 */
	void SetupInput(UEnhancedInputComponent* EnhancedInput);

	/** Returns the player controller that owns the component (null when attached elsewhere). */
	APlayerController* GetPlayerController() const;

	/** Returns the NetId of the player, empty while the player state or its unique id is missing. */
	FString GetPlayerId() const;

	/**
	 * Owning machine: sends the choices to the server when they changed since the last send (or were never sent and
	 * the connection is ready). Called every tick of the voice state, so a send skipped early is retried.
	 *
	 * @param Preferences  Current choices of the local player.
	 */
	void SendPreferences(const FMKHVoicePreferences& Preferences);

	/** Returns the server decision for every other player (owning machine and server). */
	const TArray<FMKHVoicePeerStatus>& GetPeerStatuses() const { return PeerStatuses; }

	/**
	 * Returns the server decision for one player.
	 *
	 * @param PlayerId  NetId of the other player.
	 * @return The reason, Unavailable while the server did not decide yet.
	 */
	EMKHVoiceBlockReason GetPeerReason(const FString& PlayerId) const;

	/** Returns true when the server opened the voice with at least one other player. */
	bool HasOpenPeer() const;

	/** Server: returns the choices sent by the owning machine. */
	const FMKHVoicePreferences& GetServerPreferences() const { return ServerPreferences; }

	/** Server: returns true once the owning machine sent its choices. */
	bool HasServerPreferences() const { return bHasServerPreferences; }

	/**
	 * Server: records whether the player arrived while the host lobby was passive (invite or friend join only), the
	 * condition for accepting their party claim.
	 *
	 * @param bPassive  True when the lobby level was loaded and the host lobby was not matchmaking.
	 */
	void SetArrivedInPassiveLobby(bool bPassive) { bArrivedInPassiveLobby = bPassive; }

	/** Server: returns true when the player arrived while the host lobby was passive. */
	bool HasArrivedInPassiveLobby() const { return bArrivedInPassiveLobby; }

	/**
	 * Server: stores the decision for every other player (replicated to the owner).
	 *
	 * @param NewStatuses  One entry per other human player.
	 */
	void SetPeerStatuses(const TArray<FMKHVoicePeerStatus>& NewStatuses);

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Receives the voice choices of the owning machine and asks the server rule to decide again.
	 *
	 * @param NewPreferences  Choices of the player.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetVoicePreferences(const FMKHVoicePreferences& NewPreferences);

	/** Replication callback of PeerStatuses. */
	UFUNCTION()
	void OnRep_PeerStatuses();

	/** Returns true when the owning player controller is local and connected (Server RPCs can leave). */
	bool CanSendToServer() const;

	/** Logs the decision for every other player when it changed. */
	void LogPeerStatuses() const;

	/** Push-to-talk pressed. */
	void HandlePushToTalkStarted(const FInputActionValue& Value);

	/** Push-to-talk released (or cancelled by a context change). */
	void HandlePushToTalkEnded(const FInputActionValue& Value);

	// ============================================================
	// Properties
	// ============================================================

	/** Server decision for every other player, replicated to the owner only (player controller). */
	UPROPERTY(ReplicatedUsing = OnRep_PeerStatuses)
	TArray<FMKHVoicePeerStatus> PeerStatuses;

private:
	/** Server: choices of the owning machine. */
	FMKHVoicePreferences ServerPreferences;

	/** Owning machine: choices sent last. */
	FMKHVoicePreferences SentPreferences;

	/** Server: true once the owning machine sent its choices. */
	bool bHasServerPreferences = false;

	/** Owning machine: true once the choices were sent at least once. */
	bool bHasSentPreferences = false;

	/** Server: true when the player arrived while the host lobby was passive. */
	bool bArrivedInPassiveLobby = false;
};
