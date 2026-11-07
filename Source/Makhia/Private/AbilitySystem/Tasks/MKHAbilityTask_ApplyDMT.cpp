// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Tasks/MKHAbilityTask_ApplyDMT.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

UMKHAbilityTask_ApplyDMT::UMKHAbilityTask_ApplyDMT(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	TimeElapsed = 0.f;
}

UMKHAbilityTask_ApplyDMT* UMKHAbilityTask_ApplyDMT::ApplyDMT(
	UGameplayAbility* OwningAbility, 
	float Radius, 
	float MaxAngle, 
	float TargetStopDistance)
{
	UMKHAbilityTask_ApplyDMT* MyObj = NewAbilityTask<UMKHAbilityTask_ApplyDMT>(OwningAbility);
	if (MyObj)
	{
		MyObj->DMTRadius = Radius;
		MyObj->DMTMaxAngle = MaxAngle;
		MyObj->DMTTargetStopDistance = TargetStopDistance;
	}
	return MyObj;
}

void UMKHAbilityTask_ApplyDMT::Activate()
{
	Super::Activate();

	AActor* BestTarget = nullptr;
	if (FindBestTarget(BestTarget) && IsValid(BestTarget))
	{
		TargetActor = BestTarget;
	}
}

void UMKHAbilityTask_ApplyDMT::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!Ability || !Ability->GetCurrentActorInfo() || !Ability->GetCurrentActorInfo()->AvatarActor.IsValid())
	{
		EndTask();
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Ability->GetCurrentActorInfo()->AvatarActor.Get());

	if (!IsValid(Character))
	{
		EndTask();
		return;
	}

	TimeElapsed += DeltaTime;
	if (TimeElapsed >= DMTDuration)
	{
		EndTask();
		return;
	}

	AActor* LockedTarget = TargetActor.Get();
	if (IsValid(LockedTarget))
	{
		UpdateRotation(Character, LockedTarget, DeltaTime);
		UpdatePosition(Character, LockedTarget, DeltaTime);
	}
	else
	{
		UpdateRotation(Character, DeltaTime);
	}
}

void UMKHAbilityTask_ApplyDMT::UpdateRotation(ACharacter* Character, AActor* Target, float DeltaTime) const
{
	const FVector CharLoc = Character->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();

	FVector DirectionToTarget = TargetLoc - CharLoc;
	DirectionToTarget.Z = 0.f; // We only care about Yaw

	if (!DirectionToTarget.IsNearlyZero())
	{
		const FRotator TargetRotation = DirectionToTarget.Rotation();
		const FRotator CurrentRotation = Character->GetActorRotation();
		const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, DMTInterpSpeedRotation);
		Character->SetActorRotation(NewRotation);
	}
}

void UMKHAbilityTask_ApplyDMT::UpdateRotation(ACharacter* Character, float DeltaTime) const
{
	FVector AimingForward;
	if (GetTargetingForwardVector(AimingForward))
	{
		AimingForward.Z = 0.f; // We only care about Yaw
		if (!AimingForward.IsNearlyZero())
		{
			const FRotator TargetRotation = AimingForward.Rotation();
			const FRotator CurrentRotation = Character->GetActorRotation();
			const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, DMTInterpSpeedRotation);
			Character->SetActorRotation(NewRotation);
		}
	}
}

void UMKHAbilityTask_ApplyDMT::UpdatePosition(ACharacter* Character, AActor* Target, float DeltaTime) const
{
	const FVector CharLoc = Character->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();

	const float CurrentDist3D = FVector::Distance(CharLoc, TargetLoc);

	if (CurrentDist3D > DMTTargetStopDistance)
	{
		FVector DirectionNormalized = (TargetLoc - CharLoc).GetSafeNormal();
		
		// Stop Location is distance offset away from target
		FVector StopLocation = TargetLoc - (DirectionNormalized * DMTTargetStopDistance);
		
		FVector NewLocation = FMath::VInterpTo(CharLoc, StopLocation, DeltaTime, DMTInterpSpeedPosition);
		Character->SetActorLocation(NewLocation, true);
	}
}

void UMKHAbilityTask_ApplyDMT::OnDestroy(bool bInOwnerFinished)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnDMTFinished.Broadcast();
	}

	Super::OnDestroy(bInOwnerFinished);
}

bool UMKHAbilityTask_ApplyDMT::FindBestTarget(AActor*& OutTarget) const
{
	FVector ForwardVector;
	if (!GetTargetingForwardVector(ForwardVector))
	{
		return false;
	}

	AActor* AvatarOwner = Ability->GetCurrentActorInfo()->AvatarActor.Get();
	const FVector TraceStart = AvatarOwner->GetActorLocation();

	TArray<FHitResult> OutHits;
	PerformTargetingTrace(TraceStart, OutHits);

	OutTarget = SelectBestTargetFromHits(OutHits, TraceStart, ForwardVector);
	return OutTarget != nullptr;
}

bool UMKHAbilityTask_ApplyDMT::GetTargetingForwardVector(FVector& OutForwardVector) const
{
	if (!Ability || !Ability->GetCurrentActorInfo())
	{
		return false;
	}
	
	APawn* PawnOwner = Cast<APawn>(Ability->GetCurrentActorInfo()->AvatarActor.Get());
	if (!IsValid(PawnOwner))
	{
		return false;
	}

	OutForwardVector = PawnOwner->GetActorForwardVector();
	const AController* Controller = PawnOwner->GetController();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			OutForwardVector = CameraManager->GetCameraRotation().Vector();
		}
		else
		{
			OutForwardVector = PC->GetControlRotation().Vector();
		}
	}
	else if (IsValid(Controller))
	{
		// AI: the body faces where it last walked, the control rotation follows the focus (the duel bot's opponent).
		OutForwardVector = Controller->GetControlRotation().Vector();
	}
	OutForwardVector.Normalize();

	return true;
}

void UMKHAbilityTask_ApplyDMT::PerformTargetingTrace(const FVector& TraceStart, TArray<FHitResult>& OutHits) const
{
	APawn* PawnOwner = Cast<APawn>(Ability->GetCurrentActorInfo()->AvatarActor.Get());
	if (!IsValid(PawnOwner)) return;

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

	// Exclude Self
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(PawnOwner);

	UKismetSystemLibrary::SphereTraceMultiForObjects(
		PawnOwner,
		TraceStart,
		TraceStart,
		DMTRadius,
		ObjectTypes,
		false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		OutHits,
		true
	);
}

AActor* UMKHAbilityTask_ApplyDMT::SelectBestTargetFromHits(const TArray<FHitResult>& Hits, const FVector& TraceStart, const FVector& ForwardVector) const
{
	AActor* BestTarget = nullptr;
	float BestScore = -1.f;
	APawn* PawnOwner = Cast<APawn>(Ability->GetCurrentActorInfo()->AvatarActor.Get());

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor) || HitActor == PawnOwner)
		{
			continue;
		}
		
		FVector DirToTarget = (HitActor->GetActorLocation() - TraceStart);
		DirToTarget.Z = 0.f; // Ignore Z axis for camera orientation
		DirToTarget.Normalize();
		
		FVector Forward2D = ForwardVector;
		Forward2D.Z = 0.f; 
		Forward2D.Normalize();
		
		// Calculate angle (Dot Product)
		float DotToTarget = FMath::Clamp(FVector::DotProduct(Forward2D, DirToTarget), -1.f, 1.f);
		float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(DotToTarget));

		if (AngleDegrees <= DMTMaxAngle)
		{
			// Alignment calculation (1.0 = perfectly aligned, 0.0 = on the edge of MaxAngle)
			float Alignment = FMath::Clamp(1.f - (AngleDegrees / DMTMaxAngle), 0.f, 1.f);
			
			// Distance calculation (1.0 = extremely close, 0.0 = on the edge of Radius)
			float Distance = FVector::Distance(TraceStart, HitActor->GetActorLocation());
			float NormalizedDistance = 1.f - FMath::Clamp(Distance / DMTRadius, 0.f, 1.f);
			
			float Score = (Alignment * DMTAlignmentWeight) + (NormalizedDistance * DMTDistanceWeight);
			
			if (Score > BestScore)
			{
				BestScore = Score;
				BestTarget = HitActor;
			}
		}
	}

	return BestTarget;
}
