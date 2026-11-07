// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/MovementStateMachine/MovementStateTypes.h"

FString UMovementStateTypes::MovementStateToString(EMovementStateValue State)
{
	switch (State)
	{
	case EMovementStateValue::None:
		return TEXT("None");
	case EMovementStateValue::Idle:
		return TEXT("Idle");
	case EMovementStateValue::Walking:
		return TEXT("Walking");
	case EMovementStateValue::Sprinting:
		return TEXT("Sprinting");
	case EMovementStateValue::CrouchingIdle:
		return TEXT("Crouching Idle");
	case EMovementStateValue::CrouchingMoving:
		return TEXT("Crouching Moving");
	case EMovementStateValue::Jumping:
		return TEXT("Jumping");
	case EMovementStateValue::Falling:
		return TEXT("Falling");
	case EMovementStateValue::LandingInPlace:
		return TEXT("Landing In Place");
	case EMovementStateValue::LandingMoving:
		return TEXT("Landing Moving");
	case EMovementStateValue::Dodging:
		return TEXT("Dodging");
	case EMovementStateValue::Blocking:
		return TEXT("Blocking");
	case EMovementStateValue::Attacking:
		return TEXT("Attacking");
	default:
		return TEXT("Unknown");
	}
}

bool UMovementStateTypes::IsGroundedState(EMovementStateValue State)
{
	switch (State)
	{
	case EMovementStateValue::Idle:
	case EMovementStateValue::Walking:
	case EMovementStateValue::Sprinting:
	case EMovementStateValue::CrouchingIdle:
	case EMovementStateValue::CrouchingMoving:
	case EMovementStateValue::LandingInPlace:
	case EMovementStateValue::LandingMoving:
	case EMovementStateValue::Dodging:
	case EMovementStateValue::Blocking:
	case EMovementStateValue::Attacking:
		return true;
	default:
		return false;
	}
}

bool UMovementStateTypes::IsAirborneState(EMovementStateValue State)
{
	switch (State)
	{
	case EMovementStateValue::Jumping:
	case EMovementStateValue::Falling:
		return true;
	default:
		return false;
	}
}

bool UMovementStateTypes::CanReceiveMovementInput(EMovementStateValue State)
{
	switch (State)
	{
	case EMovementStateValue::Idle:
	case EMovementStateValue::Walking:
	case EMovementStateValue::Sprinting:
	case EMovementStateValue::CrouchingIdle:
	case EMovementStateValue::CrouchingMoving:
	case EMovementStateValue::Jumping:
	case EMovementStateValue::Falling:
	case EMovementStateValue::Blocking:
	case EMovementStateValue::Attacking:
		return true;
	case EMovementStateValue::LandingInPlace:
	case EMovementStateValue::LandingMoving:
	case EMovementStateValue::Dodging:
		return false;
	default:
		return false;
	}
}
