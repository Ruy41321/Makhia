// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Interfaces/MKHAbilitySystemInterface.h"
#include "Interfaces/MKHDuelParticipantInterface.h"
#include "Character/CharacterBase.h"
#include "MKHPlayerCharacter.generated.h"

// Forward declarations for Enhanced Input
class UInputMappingContext;
class UInputAction;
class UCameraComponent;
class USpringArmComponent;
class UMKHSkinComponent;

/**
 * Broadcast on every peer whenever the replicated username changes.
 * The username reaches remote peers asynchronously (owning client -> server RPC -> replication back),
 * so UI must react to this event rather than reading the name once at registration time.
 *
 * @param NewUsername  The username now in effect on this machine.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMKHOnUsernameChangedSignature, const FString&, NewUsername);

/** Broadcast on this machine once the player character's ability system is initialized and ready to be used. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMKHOnPlayerCharacterReadySignature);

/**
 * Character controlled by a player: camera rig, Enhanced Input bindings of the movement components inherited from
 * ACharacterBase (see Documentation/CharacterSystemArchitecture.md), ability system owned by AMKHPlayerState, skin
 * from the player state, replicated username. Takes a slot of the arena duel as a human IMKHDuelParticipant.
 */
UCLASS()
class MAKHIA_API AMKHPlayerCharacter : public ACharacterBase, public IMKHDuelParticipant
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Creates the camera rig and the skin component; the movement components come from ACharacterBase. */
	AMKHPlayerCharacter();

	/** Per-frame update (currently only forwards to the base class). */
	virtual void Tick(float DeltaTime) override;

	/** Lets every movement component bind its Enhanced Input actions. */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Registers Username for replication. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: initializes the ability actor info, binds the hit-inflicted event and seeds the host's username. */
	virtual void PossessedBy(AController* NewController) override;

	/** Client: initializes the ability actor info once the player state (owner of the ASC) has replicated. */
	virtual void OnRep_PlayerState() override;

	/**
	 * Re-initializes the ability actor info when the controller replicates after the player
	 * state. PlayerState and Controller replication order is not guaranteed; if the actor
	 * info was built with a null controller, LocalPredicted abilities stay non-local forever
	 * unless this re-runs the init once the controller arrives.
	 */
	virtual void OnRep_Controller() override;

protected:
	/** Makes the mesh always tick its pose, so animations stay smooth on clients. */
	virtual void BeginPlay() override;

	// ============================================================
	// Public Interface (Round reset)
	// ============================================================

public:
	/**
	 * Snaps the character back to its spawn transform on every machine (server, owner and simulated proxies).
	 * A server-side SetActorTransform is not enough: the movement correction sent to the owning client
	 * (ClientAdjustPosition) carries location and velocity but no rotation, so the owner would keep the
	 * orientation it had before the reset. This also restores the control rotation locally, which drives
	 * the camera boom and, through bOrientRotationToMovement, where the character turns on the next input.
	 *
	 * Depends on p.UseLastGoodRotationDuringCorrection being disabled (see DefaultEngine.ini): with the
	 * engine default the next correction would restore the client's own pre-reset rotation on top of this.
	 *
	 * @param SpawnTransform  Transform captured when the participant was registered, at its first spawn.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ResetToSpawnTransform(const FTransform& SpawnTransform);

	// ============================================================
	// Public Interface (IMKHDuelParticipant)
	// ============================================================

	/**
	 * Server: snaps the character back to its spawn (Multicast_ResetToSpawnTransform) and forces the movement state
	 * machine back to Idle on every machine, the only way out of Dead.
	 */
	virtual void ResetForNewRound(const FTransform& SpawnTransform) override;

	/** Returns the replicated username; without one (not signed in), "You" for the local player, empty otherwise. */
	virtual FText GetParticipantDisplayName() const override;

	/** A player character is always human. */
	virtual bool IsHumanParticipant() const override { return true; }

	/** Returns the career levels of the player state, INDEX_NONE without one. */
	virtual int32 GetParticipantCareerLevels() const override;

	// ============================================================
	// Public Interface (Identity)
	// ============================================================

	/** Returns the player's display username (replicated to every peer). */
	UFUNCTION(BlueprintPure, Category = "Player|Identity")
	const FString& GetUsername() const { return Username; }

	/**
	 * Sets the player's display username. Callable from the owning client: the value is routed to
	 * the server and then replicated back to every peer, so all machines observe the same username.
	 *
	 * @param NewUsername  The username to assign.
	 */
	UFUNCTION(BlueprintCallable, Category = "Player|Identity")
	void SetUsername(const FString& NewUsername);

	/**
	 * Fired on this machine whenever the username changes: on the server when it is committed, and on
	 * each client when the new value replicates in. Bind UI here instead of reading the name from the
	 * game state's player data, which is populated as soon as the character reference is registered —
	 * several frames before the username round-trip completes.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Player|Identity")
	FMKHOnUsernameChangedSignature OnUsernameChanged;

	/**
	 * Fired on this machine when the player character is fully initialized and ready to be used.
	 * Bind UI here instead of reading the name from the game state's player data, which is populated
	 * as soon as the character reference is registered — several frames before the username round-trip
	 * completes.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Player|Identity")
	FMKHOnPlayerCharacterReadySignature OnPlayerCharacterReady;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/** Binds the ASC and attribute set owned by the player state, syncs the movement state tag and, on the server, applies the class defaults. */
	virtual void InitAbilityActorInfo() override;

	/**
	 * Server: hit-inflicted event callback; shows the floating damage text over the target.
	 *
	 * @param GameplayEventData  The hit event, carrying the target, the damage and the critical flag.
	 */
	void OnHitInflicted(const FGameplayEventData* GameplayEventData);

	/**
	 * Blueprint hook that spawns the floating damage number.
	 *
	 * @param SpawnTransform  Transform of the damaged target.
	 * @param Damage          Damage dealt.
	 * @param bIsCritical     True for a critical hit.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void ShowFloatingDamageText(FTransform SpawnTransform, float Damage, bool bIsCritical);

	/** Applies the starting abilities and attributes of the character class (see ACharacterBase). */
	virtual void InitClassDefaults() override;

private:
	/**
	 * Seeds the username from the local game instance for the locally controlled pawn, once.
	 * Called from the possession hooks (PossessedBy on the host, OnRep_Controller on the owning
	 * client) where the controller — and therefore ownership required to route the server RPC — is
	 * guaranteed to be in place. Idempotent and a no-op on non-locally-controlled pawns.
	 */
	void TrySeedUsernameFromLocalGameInstance();

	/**
	 * Realigns the controller view rotation with the given spawn orientation.
	 * Only applied on the machines that actually own the view (the locally controlled client and the
	 * authority): anywhere else the control rotation is unused or overwritten by the owner's updates.
	 *
	 * @param SpawnRotation  Actor rotation the character spawned with; only its yaw is used.
	 */
	void ApplySpawnViewRotation(const FRotator& SpawnRotation);

	/** Dresses the character with the skin of its player state, once that skin is known (see AMKHPlayerState::SetSkinId). */
	void ApplySkinFromPlayerState();

	/** Server RPC that commits the username on the authority so it replicates to every peer. */
	UFUNCTION(Server, Reliable)
	void Server_SetUsername(const FString& NewUsername);

	/** Replication callback: notifies this client that the username arrived from the server. */
	UFUNCTION()
	void OnRep_Username();

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** Player camera, at the end of the boom. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Spring arm following the control rotation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Puts the player's skin on the character mesh, on every machine. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	TObjectPtr<UMKHSkinComponent> SkinComponent;

private:
	/** Player's display username, replicated to every peer. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, ReplicatedUsing = OnRep_Username, meta=(AllowPrivateAccess = "true"), Category = "MKHPlayerCharacter")
	FString Username = "Player";

	/** Guards TrySeedUsernameFromLocalGameInstance so the initial seed runs only once per machine. */
	bool bUsernameSeeded = false;
};
