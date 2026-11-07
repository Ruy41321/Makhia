// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Lobby/MKHLobbyCharacterPreview.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/MKHSkinComponent.h"

// ============================================================
// Lifecycle
// ============================================================

AMKHLobbyCharacterPreview::AMKHLobbyCharacterPreview()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Same offset as the player character's mesh on its capsule (feet on the ground, facing +X).
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	SkinComponent = CreateDefaultSubobject<UMKHSkinComponent>(TEXT("SkinComponent"));
}

// ============================================================
// Public Interface
// ============================================================

void AMKHLobbyCharacterPreview::SetSkin(const FName SkinId)
{
	if (!IsValid(SkinComponent) || !IsValid(Mesh))
	{
		return;
	}

	SkinComponent->SetSkin(SkinId);

	// Hide the Blueprint's default look until the first skin is on, so it never flashes while loading.
	if (!bSkinApplied && !SkinComponent->GetSkinId().IsNone())
	{
		Mesh->SetVisibility(false, true);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AMKHLobbyCharacterPreview::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(SkinComponent))
	{
		SkinComponent->OnSkinApplied.AddUObject(this, &AMKHLobbyCharacterPreview::HandleSkinApplied);
	}
	PlayIdle();
}

void AMKHLobbyCharacterPreview::PlayIdle() const
{
	if (!IsValid(Mesh) || !IsValid(IdleAnimation))
	{
		return;
	}

	Mesh->PlayAnimation(IdleAnimation, true);
	if (bRandomizeIdleStart)
	{
		Mesh->SetPosition(FMath::FRand() * IdleAnimation->GetPlayLength(), false);
	}
}

void AMKHLobbyCharacterPreview::HandleSkinApplied(FName SkinId)
{
	if (!IsValid(Mesh))
	{
		return;
	}

	bSkinApplied = true;
	Mesh->SetVisibility(true, true);
	if (!Mesh->IsPlaying())
	{
		PlayIdle();
	}
}
