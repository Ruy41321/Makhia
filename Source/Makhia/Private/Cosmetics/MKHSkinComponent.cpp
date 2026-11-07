// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Cosmetics/MKHSkinComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/MKHSkinCatalog.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"
#include "MKHLogChannels.h"

// ============================================================
// Lifecycle
// ============================================================

UMKHSkinComponent::UMKHSkinComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UMKHSkinComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}
	DestroyAccessories();

	Super::EndPlay(EndPlayReason);
}

// ============================================================
// Public Interface
// ============================================================

void UMKHSkinComponent::SetSkin(const FName SkinId)
{
	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	if (!IsValid(Skins))
	{
		UE_LOG(LogMKHCosmetics, Warning, TEXT("%s: no skin subsystem, skin '%s' not applied."), *GetNameSafe(GetOwner()), *SkinId.ToString());
		return;
	}

	const FName ResolvedId = Skins->ResolveSkinId(SkinId);
	const FMKHSkinDefinition* Skin = Skins->FindSkin(ResolvedId);
	if (!Skin || ResolvedId == RequestedSkinId)
	{
		return;
	}

	RequestedSkinId = ResolvedId;
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}

	TArray<FSoftObjectPath> Assets = CollectSkinAssets(*Skin);
	if (Assets.IsEmpty())
	{
		ApplyLoadedSkin(ResolvedId);
		return;
	}

	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Assets),
		FStreamableDelegate::CreateWeakLambda(this, [this, ResolvedId]() { ApplyLoadedSkin(ResolvedId); }));
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSkinComponent::ApplyLoadedSkin(const FName SkinId)
{
	// A newer request may have replaced this one while it was loading.
	if (SkinId != RequestedSkinId)
	{
		return;
	}

	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	const FMKHSkinDefinition* Skin = IsValid(Skins) ? Skins->FindSkin(SkinId) : nullptr;
	USkeletalMeshComponent* Mesh = ResolveTargetMesh();
	if (!Skin || !IsValid(Mesh))
	{
		UE_LOG(LogMKHCosmetics, Warning, TEXT("%s: skin '%s' or target mesh missing."), *GetNameSafe(GetOwner()), *SkinId.ToString());
		return;
	}

	USkeletalMesh* NewMesh = Skin->Mesh.Get();
	if (IsValid(NewMesh) && Mesh->GetSkeletalMeshAsset() != NewMesh)
	{
		// Every skin shares the character skeleton: keep the running animation instead of reinitializing it.
		Mesh->SetSkinnedAssetAndUpdate(NewMesh, false);
	}

	RebuildAccessories(*Skin, Mesh);
	OnSkinApplied.Broadcast(SkinId);
}

void UMKHSkinComponent::RebuildAccessories(const FMKHSkinDefinition& Skin, USkeletalMeshComponent* Mesh)
{
	DestroyAccessories();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(Mesh))
	{
		return;
	}

	for (const FMKHSkinAccessory& Accessory : Skin.Accessories)
	{
		UStaticMesh* AccessoryMesh = Accessory.Mesh.Get();
		if (!IsValid(AccessoryMesh))
		{
			continue;
		}

		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Component->SetStaticMesh(AccessoryMesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetupAttachment(Mesh, Accessory.AttachSocket);
		Component->SetRelativeTransform(Accessory.RelativeTransform);
		Component->RegisterComponent();
		Accessories.Add(Component);
	}
}

void UMKHSkinComponent::DestroyAccessories()
{
	for (UStaticMeshComponent* Component : Accessories)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}
	Accessories.Reset();
}

USkeletalMeshComponent* UMKHSkinComponent::ResolveTargetMesh() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	if (const ACharacter* Character = Cast<ACharacter>(Owner); IsValid(Character))
	{
		return Character->GetMesh();
	}
	return Owner->FindComponentByClass<USkeletalMeshComponent>();
}

TArray<FSoftObjectPath> UMKHSkinComponent::CollectSkinAssets(const FMKHSkinDefinition& Skin)
{
	TArray<FSoftObjectPath> Assets;
	if (!Skin.Mesh.IsNull())
	{
		Assets.Add(Skin.Mesh.ToSoftObjectPath());
	}
	for (const FMKHSkinAccessory& Accessory : Skin.Accessories)
	{
		if (!Accessory.Mesh.IsNull())
		{
			Assets.Add(Accessory.Mesh.ToSoftObjectPath());
		}
	}
	return Assets;
}
