// Copyright (c) 2025 Luigi Pennisi. All rights reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DodgeSystemComponent.generated.h"
enum class EMovementStateValue : uint8;
class ACharacterBase;
class UInputAction;
class UEnhancedInputComponent;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MAKHIA_API UDodgeSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDodgeSystemComponent();

	// ============================================================
	// Lifecycle
	// ============================================================
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================
	// Public Interface
	// ============================================================
	/**
	 * Attempts to transition the movement state machine into the Dodge state.
	 * Validates current state before committing the transition.
	 *
	 * @return True if the transition was accepted, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement|Dodge")
	bool StartDodge();

	/** 
	 * Ends the dodge state and resets directions.
	 * Attempts to uncrouch if needed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement|Dodge")
	void ResetDodge();
	
	/**
	 * Updates the current dodge direction based on player input.
	 * A dodge started on a requested direction (SetRequestedDodgeDirection) keeps that direction.
	 * @return True if the direction was updated, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement|Dodge")
	bool UpdateDodgeDirection();

	/**
	 * Requests the direction of the next dodge, for characters without player input (AI). StartDodge consumes the
	 * request and the dodge then keeps this direction for its whole duration instead of following movement input.
	 * Without a request a non-player dodge follows the character's acceleration.
	 *
	 * @param WorldDirection  World-space dodge direction; only its horizontal part is used, zero clears the request.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement|Dodge")
	void SetRequestedDodgeDirection(const FVector& WorldDirection);

	/**
	 * Checks whether the provided state allows the character to dodge.
	 * @param CurrentState The state to evaluate.
	 * @return True if dodgeable, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "Movement|Dodge")
	bool IsInDodgeableState(EMovementStateValue CurrentState) const;

	/** Returns true if the player is currently executing a dodge maneuver. */
	UFUNCTION(BlueprintPure, Category = "Movement|Dodge")
	bool IsDodging() const { return bIsDodging; }

	/** Gets dodge movement speed in cm/s. */
	UFUNCTION(BlueprintPure, Category = "Movement|Dodge")
	float GetDodgeSpeed() const { return DodgeSpeed; }

	/** Gets how much input influences the dodge direction during the maneuver. */
	UFUNCTION(BlueprintPure, Category = "Movement|Dodge")
	float GetInputInfluenceFactor() const { return InputInfluenceFactor; }

	/** Gets the current world-space dodge direction. */
	UFUNCTION(BlueprintPure, Category = "Movement|Dodge")
	const FVector& GetDodgeDirection() const { return DodgeDirection; }

	/** Gets the initial dodge direction when the dodge originally started. */
	UFUNCTION(BlueprintPure, Category = "Movement|Dodge")
	const FVector& GetInitialDodgeDirection() const { return InitialDodgeDirection; }

	/** Returns true if the player was crouching before the dodge maneuver started. */
	UFUNCTION(BlueprintPure, Category = "Movement|Dodge")
	bool WasCrouchingPreDodge() const { return bWasCrouchingPreDodge; }

	/** Returns the character owning this component. */
	UFUNCTION(BlueprintCallable, Category = "Movement|Dodge")
	ACharacterBase* GetOwnerCharacter() const { return GetValidCharacter(); }

	// Inline Setters
	void SetIsDodging(bool bNewIsDodging) { bIsDodging = bNewIsDodging; }
	void SetDodgeSpeed(float NewDodgeSpeed) { DodgeSpeed = NewDodgeSpeed; }
	void SetInputInfluenceFactor(float NewFactor) { InputInfluenceFactor = FMath::Clamp(NewFactor, 0.0f, 1.0f); }
	void SetDodgeDirection(const FVector& NewDodgeDirection) { DodgeDirection = NewDodgeDirection; }
	void SetWasCrouchingPreDodge(bool bNewWasCrouching) { bWasCrouchingPreDodge = bNewWasCrouching; }

	// ============================================================
	// Protected / Internal Logic
	// ============================================================
protected:
	/** Returns the owning character, re-casting the owner as a fallback. */
	ACharacterBase* GetValidCharacter() const;

private:
	/** RPC callback executed when the pre-dodge crouching state is updated. */
	UFUNCTION()
	void OnRep_bWasCrouchingPreDodge();

	/** Evaluates current input, checking either local Enhanced Input or the replicated/AI acceleration. */
	bool TryFetchMovementInput(bool& bOutHasInput, FVector& OutMovementInput) const;

	/**
	 * Returns true when the movement input comes from a player on this machine (Enhanced Input), false for an AI
	 * (locally controlled on the server but without input) and for remote characters.
	 */
	bool HasLocalPlayerInput() const;

	/**
	 * Applies a pending SetRequestedDodgeDirection request to the dodge being started and clears it.
	 *
	 * @return True if a requested direction was applied (the dodge direction is then locked).
	 */
	bool ApplyRequestedDodgeDirection();

	/** Calculates world-space input direction based on the character's forward/right vectors. */
	FVector CalculateLocalInputDirection(const FVector& MovementInput) const;

	/** Handles setting the dodge direction without blending, primarily for initial execution. */
	bool HandleInitialDodgeDirection(bool bHasInput, const FVector& MovementInput);

	/** Updates the current dodge direction by blending the initial direction with current input. */
	void UpdateBlendedDodgeDirection(bool bHasInput, const FVector& MovementInput);

	/**
	 * Stands the character up after a dodge that crouched it; when the headroom is blocked (e.g. by the opponent's
	 * capsule) the stand up stays pending and TickComponent retries it until it succeeds.
	 *
	 * @param Character  The dodging character.
	 */
	void StandUpAfterDodge(ACharacterBase& Character);

	/**
	 * Failsafe: ends a dodge that lasted more than MaxDodgeDuration (GA_Dodge resets it only when it ends).
	 *
	 * @param Character  The dodging character.
	 */
	void EnforceMaxDodgeDuration(const ACharacterBase& Character);

	// ============================================================
	// Properties
	// ============================================================
public:
	/** Input action used to trigger a dodge maneuver. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> DodgeAction;

protected:
	/** Controls how much player input affects dodge direction. 0 = locked direction, 1 = full current input influence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InputInfluenceFactor = 0.49f;

	/** Movement speed during a dodge in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "100.0", ClampMax = "1000.0"))
	float DodgeSpeed = 650.0f;

	/** Failsafe length of a dodge: past it the dodge is reset even if GA_Dodge did not end (logged as a warning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0.1", Units = "s"))
	float MaxDodgeDuration = 2.0f;

	/** Whether the character is currently performing a dodge maneuver. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
	bool bIsDodging = false;

	/** World-space dodge direction vector. */
	UPROPERTY(BlueprintReadOnly, Category = "State", meta=(AllowPrivateAccess="true"))
	FVector DodgeDirection = FVector::ZeroVector;

	/** World-space initial dodge direction captured at the start of the maneuver. */
	UPROPERTY(BlueprintReadOnly, Category = "State", meta=(AllowPrivateAccess="true"))
	FVector InitialDodgeDirection = FVector::ZeroVector;

	/** Tracks if the character was crouching before dodging, to restore crouch state conditionally. */
	UPROPERTY(ReplicatedUsing=OnRep_bWasCrouchingPreDodge, BlueprintReadOnly, Category = "State")
	bool bWasCrouchingPreDodge = false;

private:
	/** World-space direction requested for the next dodge (SetRequestedDodgeDirection); zero when none. */
	FVector RequestedDodgeDirection = FVector::ZeroVector;

	/** True while the current dodge follows a requested direction instead of movement input. */
	bool bDodgeDirectionLocked = false;

	/** True when StartDodge really crouched the character (the crouch is refused while attacking or blocking). */
	bool bCrouchedForDodge = false;

	/** True while the stand up after a dodge waits for headroom (retried every tick). */
	bool bStandUpPending = false;

	/** World time the current dodge started (MaxDodgeDuration failsafe). */
	float DodgeStartTime = 0.f;

	/** Cached reference to the character owning this component. */
	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;
};
