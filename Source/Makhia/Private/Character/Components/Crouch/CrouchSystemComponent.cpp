// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"
#include "Character/Components/Dodge/DodgeSystemComponent.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "Utils/Utils.h"
#include "Components/CapsuleComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"

UCrouchSystemComponent::UCrouchSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
	SetIsReplicatedByDefault(true);

	// Initialize default values
	CrouchTargetHeight = 65.0f;
	StandTargetHeight = 90.0f;
	HeightAdjustmentRate = FMath::Abs(StandTargetHeight - CrouchTargetHeight) * 10.0f;

	StandMeshHeightOffset = -90.0f;
	CrouchMeshHeightOffset = -60.0f;
	MeshOffsetAdjustmentRate = FMath::Abs(StandMeshHeightOffset - CrouchMeshHeightOffset) * 10.0f;

	UncrouchSafetyMargin = 5.0f;
	CrouchSpeed = 100.0f;
}

void UCrouchSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCrouchSystemComponent, bIsCrouched);
}

void UCrouchSystemComponent::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	
	if (!OwnerCharacter)
	{
		UE_LOG(LogMKHMovement, Error, TEXT("CrouchSystemComponent: Owner is not a CharacterBase! Owner class: %s"),
			GetOwner() ? *GetOwner()->GetClass()->GetName() : TEXT("NULL"));
	}
}

void UCrouchSystemComponent::SetupInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("CrouchSystemComponent: EnhancedInputComponent is null"));
		return;
	}

	if (!CrouchPressedAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("CrouchSystemComponent: CrouchPressedAction is not set"));
		return;
	}

	// Bind crouch input
	EnhancedInputComponent->BindActionValueLambda(CrouchPressedAction, ETriggerEvent::Started, 
		[this](const FInputActionValue& Value) {
			CrouchPressed(Value);
		});


}

void UCrouchSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//ACharacterBase* Character = GetValidCharacter();
	//if (!Character)
	//	return;

	// Handle crouch height adjustment
	//if (IsCrouchingInProgress())
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("CrouchSystemComponent: Adjusting crouch height..."));
	//	float TargetCapsuleHeight = IsCrouched() ? GetCrouchTargetHeight() : GetStandTargetHeight();
	//	float TargetMeshHeight = IsCrouched() ? GetCrouchMeshHeightOffset() : GetStandMeshHeightOffset();
	//	AdjustCapsuleHeight(DeltaTime, TargetCapsuleHeight, TargetMeshHeight);
	//}
}

void UCrouchSystemComponent::CrouchPressed(const FInputActionValue& Value)
{
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
		return;

	// Crowd-control states (Stunned / Staggered) must suppress any movement action.
	if (IsCrowdControlled(Character))
		return;

	// Don't allow crouch changes during dodge (not using the state check because its called right after the dodge and the state could be still not updated)
	if (Character->DodgeSystem && Character->DodgeSystem->IsDodging())
		return;

	if (bIsCrouched)
	{
		if (!CanUncrouchSafely())
			return;
		UnCrouch();
	}
	else
		Crouch();
	
}

void UCrouchSystemComponent::Crouch()
{
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
		return;

	if (Character->GetMovementStateMachine())
	{
		EMovementStateValue CurrentState = Character->GetMovementStateMachine()->GetCurrentState();

		if (CurrentState == EMovementStateValue::Falling || CurrentState == EMovementStateValue::Jumping || CurrentState == EMovementStateValue::Blocking
			|| CurrentState == EMovementStateValue::Dead || CurrentState == EMovementStateValue::Attacking)
			return;
	}
	
	// Call server RPC to execute on server
	if (!Character->HasAuthority())
	{
		ServerCrouch();
	}
	
	bIsCrouched = true;
	Character->Crouch();
}

void UCrouchSystemComponent::ServerCrouch_Implementation()
{
	this->Crouch();
}

void UCrouchSystemComponent::UnCrouch()
{
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
		return;

	// Call server RPC to execute on server
	if (!Character->HasAuthority())
	{
		ServerUnCrouch();
	}
	
	bIsCrouched = false;
	Character->UnCrouch();
}

void UCrouchSystemComponent::ServerUnCrouch_Implementation()
{
	this->UnCrouch();
}

void UCrouchSystemComponent::ForceUncrouch()
{
	if (!bIsCrouched)
		return;

	// Respect headroom so a forced uncrouch never pushes the capsule into geometry.
	if (!CanUncrouchSafely())
		return;

	UnCrouch();
}

bool UCrouchSystemComponent::CanUncrouchSafely() const
{
	ACharacterBase* Character = GetValidCharacter();
	if (!Character)
		return false;

	UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent();
	UWorld* World = Character->GetWorld();
	
	if (!CapsuleComponent || !World || !bIsCrouched)
	{
		return false;
	}

	// If we're already at standing height or taller, no need to check
	if (StandTargetHeight <= CapsuleComponent->GetScaledCapsuleHalfHeight())
	{
		return true;
	}

	FString BlockerName;
	const bool bFree = IsStandingSpaceFree(*Character, Character->GetActorLocation(), &BlockerName);
	if (!bFree)
	{
		UE_LOG(LogMKHMovement, Verbose, TEXT("CrouchSystemComponent: no headroom for %s to stand up (blocked by %s)."),
			*GetNameSafe(Character), *BlockerName);
	}
	return bFree;
}

bool UCrouchSystemComponent::TryStandUpWithNudge()
{
	ACharacterBase* Character = GetValidCharacter();
	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return false;
	}
	if (!bIsCrouched)
	{
		return true;
	}
	if (IsCrowdControlled(Character) || (IsValid(Character->DodgeSystem) && Character->DodgeSystem->IsDodging()))
	{
		return false;
	}

	FString BlockerName;
	if (!IsStandingSpaceFree(*Character, Character->GetActorLocation(), &BlockerName) && !MoveToNearestStandingSpace(*Character))
	{
		UE_LOG(LogMKHMovement, Warning, TEXT("CrouchSystemComponent: %s cannot stand up, no free spot near %s (blocked by %s)."),
			*GetNameSafe(Character), *Character->GetActorLocation().ToCompactString(), *BlockerName);
		return false;
	}

	UE_LOG(LogMKHMovement, Log, TEXT("CrouchSystemComponent: %s stands up at %s (blocker: %s)."),
		*GetNameSafe(Character), *Character->GetActorLocation().ToCompactString(), BlockerName.IsEmpty() ? TEXT("none") : *BlockerName);
	UnCrouch();
	return true;
}

bool UCrouchSystemComponent::MoveToNearestStandingSpace(ACharacterBase& Character) const
{
	constexpr float NudgeStep = 5.0f;
	constexpr float MaxNudgeDistance = 40.0f;
	constexpr int32 NudgeDirections = 8;

	const FVector Start = Character.GetActorLocation();
	for (float Distance = NudgeStep; Distance <= MaxNudgeDistance; Distance += NudgeStep)
	{
		for (int32 Index = 0; Index < NudgeDirections; ++Index)
		{
			const FVector Direction = FVector::ForwardVector.RotateAngleAxis(360.0f * Index / NudgeDirections, FVector::UpVector);
			const FVector Candidate = Start + Direction * Distance;
			if (!IsStandingSpaceFree(Character, Candidate))
			{
				continue;
			}

			FHitResult Hit;
			if (Character.SetActorLocation(Candidate, true, &Hit) && !Hit.bBlockingHit)
			{
				return true;
			}
		}
	}
	return false;
}

bool UCrouchSystemComponent::IsStandingSpaceFree(const ACharacterBase& Character, const FVector& CrouchedLocation, FString* OutBlockerName) const
{
	const UCapsuleComponent* CapsuleComponent = Character.GetCapsuleComponent();
	const UWorld* World = Character.GetWorld();
	if (!IsValid(CapsuleComponent) || !IsValid(World))
	{
		return false;
	}

	const float CurrentCapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
	const float TargetCapsuleHalfHeight = StandTargetHeight;
	const float HeightDifference = TargetCapsuleHalfHeight - CapsuleComponent->GetScaledCapsuleHalfHeight();

	// Create a capsule shape for the standing position with safety margin
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		CurrentCapsuleRadius, 
		TargetCapsuleHalfHeight + UncrouchSafetyMargin
	);
	
	// Calculate the position where the capsule would be when standing
	const FVector TargetLocation = CrouchedLocation + FVector(0.0f, 0.0f, HeightDifference + UncrouchSafetyMargin);

	// Set up collision query parameters
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CrouchHeadroom), false, &Character);
	// Actors attached to the character (its weapons) move with it: they never take its headroom.
	TArray<AActor*> AttachedActors;
	Character.GetAttachedActors(AttachedActors, true, true);
	QueryParams.AddIgnoredActors(AttachedActors);

	// Perform overlap test at the target standing position. Other characters never take the headroom: duelists often
	// touch (pawn stacking) and the movement pushes them apart; counting them left a character crouched after a dodge
	// next to its opponent (see Documentation/CharacterSystemArchitecture.md, Dodge <-> Crouch).
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, TargetLocation, FQuat::Identity, ECollisionChannel::ECC_Pawn, CapsuleShape, QueryParams);
	const FOverlapResult* Blocker = Overlaps.FindByPredicate([](const FOverlapResult& Overlap)
	{
		return Overlap.bBlockingHit && !Cast<APawn>(Overlap.GetActor());
	});
	if (Blocker != nullptr && OutBlockerName != nullptr)
	{
		*OutBlockerName = FString::Printf(TEXT("%s.%s"), *GetNameSafe(Blocker->GetActor()), *GetNameSafe(Blocker->GetComponent()));
	}
	return Blocker == nullptr;
}

bool UCrouchSystemComponent::IsCrowdControlled(const ACharacterBase* Character) const
{
	if (!IsValid(Character))
		return false;

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!IsValid(ASC))
		return false;

	// Actions are blocked while the character is under any crowd-control effect.
	static FGameplayTagContainer CrowdControlTags;
	if (CrowdControlTags.IsEmpty())
	{
		CrowdControlTags.AddTag(MKHGameplayTags::State::Stunned);
		CrowdControlTags.AddTag(MKHGameplayTags::State::Staggered);
	}

	return ASC->HasAnyMatchingGameplayTags(CrowdControlTags);
}

ACharacterBase* UCrouchSystemComponent::GetValidCharacter() const
{
	if (LIKELY(OwnerCharacter != nullptr))
	{
		return OwnerCharacter.Get();
	}

	// Fallback: re-cache if necessary (should be very rare)
	return Cast<ACharacterBase>(GetOwner());
}

//void UCrouchSystemComponent::AdjustCapsuleHeight(float DeltaTime, float TargetCapsuleHeight, float TargetMeshHeight)
//{
//	ACharacterBase* Character = GetValidCharacter();
//	if (!Character)
//		return;
//
//	bool IsFinished = true;
//	const float Tolerance = 0.01f;
//
//	// Adjusting the capsule height
//	UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent();
//	if (CapsuleComponent)
//	{
//		float CurrentHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
//
//		if (!FUtils::HandleGenericInterpolation(CurrentHeight, TargetCapsuleHeight, HeightAdjustmentRate, DeltaTime, Tolerance))
//			IsFinished = false;
//		CapsuleComponent->SetCapsuleHalfHeight(CurrentHeight);
//	}
//
//	// Adjusting the mesh position
//	USkeletalMeshComponent* Mesh = Character->GetMesh();
//	if (Mesh)
//	{
//		FVector CurrentLocation = Mesh->GetRelativeLocation();
//		float CurrentMeshHeight = CurrentLocation.Z;
//
//		if (!FUtils::HandleGenericInterpolation(CurrentMeshHeight, TargetMeshHeight, MeshOffsetAdjustmentRate, DeltaTime, Tolerance))
//			IsFinished = false;
//		CurrentLocation.Z = CurrentMeshHeight;
//		Mesh->SetRelativeLocation(CurrentLocation);
//	}
//
//	bIsCrouchingInProgress = !IsFinished;
//}