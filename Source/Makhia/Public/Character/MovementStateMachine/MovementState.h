// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "Character/MovementStateMachine/MovementStateTypes.h"
#include "MovementState.generated.h"

// Forward declarations
class ACharacterBase;
class UMovementStateMachine;

/**
 * Base class for all movement states
 */
UCLASS(BlueprintType, Blueprintable, Abstract)
class MAKHIA_API UMovementState : public UObject
{
	GENERATED_BODY()

public:
	UMovementState();

	/** Initialize the state with its state machine and the character owning it */
	virtual void Initialize(UMovementStateMachine* InStateMachine, ACharacterBase* InCharacter);

	/** Called when entering this state */
	UFUNCTION(BlueprintImplementableEvent, Category = "Movement State")
	void OnEnterState(EMovementStateValue PreviousState);

	/** Called every frame while in this state */
	UFUNCTION(BlueprintImplementableEvent, Category = "Movement State")
	void OnUpdateState(float DeltaTime);

	/** Called when exiting this state */
	UFUNCTION(BlueprintImplementableEvent, Category = "Movement State")
	void OnExitState(EMovementStateValue NextState);

	/** Check if this state can transition to another state */
	UFUNCTION(BlueprintNativeEvent, Category = "Movement State")
	bool CanTransitionTo(EMovementStateValue NewState) const;
	virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const;

	/** Check if this state should transition to another state automatically */
	UFUNCTION(BlueprintNativeEvent, Category = "Movement State")
	EMovementStateValue GetDesiredTransition() const;
	virtual EMovementStateValue GetDesiredTransition_Implementation() const;

	/** Get the state type this class represents */
	UFUNCTION(BlueprintPure, Category = "Movement State")
	virtual EMovementStateValue GetStateType() const PURE_VIRTUAL(UMovementState::GetStateType, return EMovementStateValue::None;);

	/** C++ versions of the Blueprint events for derived classes */
	virtual void EnterState(EMovementStateValue PreviousState);
	virtual void UpdateState(float DeltaTime);
	virtual void ExitState(EMovementStateValue NextState);

	/** Returns the gameplay tag associated with a given movement state */
	static FGameplayTag GetTagForState(EMovementStateValue StateType);

protected:
	/** Reference to the state machine that owns this state */
	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	TObjectPtr<UMovementStateMachine> StateMachine;

	/** Character owning the state machine (player or AI). */
	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	TObjectPtr<ACharacterBase> OwnerCharacter;

	/** Returns the character owning the state machine. */
	UFUNCTION(BlueprintPure, Category = "Movement State")
	ACharacterBase* GetOwnerCharacter() const { return OwnerCharacter.Get(); }

	/** Helper function to get valid state machine */
	UFUNCTION(BlueprintPure, Category = "Movement State")
	UMovementStateMachine* GetStateMachine() const { return StateMachine.Get(); }

	/** Sets the appropriate movement speed for this state */
	virtual void SetStateSpeed();

	/** Gets the speed value for a specific state type */
	virtual float GetSpeedForState(EMovementStateValue StateType) const;
};
