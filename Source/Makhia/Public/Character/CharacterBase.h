// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Character/MovementStateMachine/MovementStateTypes.h"
#include "CharacterBase.generated.h"

class UMKHAbilitySystemComponent;
class UMKHAttributeSet;
class UDodgeSystemComponent;
class UCrouchSystemComponent;
class UBlockingSystemComponent;
class UBasicMovementComponent;
class UJumpSystemComponent;
class USprintSystemComponent;
class UMovementStateMachine;
class UMKHFootprintComponent;
class UMKHSoundEventComponent;
class UAnimMontage;
class UGameplayEffect;

/**
 * Broadcast when the Health attribute changes.
 *
 * @param OldHealth      Health before the change.
 * @param CurrentHealth  Health after the change.
 * @param MaxHealth      Current MaxHealth.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChangedSignature, float, OldHealth, float, CurrentHealth, float, MaxHealth);

/**
 * Broadcast when the Stamina attribute changes.
 *
 * @param OldStamina      Stamina before the change.
 * @param CurrentStamina  Stamina after the change.
 * @param MaxStamina      Current MaxStamina.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStaminaChangedSignature, float, OldStamina, float, CurrentStamina, float, MaxStamina);

/**
 * Base of every character of the duel, players (AMKHPlayerCharacter) and AI (AEnemyBase, AMKHDuelBot): the GAS
 * integration point (the subclasses decide who owns the ability system component), the stamina and crowd-control
 * rules, and the component-based movement framework (Dodge, Crouch, Blocking, BasicMovement, Jump, Sprint and the
 * movement state machine), so the bot uses the same moves as the player.
 *
 * Full model: see Documentation/CharacterSystemArchitecture.md.
 */
UCLASS()
class MAKHIA_API ACharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/**
	 * Creates the movement system components and the movement state machine (same subobject names on every subclass)
	 * and swaps the engine character movement for UMKHCharacterMovementComponent.
	 *
	 * @param ObjectInitializer  Initializer of the actor (defaulted so subclasses keep their default constructors).
	 */
	ACharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Registers bIsHoldingWeapon for replication. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Makes the capsule and mesh transparent to camera booms once the Blueprint component settings are applied. */
	virtual void PostInitializeComponents() override;

protected:
	/** Hands the landing to the jump system. */
	virtual void Landed(const FHitResult& Hit) override;

	// ============================================================
	// Public Interface (Ability System)
	// ============================================================

public:
	/** Returns the character's ability system component (owned by the player state for players, by the actor for AI). */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ============================================================
	// Public Interface (Movement)
	// ============================================================

	/**
	 * Sets the character's maximum walk speed (standing and crouched).
	 *
	 * @param NewSpeed  The new maximum speed, in cm/s.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetMaxWalkSpeed(float NewSpeed);

	/**
	 * Sets the capsule and mesh to ignore the camera channels (Camera and the boom's CameraObstacle probe), so no camera boom (own or the opponent's) collides with
	 * this character. Call again after any SetCollisionProfileName on them (revive, ragdoll), which resets the response.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void IgnoreCameraCollision() const;

	/** Returns the movement state machine's current state, or None without a state machine. */
	UFUNCTION(BlueprintPure, Category = "Movement State Machine")
	EMovementStateValue GetCurrentMovementState() const;

	/** Returns the movement state machine's previous state, or None without a state machine. */
	UFUNCTION(BlueprintPure, Category = "Movement State Machine")
	EMovementStateValue GetPreviousMovementState() const;

	/**
	 * Asks the movement state machine to enter a new state on this machine.
	 *
	 * @param NewState          The state to enter.
	 * @param bForceTransition  True to skip the current state's CanTransitionTo check (e.g. to leave Dead).
	 * @return True if the transition happened.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement State Machine")
	bool TransitionToMovementState(EMovementStateValue NewState, bool bForceTransition = false);

	/**
	 * Runs TransitionToMovementState on every machine (server, owner and simulated proxies).
	 *
	 * @param NewState          The state to enter.
	 * @param bForceTransition  True to skip the current state's CanTransitionTo check.
	 */
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "Movement State Machine")
	void Multicast_TransitionToMovementState(EMovementStateValue NewState, bool bForceTransition = false);

	/** Returns the current movement state as a readable string (debug UI). */
	UFUNCTION(BlueprintPure, Category = "Movement State Machine")
	FString GetCurrentMovementStateAsString() const;

	/** Returns the movement state machine component. */
	UFUNCTION(BlueprintPure, Category = "Movement State Machine")
	UMovementStateMachine* GetMovementStateMachine() const { return MovementStateMachine.Get(); }

	/**
	 * Removes every binding the given object has on the movement state machine's OnStateChanged.
	 *
	 * @param Subscriber  The object to unsubscribe.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement State Machine")
	void UnsubscribeFromMovementStateChanges(UObject* Subscriber);

	// ============================================================
	// Public Interface (Animation)
	// ============================================================

	/**
	 * Stops the given montage on every machine (server, owner, and simulated proxies).
	 * Needed because GAS does not reliably replicate a montage stop to simulated proxies
	 * when the owning ability is cancelled externally (e.g. a stun interrupted by an attack).
	 *
	 * @param MontageToStop    The specific montage to stop; other montages are left untouched.
	 * @param InBlendOutTime   Blend-out time used when stopping the montage.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopMontage(UAnimMontage* MontageToStop, const float InBlendOutTime);

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/** Binds the ability system component and attribute set to this character; implemented by the subclasses. */
	virtual void InitAbilityActorInfo();

	/** Binds the health, stamina, crowd-control and sound event callbacks on the ability system component. Rebind-safe. */
	virtual void BindCallbacksToDependencies();

	/** Grants the starting abilities and passives and applies the default attributes of CharacterTag (DA_CharacterClassInfo). */
	virtual void InitClassDefaults();

	/**
	 * Called before broadcasting OnStaminaChanged: adds the OutOfStamina tag when stamina runs out and removes it once
	 * stamina is full again, and drives the stamina regeneration (UpdateStaminaRegen), for players and bot alike.
	 *
	 * @param OldStamina      Stamina before the change.
	 * @param CurrentStamina  Stamina after the change.
	 * @param MaxStamina      Current MaxStamina.
	 */
	virtual void HandleStaminaChanged(float OldStamina, float CurrentStamina, float MaxStamina);

	/** Broadcasts the current health and stamina so freshly bound UI shows them. */
	UFUNCTION(BlueprintCallable)
	virtual void BroadcastInitialValues();

	/**
	 * Reacts to a crowd-control gameplay tag (Stunned / Staggered) being added or removed.
	 * When the tag becomes active it cancels ongoing movement actions by forcing an uncrouch
	 * and interrupting any in-progress jump, so crowd control fully suppresses movement.
	 *
	 * @param CallbackTag  The crowd-control tag whose count changed.
	 * @param NewCount     The new stack count for the tag (greater than zero means active).
	 */
	void HandleCrowdControlTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

private:
	/** Binds the Stamina attribute change (OutOfStamina tag + OnStaminaChanged). Rebind-safe. */
	void BindStaminaCallbacks();

	/** Binds the Stunned / Staggered tag events to HandleCrowdControlTagChanged. Rebind-safe. */
	void BindCrowdControlCallbacks();

	/**
	 * Server only: spending stamina stops the regeneration and restarts the StaminaRegenDelay timer; full stamina
	 * stops it. See Documentation/CharacterSystemArchitecture.md (Stamina).
	 *
	 * @param OldStamina      Stamina before the change.
	 * @param CurrentStamina  Stamina after the change.
	 * @param MaxStamina      Current MaxStamina.
	 */
	void UpdateStaminaRegen(float OldStamina, float CurrentStamina, float MaxStamina);

	/** Timer callback: applies StaminaRegenEffectClass unless the character blocks (the block resumes it on release). */
	void ApplyStaminaRegen();

	/** Removes the running stamina regeneration (effects granting State.General.Stamina.Recovery). */
	void RemoveStaminaRegen();

	// ============================================================
	// Properties
	// ============================================================

public:
	/** Fired when the Health attribute changes. */
	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnHealthChangedSignature OnHealthChanged;

	/** Fired when the Stamina attribute changes. */
	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnStaminaChangedSignature OnStaminaChanged;

	/** Delay, in seconds, before stamina starts regenerating after being spent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina")
	float StaminaRegenDelay = 1.0f;

	/** Effect applied StaminaRegenDelay seconds after the last stamina spend (GE_Status_StaminaRegen). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina")
	TSubclassOf<UGameplayEffect> StaminaRegenEffectClass;

	/** Dodge mechanic. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Dodge System")
	TObjectPtr<UDodgeSystemComponent> DodgeSystem;

	/** Crouch mechanic (also checks the headroom to stand up). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Crouch System")
	TObjectPtr<UCrouchSystemComponent> CrouchSystem;

	/** Block mechanic. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Blocking System")
	TObjectPtr<UBlockingSystemComponent> BlockingSystem;

	/** Walking and, for players, looking around. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Basic Movement")
	TObjectPtr<UBasicMovementComponent> BasicMovementSystem;

	/** Jump mechanic. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Jump System")
	TObjectPtr<UJumpSystemComponent> JumpSystem;

	/** Sprint mechanic. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Sprint System")
	TObjectPtr<USprintSystemComponent> SprintSystem;

	/** State machine arbitrating the movement mechanics (Idle, Dodging, Dead, ...). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Movement State Machine")
	TObjectPtr<UMovementStateMachine> MovementStateMachine;

	/** Cosmetic footprints left on sand (decals at each foot plant, local only). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Footprints")
	TObjectPtr<UMKHFootprintComponent> FootprintComponent;

	/** Plays on every machine the sound effects that server gameplay events trigger (DT_SoundEffects TriggerEvent). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<UMKHSoundEventComponent> SoundEventComponent;

	/** Whether the character is holding a weapon; switches the locomotion blendspace. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Animation")
	bool bIsHoldingWeapon = false;

protected:
	/** Gameplay tag of the character class, key of its entry in DA_CharacterClassInfo. */
	UPROPERTY(EditAnywhere, Category = "Custom Values | Character Info")
	FGameplayTag CharacterTag;

	/** Ability system component: the player state's for players, the actor's own for AI. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UMKHAbilitySystemComponent> MKHAbilitySystemComponent;

	/** Attribute set living next to MKHAbilitySystemComponent. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UMKHAttributeSet> MKHAttributeSet;

private:
	/** Delay between the last stamina spend and the regeneration (UpdateStaminaRegen). */
	FTimerHandle StaminaRegenTimer;
};
