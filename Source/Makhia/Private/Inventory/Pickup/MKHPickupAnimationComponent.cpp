// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/Pickup/MKHPickupAnimationComponent.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "MKHLogChannels.h"

namespace
{
	/** Capsule half height assumed for a collector that is not available on this machine (default character). */
	constexpr float FallbackPawnHalfHeight = 88.f;

	/** Fast start, soft end. */
	float EaseOutCubic(float Alpha)
	{
		return 1.f - FMath::Pow(1.f - Alpha, 3.f);
	}

	/** Soft start, fast end. */
	float EaseInQuad(float Alpha)
	{
		return Alpha * Alpha;
	}

	/** Progress of a phase lasting Duration at Time, clamped to [0, 1]; a zero-length phase is already complete. */
	float PhaseAlpha(float Time, float Duration)
	{
		return Duration > 0.f ? FMath::Clamp(Time / Duration, 0.f, 1.f) : 1.f;
	}

	/** Height of a parabolic hop peaking at Height halfway through. */
	float HopHeight(float Alpha, float Height)
	{
		return 4.f * Height * Alpha * (1.f - Alpha);
	}
}

UMKHPickupAnimationComponent::UMKHPickupAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// After the pawns have moved, so a collection follows the collector without lagging a frame behind.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UMKHPickupAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsPlaying() || !IsValid(AnimatedComponent) || !IsValid(PivotComponent))
	{
		ResetPlayback();
		return;
	}

	Elapsed += DeltaTime;
	if (Elapsed >= GetAnimationDuration(ActiveType))
	{
		FinishAnimation();
		return;
	}

	if (ActiveType == EMKHPickupAnimationType::Collect)
	{
		UpdateCollectTargets();
	}
	ApplyPose(ComputePose(Elapsed));
}

void UMKHPickupAnimationComponent::Play(EMKHPickupAnimationType Type, USceneComponent* InAnimatedComponent,
	const USceneComponent* InPivotComponent, const APawn* Pawn, const FVector& FallbackOrigin)
{
	if (Type == EMKHPickupAnimationType::None || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!IsValid(InAnimatedComponent) || !IsValid(InPivotComponent) || InPivotComponent->GetAttachParent() != InAnimatedComponent)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: pickup animation needs a pivot attached directly to the animated component."), *GetNameSafe(GetOwner()));
		return;
	}

	if (GetAnimationDuration(Type) <= 0.f)
	{
		SkipToEnd(Type, InAnimatedComponent);
		return;
	}

	AnimatedComponent = InAnimatedComponent;
	PivotComponent = InPivotComponent;
	ActiveType = Type;
	Elapsed = 0.f;

	if (Type == EMKHPickupAnimationType::Drop)
	{
		StartLocation = IsValid(Pawn) ? ComputeDropOrigin(*Pawn) : FallbackOrigin;
	}
	else if (Type == EMKHPickupAnimationType::Collect)
	{
		// Starts from where the item is seen now, even if another animation was still moving it.
		StartLocation = PivotComponent->GetComponentLocation();
		TargetPawn = Pawn;
		SetCollectTargets(FallbackOrigin, FallbackPawnHalfHeight);
		UpdateCollectTargets();
	}

	// First frame applied right away, so the item is never seen at its resting place before it moves.
	ApplyPose(ComputePose(0.f));
	SetComponentTickEnabled(true);
}

void UMKHPickupAnimationComponent::SkipToEnd(EMKHPickupAnimationType Type, USceneComponent* InAnimatedComponent)
{
	ResetPlayback();

	if (!IsValid(InAnimatedComponent))
	{
		return;
	}

	InAnimatedComponent->SetRelativeTransform(FTransform::Identity);
	if (Type == EMKHPickupAnimationType::Collect)
	{
		InAnimatedComponent->SetVisibility(false, true);
	}
}

float UMKHPickupAnimationComponent::GetAnimationDuration(EMKHPickupAnimationType Type) const
{
	switch (Type)
	{
	case EMKHPickupAnimationType::Drop:
		return DropFlightDuration + (DropBounceCount > 0 ? DropBounceDuration : 0.f);
	case EMKHPickupAnimationType::RiseFromGround:
		return RiseDuration;
	case EMKHPickupAnimationType::Collect:
		return CollectRiseDuration + CollectHoldDuration + CollectDescendDuration;
	default:
		return 0.f;
	}
}

float UMKHPickupAnimationComponent::GetCollectDestroyDelay() const
{
	return GetAnimationDuration(EMKHPickupAnimationType::Collect) + CollectDestroyMargin;
}

FVector UMKHPickupAnimationComponent::ComputeDropOrigin(const APawn& Pawn) const
{
	const FVector Forward = FRotator(0.f, Pawn.GetActorRotation().Yaw, 0.f).Vector();
	return Pawn.GetActorLocation() + Forward * DropOriginForwardOffset + FVector::UpVector * DropOriginHeightOffset;
}

UMKHPickupAnimationComponent::FPose UMKHPickupAnimationComponent::ComputePose(float Time) const
{
	switch (ActiveType)
	{
	case EMKHPickupAnimationType::Drop:
		return ComputeDropPose(Time);
	case EMKHPickupAnimationType::RiseFromGround:
		return ComputeRisePose(Time);
	case EMKHPickupAnimationType::Collect:
		return ComputeCollectPose(Time);
	default:
		return FPose{ GetRestPivotLocation() };
	}
}

UMKHPickupAnimationComponent::FPose UMKHPickupAnimationComponent::ComputeDropPose(float Time) const
{
	if (Time >= DropFlightDuration && DropBounceCount > 0)
	{
		return ComputeDropBouncePose(Time - DropFlightDuration);
	}

	// Constant horizontal speed plus a parabola: reads as a throw.
	const float Alpha = PhaseAlpha(Time, DropFlightDuration);
	const float Eased = EaseOutCubic(Alpha);

	FPose Pose;
	Pose.Location = FMath::Lerp(StartLocation, GetRestPivotLocation(), Alpha) + FVector::UpVector * HopHeight(Alpha, DropArcHeight);
	Pose.Scale = FVector(FMath::Lerp(DropStartScale, 1.f, Eased));
	Pose.SpinDegrees = -DropSpinDegrees * (1.f - Eased);
	return Pose;
}

UMKHPickupAnimationComponent::FPose UMKHPickupAnimationComponent::ComputeDropBouncePose(float BounceTime) const
{
	FPose Pose{ GetRestPivotLocation() };

	// A real bounce lasts in proportion to the square root of its height.
	float WeightSum = 0.f;
	for (int32 Index = 0; Index < DropBounceCount; ++Index)
	{
		WeightSum += FMath::Sqrt(FMath::Pow(DropBounceDamping, static_cast<float>(Index)));
	}

	float BounceStart = 0.f;
	for (int32 Index = 0; Index < DropBounceCount; ++Index)
	{
		const float HeightRatio = FMath::Pow(DropBounceDamping, static_cast<float>(Index));
		const float Duration = WeightSum > 0.f ? DropBounceDuration * FMath::Sqrt(HeightRatio) / WeightSum : 0.f;
		if (BounceTime < BounceStart + Duration || Index == DropBounceCount - 1)
		{
			const float SinceContact = BounceTime - BounceStart;
			Pose.Location += FVector::UpVector * HopHeight(PhaseAlpha(SinceContact, Duration), DropBounceHeight * HeightRatio);
			Pose.Scale = ComputeSquashScale(SinceContact, LandingSquash * HeightRatio);
			return Pose;
		}
		BounceStart += Duration;
	}

	return Pose;
}

UMKHPickupAnimationComponent::FPose UMKHPickupAnimationComponent::ComputeRisePose(float Time) const
{
	const float Eased = EaseOutCubic(PhaseAlpha(Time, RiseDuration));

	FPose Pose;
	Pose.Location = GetRestPivotLocation() - FVector::UpVector * (RiseDepth * (1.f - Eased));
	Pose.Scale = FVector(FMath::Lerp(RiseStartScale, 1.f, Eased));
	Pose.SpinDegrees = -RiseSpinDegrees * (1.f - Eased);
	return Pose;
}

UMKHPickupAnimationComponent::FPose UMKHPickupAnimationComponent::ComputeCollectPose(float Time) const
{
	FPose Pose;
	const float DescendStart = CollectRiseDuration + CollectHoldDuration;
	if (Time < CollectRiseDuration)
	{
		Pose.Location = FMath::Lerp(StartLocation, CollectHeadLocation, EaseOutCubic(PhaseAlpha(Time, CollectRiseDuration)));
	}
	else if (Time < DescendStart)
	{
		Pose.Location = CollectHeadLocation;
	}
	else
	{
		const float Eased = EaseInQuad(PhaseAlpha(Time - DescendStart, CollectDescendDuration));
		Pose.Location = FMath::Lerp(CollectHeadLocation, CollectBellyLocation, Eased);
		Pose.Scale = FVector(FMath::Lerp(1.f, CollectEndScale, Eased));
	}

	Pose.SpinDegrees = CollectSpinDegrees * PhaseAlpha(Time, GetAnimationDuration(EMKHPickupAnimationType::Collect));
	return Pose;
}

FVector UMKHPickupAnimationComponent::ComputeSquashScale(float TimeSinceContact, float Amount) const
{
	if (LandingSquashDuration <= 0.f || TimeSinceContact < 0.f || TimeSinceContact >= LandingSquashDuration)
	{
		return FVector::OneVector;
	}

	// Flatter and wider, keeping roughly the same volume.
	const float Squash = Amount * FMath::Sin(PI * TimeSinceContact / LandingSquashDuration);
	return FVector(1.f + Squash * 0.5f, 1.f + Squash * 0.5f, 1.f - Squash);
}

void UMKHPickupAnimationComponent::UpdateCollectTargets()
{
	const APawn* Pawn = TargetPawn.Get();
	if (!IsValid(Pawn))
	{
		return;
	}

	float PawnRadius = 0.f;
	float PawnHalfHeight = 0.f;
	Pawn->GetSimpleCollisionCylinder(PawnRadius, PawnHalfHeight);
	SetCollectTargets(Pawn->GetActorLocation(), PawnHalfHeight);
}

void UMKHPickupAnimationComponent::SetCollectTargets(const FVector& PawnCenter, float PawnHalfHeight)
{
	CollectHeadLocation = PawnCenter + FVector::UpVector * (PawnHalfHeight + CollectAboveHeadHeight);
	CollectBellyLocation = PawnCenter + FVector::UpVector * CollectTargetHeightOffset;
}

FVector UMKHPickupAnimationComponent::GetRestPivotLocation() const
{
	return IsValid(PivotComponent) ? GetParentTransform().TransformPosition(PivotComponent->GetRelativeLocation()) : FVector::ZeroVector;
}

FTransform UMKHPickupAnimationComponent::GetParentTransform() const
{
	const USceneComponent* Parent = IsValid(AnimatedComponent) ? AnimatedComponent->GetAttachParent() : nullptr;
	return IsValid(Parent) ? Parent->GetComponentTransform() : FTransform::Identity;
}

void UMKHPickupAnimationComponent::ApplyPose(const FPose& Pose) const
{
	if (!IsValid(AnimatedComponent) || !IsValid(PivotComponent))
	{
		return;
	}

	// Scale and spin around the pivot P (in the animated component's space), then move P to the pose:
	// relative = Spin * Scale * (x - P) + P + Offset.
	const FVector Pivot = PivotComponent->GetRelativeLocation();
	const FVector Offset = GetParentTransform().InverseTransformVector(Pose.Location - GetRestPivotLocation());
	const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(Pose.SpinDegrees));
	const FVector Translation = Pivot + Offset - Spin.RotateVector(Pose.Scale * Pivot);
	AnimatedComponent->SetRelativeTransform(FTransform(Spin, Translation, Pose.Scale));
}

void UMKHPickupAnimationComponent::FinishAnimation()
{
	const EMKHPickupAnimationType FinishedType = ActiveType;
	if (FinishedType == EMKHPickupAnimationType::Collect)
	{
		// Stays at the end point until the server destroys the pickup.
		UpdateCollectTargets();
		ApplyPose(ComputePose(GetAnimationDuration(FinishedType)));
		AnimatedComponent->SetVisibility(false, true);
	}
	else
	{
		AnimatedComponent->SetRelativeTransform(FTransform::Identity);
	}

	// Cleared before broadcasting, so listeners already see the component as idle.
	ResetPlayback();
	OnAnimationFinished.Broadcast(FinishedType);
}

void UMKHPickupAnimationComponent::ResetPlayback()
{
	ActiveType = EMKHPickupAnimationType::None;
	Elapsed = 0.f;
	TargetPawn.Reset();
	SetComponentTickEnabled(false);
}
