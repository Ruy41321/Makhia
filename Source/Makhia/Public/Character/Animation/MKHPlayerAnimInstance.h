// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Character/MovementStateMachine/MovementStateTypes.h"
#include "MKHPlayerAnimInstance.generated.h"

class ACharacterBase;

/**
 * Anim instance of the duel characters, players and the duel bot alike (parent of ABP_MKHPlayerCharacter): reads
 * speed, direction and bIsHoldingWeapon from its ACharacterBase owner and follows its movement state machine.
 */
UCLASS()
class MAKHIA_API UMKHPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
protected:
    virtual void NativeBeginPlay() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;
	
	/** Callback for when movement state changes */
	UFUNCTION()
	void OnMovementStateChanged(EMovementStateValue OldState, EMovementStateValue NewState);

protected:
	/** Character playing this animation (player or AI), cached at NativeBeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	TObjectPtr<ACharacterBase> OwnerCharacter;

	// Locomotion properties for Blend Space, indicates the current horizontal velocity
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float CurrentSpeed;
	
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.0f;
	
	// Property to indicates whenever the character is holding a weapon to switch locomotion blendspace
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsHoldingWeapon = false; 
	
	// Movement State Machine properties - now updated via observer pattern
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State Machine")
	EMovementStateValue CurrentMovementState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State Machine")
	EMovementStateValue PreviousMovementState;
	
	// Crouching properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	float CrouchingRate = 1000.0f; // Rate at which the player crouches, can be adjusted in the editor

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	float CrouchingTransitionTime = 0.0f;

	// Helper function to check if states are valid (for Blueprint debugging)
	UFUNCTION(BlueprintPure, Category = "Movement State Machine")
	bool IsMovementStateDataValid() const;

	// Get movement state as string for debugging
	UFUNCTION(BlueprintPure, Category = "Movement State Machine")
	FString GetCurrentMovementStateString() const;

	UFUNCTION(BlueprintPure, Category = "Movement State Machine")
	FString GetPreviousMovementStateString() const;

private:
	/** Flag to track if we're subscribed to state change events */
	bool bIsSubscribedToStateChanges = false;

	/** Cached crouch transition target - updated only when state changes */
	float CrouchingTransitionTarget = 0.0f;

	/** Helper function to subscribe to movement state changes */
	void SubscribeToMovementStateChanges();

	/** Helper function to unsubscribe from movement state changes */
	void UnsubscribeFromMovementStateChanges();
	
};
