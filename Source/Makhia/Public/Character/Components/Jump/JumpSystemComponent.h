// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JumpSystemComponent.generated.h"

// Forward declaration
class ACharacterBase;
class UInputAction;
class UEnhancedInputComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MAKHIA_API UJumpSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UJumpSystemComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Sets up input bindings for this component */
	UFUNCTION(BlueprintCallable, Category = "Jump System", meta = (ToolTip = "Sets up jump input bindings"))
	void SetupInput(UEnhancedInputComponent* EnhancedInputComponent);

	/** Handles jump input - jumps if conditions are met, uncrouches if crouched */
	UFUNCTION(BlueprintCallable, Category = "Jump System", meta = (ToolTip = "Handles jump input and conditions"))
	void JumpPressed(const FInputActionValue& Value);

	/** Handles landing event and sets landing state */
	UFUNCTION(BlueprintCallable, Category = "Jump System", meta = (ToolTip = "Called when character lands"))
	void OnLanded(const FHitResult& Hit);

	/**
	 * Interrupts an in-progress jump, used when an external system (e.g. a Stun/Stagger
	 * crowd-control effect) must cancel upward movement. Stops the sustained jump input
	 * and cancels any remaining upward velocity so the character immediately begins to fall.
	 */
	UFUNCTION(BlueprintCallable, Category = "Jump System", meta = (ToolTip = "Interrupts an in-progress jump"))
	void InterruptJump();

	/** Returns true if character is currently in landing state */
	UFUNCTION(BlueprintPure, Category = "Jump System", meta = (ToolTip = "Check if character is landing"))
	bool IsLanding() const { return bIsLanding; }

	/** Resets landing state */
	UFUNCTION(BlueprintCallable, Category = "Jump System", meta = (ToolTip = "Resets landing state"))
	void ResetLanding();

	/** Returns the character owning this component. */
	UFUNCTION(BlueprintCallable, Category = "Jump System", meta = (ToolTip = "Returns the character owning this component"))
	ACharacterBase* GetOwnerCharacter() const { return GetValidCharacter(); }

	// Setters for external control
	void SetIsLanding(bool NewIsLanding) { bIsLanding = NewIsLanding; }
	void SetLandingResetTime(float NewLandingResetTime) { LandingResetTime = NewLandingResetTime; }

protected:
	/** Time in seconds before landing state is reset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System", 
		meta = (ToolTip = "Time to wait before resetting landing state", ClampMin = "0.0", ClampMax = "1.0"))
	float LandingResetTime = 0.1f;

	/** Whether character is currently in landing state */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Jump System", 
		meta = (ToolTip = "True while character is landing"))
	bool bIsLanding = false;

private:
	/** Timer for resetting landing state */
	FTimerHandle LandingTimerHandle;

	/** Cached reference to the character owning this component. */
	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;

	/** Returns the owning character, re-casting the owner as a fallback. */
	ACharacterBase* GetValidCharacter() const;

	/**
	 * Checks whether the character is currently under a crowd-control effect
	 * (Stunned or Staggered) that must suppress movement actions such as jumping.
	 *
	 * @param Character  The owning character to query.
	 * @return True if a crowd-control tag is active on the character's ability system.
	 */
	bool IsCrowdControlled(const ACharacterBase* Character) const;
	
public:
	/** Input action for jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> JumpPressedAction;
};
