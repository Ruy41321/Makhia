// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/Components/Footprints/MKHFootprintComponent.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Audio/MKHSoundEffectSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/MKHPhysicalMaterial.h"

namespace MKHFootprint
{
	/** Prints this small on screen are culled (the default 0.01 dropped them a few metres away). */
	constexpr float FadeScreenSize = 0.001f;
}

// ============================================================
// Lifecycle
// ============================================================

UMKHFootprintComponent::UMKHFootprintComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);

	DefaultFootstepSoundTag = MKHGameplayTags::SFX::FootstepDefault;
}

void UMKHFootprintComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsNetMode(NM_DedicatedServer))
	{
		SetComponentTickEnabled(false);
	}
}

void UMKHFootprintComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ACharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character))
	{
		return;
	}
	const USkeletalMeshComponent* Mesh = Character->GetMesh();
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!IsValid(Mesh) || !IsValid(Capsule))
	{
		return;
	}

	const double GroundZ = Character->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	UpdateFoot(LeftFoot, true, *Mesh, GroundZ);
	UpdateFoot(RightFoot, false, *Mesh, GroundZ);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHFootprintComponent::UpdateFoot(FMKHFootTrack& Track, const bool bLeftFoot, const USkeletalMeshComponent& Mesh, const double GroundZ)
{
	const FName FootBone = bLeftFoot ? LeftFootBone : RightFootBone;
	const double Height = Mesh.GetSocketLocation(FootBone).Z - GroundZ;

	if (Height > LiftHeight)
	{
		Track.bArmed = true;
		return;
	}
	if (!Track.bArmed || Height > PlantHeight)
	{
		return;
	}

	Track.bArmed = false;
	const ACharacter* Character = GetOwnerCharacter();
	const UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (IsValid(Movement) && Movement->IsMovingOnGround())
	{
		HandleFootPlant(bLeftFoot, Mesh);
	}
}

void UMKHFootprintComponent::HandleFootPlant(const bool bLeftFoot, const USkeletalMeshComponent& Mesh)
{
	const FVector Heel = Mesh.GetSocketLocation(bLeftFoot ? LeftFootBone : RightFootBone);
	const FVector Ball = Mesh.GetSocketLocation(bLeftFoot ? LeftBallBone : RightBallBone);
	const FVector Centre = FMath::Lerp(Heel, Ball, PrintCentreAlpha);

	FHitResult Hit;
	if (!TraceGround(Centre, Hit))
	{
		PlayFootstep(nullptr, Centre);
		return;
	}

	const UMKHPhysicalMaterial* Surface = Cast<UMKHPhysicalMaterial>(Hit.PhysMaterial.Get());
	PlayFootstep(Surface, Hit.ImpactPoint);
	if (!IsValid(Surface) || !Surface->HasFootprints())
	{
		return;
	}

	FVector Forward = FVector::VectorPlaneProject(Ball - Heel, Hit.ImpactNormal).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::VectorPlaneProject(GetOwner()->GetActorForwardVector(), Hit.ImpactNormal).GetSafeNormal();
	}
	SpawnFootprintDecal(*Surface, bLeftFoot, Hit, Forward);
}

bool UMKHFootprintComponent::TraceGround(const FVector& Location, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MKHFootprintTrace), /*bTraceComplex*/ true, GetOwner());
	Params.bReturnPhysicalMaterial = true;
	const FVector Start = Location + FVector::UpVector * TraceUp;
	const FVector End = Location - FVector::UpVector * TraceDown;
	return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}

void UMKHFootprintComponent::PlayFootstep(const UMKHPhysicalMaterial* Surface, const FVector& Location) const
{
	const FGameplayTag SoundTag = (IsValid(Surface) && Surface->FootstepSoundTag.IsValid()) ? Surface->FootstepSoundTag : DefaultFootstepSoundTag;
	if (UMKHSoundEffectSubsystem* SoundEffects = UMKHSoundEffectSubsystem::Get(this))
	{
		SoundEffects->PlayAtLocation(SoundTag, Location, GetOwner());
	}
}

void UMKHFootprintComponent::SpawnFootprintDecal(const UMKHPhysicalMaterial& Surface, const bool bLeftFoot, const FHitResult& Hit, const FVector& Forward)
{
	// The decal projects along its X axis (into the ground); the texture's heel-to-toe axis runs along its Y axis.
	const FRotator Rotation = FRotationMatrix::MakeFromXY(-Hit.ImpactNormal, Forward).Rotator();
	const FVector DecalSize(Surface.FootprintProjectionDepth * 0.5f, Surface.FootprintSize.Y * 0.5f, Surface.FootprintSize.X * 0.5f);

	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(this, Surface.GetFootprintDecal(bLeftFoot), DecalSize, Hit.ImpactPoint, Rotation);
	if (!IsValid(Decal))
	{
		return;
	}
	Decal->SetFadeScreenSize(MKHFootprint::FadeScreenSize);
	Decal->SetFadeOut(Surface.FootprintHoldTime, Surface.FootprintFadeTime, /*DestroyOwnerAfterFade*/ false);

	TrimFootprints();
	ActiveFootprints.Add(Decal);
}

void UMKHFootprintComponent::TrimFootprints()
{
	ActiveFootprints.RemoveAll([](const TWeakObjectPtr<UDecalComponent>& Print) { return !Print.IsValid(); });
	while (ActiveFootprints.Num() >= MaxFootprints)
	{
		if (UDecalComponent* Oldest = ActiveFootprints[0].Get())
		{
			Oldest->DestroyComponent();
		}
		ActiveFootprints.RemoveAt(0);
	}
}

ACharacter* UMKHFootprintComponent::GetOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}
