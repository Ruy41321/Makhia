// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "MovementStateTypes.generated.h"

/**
 * Enum that defines all possible movement states
 */
UENUM(BlueprintType)
enum class EMovementStateValue : uint8
{
	None = 0			UMETA(DisplayName = "None"),
	Idle				UMETA(DisplayName = "Idle"),
	Walking				UMETA(DisplayName = "Walking"),
	Sprinting			UMETA(DisplayName = "Sprinting"),
	CrouchingIdle		UMETA(DisplayName = "Crouching Idle"),
	CrouchingMoving		UMETA(DisplayName = "Crouching Moving"),
	Jumping				UMETA(DisplayName = "Jumping"),
	Falling				UMETA(DisplayName = "Falling"),
	LandingInPlace		UMETA(DisplayName = "Landing In Place"),
	LandingMoving		UMETA(DisplayName = "Landing Moving"),
	Dodging				UMETA(DisplayName = "Dodging"),
	Blocking			UMETA(DisplayName = "Blocking"),
	Attacking			UMETA(DisplayName = "Attacking"),
	Dead				UMETA(DisplayName = "Dead")
};

/**
 * Structure that defines a transition condition between movement states
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMovementStateTransition
{
	GENERATED_BODY()

	/** The state to transition from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	EMovementStateValue FromState;

	/** The state to transition to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	EMovementStateValue ToState;

	/** Priority of this transition (higher values have higher priority) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	int32 Priority;

	FMovementStateTransition()
		: FromState(EMovementStateValue::None)
		, ToState(EMovementStateValue::None)
		, Priority(0)
	{
	}

	FMovementStateTransition(EMovementStateValue InFromState, EMovementStateValue InToState, int32 InPriority = 0)
		: FromState(InFromState)
		, ToState(InToState)
		, Priority(InPriority)
	{
	}
};

/**
 * Delegate for state change notifications
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementStateChanged, EMovementStateValue, OldState, EMovementStateValue, NewState);

/**
 * Static class for movement state utility functions
 */
UCLASS()
class MAKHIA_API UMovementStateTypes : public UObject
{
	GENERATED_BODY()

public:
	/** Convert movement state enum to string */
	UFUNCTION(BlueprintPure, Category = "Movement State")
	static FString MovementStateToString(EMovementStateValue State);

	/** Check if a state is a grounded state */
	UFUNCTION(BlueprintPure, Category = "Movement State")
	static bool IsGroundedState(EMovementStateValue State);

	/** Check if a state is an airborne state */
	UFUNCTION(BlueprintPure, Category = "Movement State")
	static bool IsAirborneState(EMovementStateValue State);

	/** Check if a state allows movement input */
	UFUNCTION(BlueprintPure, Category = "Movement State")
	static bool CanReceiveMovementInput(EMovementStateValue State);
};
