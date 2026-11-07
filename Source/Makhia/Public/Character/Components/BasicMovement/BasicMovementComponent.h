// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/MovementStateMachine/MovementStateTypes.h"
#include "BasicMovementComponent.generated.h"

class ACharacterBase;
class UInputAction;
class UEnhancedInputComponent;

/** Identifies one of the four directional movement input actions. */
UENUM(BlueprintType)
enum class EMovementInputDirection : uint8
{
	Forward		UMETA(DisplayName = "Forward"),
	Backward	UMETA(DisplayName = "Backward"),
	Right		UMETA(DisplayName = "Right"),
	Left		UMETA(DisplayName = "Left")
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MAKHIA_API UBasicMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	UBasicMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================
	// Public Interface - Input
	// ============================================================

	/**
	 * Sets up the Enhanced Input bindings for the four directional actions and the look action.
	 *
	 * @param EnhancedInputComponent  Input component owned by the possessing controller.
	 */
	UFUNCTION(BlueprintCallable, Category = "Basic Movement", meta = (ToolTip = "Sets up movement input bindings"))
	void SetupInput(UEnhancedInputComponent* EnhancedInputComponent);

	/**
	 * Handles the forward movement action (positive X of the control rotation).
	 *
	 * @param Value  Input action value; its magnitude scales the movement.
	 */
	UFUNCTION(BlueprintCallable, Category = "Basic Movement", meta = (ToolTip = "Processes forward movement"))
	void MoveForward(const FInputActionValue& Value);

	/**
	 * Handles the backward movement action (negative X of the control rotation).
	 *
	 * @param Value  Input action value; its magnitude scales the movement.
	 */
	UFUNCTION(BlueprintCallable, Category = "Basic Movement", meta = (ToolTip = "Processes backward movement"))
	void MoveBackward(const FInputActionValue& Value);

	/**
	 * Handles the rightward movement action (positive Y of the control rotation).
	 *
	 * @param Value  Input action value; its magnitude scales the movement.
	 */
	UFUNCTION(BlueprintCallable, Category = "Basic Movement", meta = (ToolTip = "Processes right movement"))
	void MoveRight(const FInputActionValue& Value);

	/**
	 * Handles the leftward movement action (negative Y of the control rotation).
	 *
	 * @param Value  Input action value; its magnitude scales the movement.
	 */
	UFUNCTION(BlueprintCallable, Category = "Basic Movement", meta = (ToolTip = "Processes left movement"))
	void MoveLeft(const FInputActionValue& Value);

	/**
	 * Handles camera look input.
	 *
	 * @param Value  2D look axis (X = yaw, Y = pitch).
	 */
	UFUNCTION(BlueprintCallable, Category = "Basic Movement", meta = (ToolTip = "Processes camera look input"))
	void Look(const FInputActionValue& Value);

	/**
	 * Handles right stick look input, with the controller sensitivity and Y inversion.
	 *
	 * @param Value  2D look axis (X = yaw, Y = pitch), already scaled per second by the mapping modifiers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Basic Movement", meta = (ToolTip = "Processes right stick look input"))
	void LookGamepad(const FInputActionValue& Value);

	/**
	 * Clears the cached magnitude of a single directional action when its key is released.
	 *
	 * @param Direction  Directional action that has been released.
	 */
	UFUNCTION(BlueprintCallable, Category = "Basic Movement", meta = (ToolTip = "Resets a directional input when released"))
	void OnDirectionalInputCompleted(EMovementInputDirection Direction);

	// ============================================================
	// Public Interface - Queries & Setters
	// ============================================================

	/** Updates the cached horizontal speed and clears stale input tracking. */
	void UpdateMovementVelocity();

	/** Returns true if the player is currently providing movement input. */
	UFUNCTION(BlueprintPure, Category = "Basic Movement", meta = (ToolTip = "Check if player is providing movement input"))
	bool HasMovementInput() const { return bHasMovementInput; }

	/** Returns the composite movement input vector (X = forward/backward, Y = right/left). */
	UFUNCTION(BlueprintPure, Category = "Basic Movement", meta = (ToolTip = "Returns current movement input vector"))
	FVector GetCurrentMovementInput() const { return CurrentMovementInput; }

	/** Returns the current horizontal speed in cm/s. */
	UFUNCTION(BlueprintPure, Category = "Basic Movement", meta = (ToolTip = "Returns current 2D velocity"))
	float GetMovementSpeed2D() const { return MovementSpeed2D; }

	/** Returns the configured walk speed in cm/s. */
	UFUNCTION(BlueprintPure, Category = "Basic Movement", meta = (ToolTip = "Returns walk speed in cm/s"))
	float GetWalkSpeed() const { return WalkSpeed; }

	/** Overrides the cached movement input flag. */
	void SetHasMovementInput(bool bNewHasMovementInput) { bHasMovementInput = bNewHasMovementInput; }

	/** Overrides the composite movement input vector. */
	void SetCurrentMovementInput(const FVector& NewMovementInput) { CurrentMovementInput = NewMovementInput; }

	/** Sets the walk speed in cm/s. */
	void SetWalkSpeed(float NewWalkSpeed) { WalkSpeed = NewWalkSpeed; }

	/** Returns the character owning this component. */
	UFUNCTION(BlueprintCallable, Category = "Basic Movement", meta = (ToolTip = "Returns the character owning this component"))
	ACharacterBase* GetOwnerCharacter() const { return GetValidCharacter(); }

	/** Returns the owning character, re-casting the owner as a fallback. */
	ACharacterBase* GetValidCharacter() const;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Common entry point for the four directional actions: caches the magnitude,
	 * refreshes the composite input vector and forwards the movement to the character.
	 *
	 * @param Direction  Directional action being processed.
	 * @param Value      Input action value coming from Enhanced Input.
	 */
	void HandleDirectionalInput(EMovementInputDirection Direction, const FInputActionValue& Value);

	/**
	 * Turns a look axis into controller yaw and pitch, shared by the mouse and the right stick.
	 *
	 * @param LookAxis     2D look axis (X = yaw, Y = pitch).
	 * @param Sensitivity  Multiplier applied to both axes.
	 * @param bInvertY     True to flip the pitch.
	 */
	void ApplyLookInput(const FVector2D& LookAxis, float Sensitivity, bool bInvertY) const;

	/**
	 * Stores the magnitude of a single directional action and recomputes the composite vector.
	 *
	 * @param Direction  Directional action to update.
	 * @param Magnitude  Strength of the action, clamped to [0, 1].
	 */
	void SetDirectionalInputMagnitude(EMovementInputDirection Direction, float Magnitude);

	/** Rebuilds CurrentMovementInput and bHasMovementInput from the four cached magnitudes. */
	void RefreshCompositeMovementInput();

	/**
	 * Returns the world-space axis matching a directional action, based on the control rotation.
	 *
	 * @param Direction  Directional action to resolve.
	 * @return Normalized world direction, or zero vector if no controller is available.
	 */
	FVector GetWorldDirection(EMovementInputDirection Direction) const;

	/** Returns true if the movement state machine currently allows regular movement input. */
	bool CanApplyMovementInput() const;

	/**
	 * Binds the Triggered and Completed events of one directional action.
	 *
	 * @param EnhancedInputComponent  Input component to bind on.
	 * @param Action                  Input action to bind; a null action is skipped with a warning.
	 * @param Direction               Directional action the binding maps to.
	 */
	void BindDirectionalAction(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* Action, EMovementInputDirection Direction);

public:
	// ============================================================
	// Properties - Input Actions
	// ============================================================

	/** Input action mapped to forward movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> MoveForwardAction;

	/** Input action mapped to backward movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> MoveBackwardAction;

	/** Input action mapped to rightward movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> MoveRightAction;

	/** Input action mapped to leftward movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> MoveLeftAction;

	/** Input action mapped to camera look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	/** Input action mapped to camera look on the right stick (own sensitivity and inversion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LookGamepadAction;

	// ============================================================
	// Properties - Tuning
	// ============================================================

	/** Mouse sensitivity multiplier applied to the look input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Movement",
		meta = (ToolTip = "Mouse sensitivity multiplier", ClampMin = "0.1", ClampMax = "5.0"))
	float MouseSensitivity = 1.0f;

	/** Inverts the Y axis of the camera look input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Movement",
		meta = (ToolTip = "Invert Y axis for camera look"))
	bool bInvertYAxis = false;

	/** Smoothing factor for movement transitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Movement",
		meta = (ToolTip = "Smoothing factor for movement transitions", ClampMin = "0.0", ClampMax = "1.0"))
	float MovementSmoothing = 0.1f;

protected:
	// ============================================================
	// Properties - Runtime State
	// ============================================================

	/** Walk speed in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Movement",
		meta = (ToolTip = "Walk speed in cm/s", ClampMin = "50.0", ClampMax = "1000.0"))
	float WalkSpeed = 300.0f;

	/** True while at least one directional action is held. */
	UPROPERTY(BlueprintReadOnly, Category = "Basic Movement",
		meta = (ToolTip = "True if player is providing movement input"))
	bool bHasMovementInput = false;

	/** Composite movement input vector (X = forward - backward, Y = right - left). */
	UPROPERTY(BlueprintReadOnly, Category = "Basic Movement",
		meta = (ToolTip = "Current movement input vector"))
	FVector CurrentMovementInput = FVector::ZeroVector;

	/** Current horizontal speed in cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Basic Movement",
		meta = (ToolTip = "Current 2D movement speed"))
	float MovementSpeed2D = 0.0f;

private:
	// ============================================================
	// Properties - Cached Input Magnitudes
	// ============================================================

	/** Cached magnitude of the forward action, in [0, 1]. */
	float ForwardInputMagnitude = 0.0f;

	/** Cached magnitude of the backward action, in [0, 1]. */
	float BackwardInputMagnitude = 0.0f;

	/** Cached magnitude of the right action, in [0, 1]. */
	float RightInputMagnitude = 0.0f;

	/** Cached magnitude of the left action, in [0, 1]. */
	float LeftInputMagnitude = 0.0f;

	/** Cached reference to the character owning this component. */
	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;
};
