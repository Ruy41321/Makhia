// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/Pickup/MKHPickupItem.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Data/MKHDataSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/MKHItemSubsystem.h"
#include "Inventory/Pickup/MKHPickupAnimationComponent.h"
#include "MKHLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "UI/HUD/Pickup/MKHItemPickupWidget.h"

AMKHPickupItem::AMKHPickupItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	PickupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PickupRoot"));
	SetRootComponent(PickupRoot);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(PickupRoot);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(VisualRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupMesh->SetGenerateOverlapEvents(false);
	PickupMesh->SetCanEverAffectNavigation(false);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(PickupRoot);
	InteractionSphere->InitSphereRadius(250.f);
	InteractionSphere->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->SetCanEverAffectNavigation(false);

	PickupAnimation = CreateDefaultSubobject<UMKHPickupAnimationComponent>(TEXT("PickupAnimation"));
}

void AMKHPickupItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMKHPickupItem, Item);
	DOREPLIFETIME(AMKHPickupItem, bExclusive);
	DOREPLIFETIME(AMKHPickupItem, ExclusiveCollector);
	DOREPLIFETIME(AMKHPickupItem, AnimationState);
}

void AMKHPickupItem::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (UMKHItemSubsystem* ItemSubsystem = UMKHItemSubsystem::Get(this))
		{
			ItemSubsystem->RegisterPickup(this);
		}
	}

	if (IsValid(PickupAnimation))
	{
		PickupAnimation->OnAnimationFinished.AddUObject(this, &AMKHPickupItem::HandlePickupAnimationFinished);
	}

	// Before the visuals, so the Blueprint already sees the animation when the item is initialized.
	PlayAnimationState();

	// After Super::BeginPlay so Blueprint components (e.g. widget components) are ready for the event.
	InitializeFromItem();
}

void AMKHPickupItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPickupMeshLoad();

	if (HasAuthority())
	{
		if (UMKHItemSubsystem* ItemSubsystem = UMKHItemSubsystem::Get(this))
		{
			ItemSubsystem->UnregisterPickup(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

AMKHPickupItem* AMKHPickupItem::SpawnGeneratedPickup(const UObject* WorldContextObject, const FGameplayTag& ItemTag, int32 Quantity,
	const FTransform& SpawnTransform, APlayerState* ExclusiveCollector, EMKHPickupAppearance Appearance, APawn* DroppedBy)
{
	if (Quantity < 1)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("AMKHPickupItem::SpawnGeneratedPickup: invalid quantity %d for '%s'."), Quantity, *ItemTag.ToString());
		return nullptr;
	}

	UMKHItemSubsystem* ItemSubsystem = UMKHItemSubsystem::Get(WorldContextObject);
	if (!IsValid(ItemSubsystem))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("AMKHPickupItem::SpawnGeneratedPickup: UMKHItemSubsystem unavailable."));
		return nullptr;
	}

	// GenerateItem already logs the reason (no authority, unknown tag) when it fails.
	const FMKHItemEntry NewItem = ItemSubsystem->GenerateItem(ItemTag, Quantity);
	return NewItem.IsValid()
		? SpawnPickupFromItem(WorldContextObject, NewItem, SpawnTransform, ExclusiveCollector, Appearance, DroppedBy)
		: nullptr;
}

AMKHPickupItem* AMKHPickupItem::SpawnPickupFromItem(const UObject* WorldContextObject, const FMKHItemEntry& InItem,
	const FTransform& SpawnTransform, APlayerState* ExclusiveCollector, EMKHPickupAppearance Appearance, APawn* DroppedBy)
{
	UWorld* World = IsValid(GEngine)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("AMKHPickupItem::SpawnPickupFromItem: ignored, pickups are spawned by the server only."));
		return nullptr;
	}

	if (!InItem.IsValid())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("AMKHPickupItem::SpawnPickupFromItem: invalid item '%s'."), *InItem.ItemTag.ToString());
		return nullptr;
	}

	AMKHPickupItem* Pickup = World->SpawnActorDeferred<AMKHPickupItem>(ResolvePickupClass(World), SpawnTransform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Pickup))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("AMKHPickupItem::SpawnPickupFromItem: failed to spawn the pickup for '%s'."), *InItem.ItemTag.ToString());
		return nullptr;
	}

	// Set before BeginPlay so the first initialization and the initial replication carry the item and its appearance.
	Pickup->Item = InItem;
	Pickup->bExclusive = IsValid(ExclusiveCollector);
	Pickup->ExclusiveCollector = ExclusiveCollector;
	Pickup->InitializeAppearance(Appearance, DroppedBy);
	Pickup->FinishSpawning(SpawnTransform);
	return Pickup;
}

FVector AMKHPickupItem::FindRestLocation(const UWorld& World, const FVector& Origin, const FVector& Target, float WallClearance,
	float GroundTraceDistance, float FallbackZ, const FCollisionQueryParams& QueryParams)
{
	// Zero-length traces are skipped: no scatter keeps the origin, no ground distance keeps FallbackZ.
	const FVector ToTarget = Target - Origin;
	float Distance = ToTarget.Size();
	FHitResult Hit;
	if (!ToTarget.IsNearlyZero() && World.LineTraceSingleByChannel(Hit, Origin, Target, ECC_Visibility, QueryParams))
	{
		Distance = FMath::Max(Hit.Distance - WallClearance, 0.f);
	}

	FVector RestLocation = Origin + ToTarget.GetSafeNormal() * Distance;
	const FVector GroundTraceEnd = RestLocation - FVector::UpVector * GroundTraceDistance;
	const bool bGroundFound = GroundTraceDistance > 0.f
		&& World.LineTraceSingleByChannel(Hit, RestLocation, GroundTraceEnd, ECC_Visibility, QueryParams);
	RestLocation.Z = bGroundFound ? Hit.Location.Z : FallbackZ;
	return RestLocation;
}

bool AMKHPickupItem::CanBeCollectedBy(const APlayerState* Collector) const
{
	return !bExclusive || (IsValid(ExclusiveCollector) && ExclusiveCollector == Collector);
}

bool AMKHPickupItem::IsCollectibleByLocalPlayer() const
{
	const APlayerController* LocalController = IsValid(GEngine) ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	return CanBeCollectedBy(IsValid(LocalController) ? LocalController->PlayerState.Get() : nullptr);
}

bool AMKHPickupItem::IsPawnInInteractionRange(const APawn& Pawn, float ExtraReach) const
{
	if (!IsValid(InteractionSphere))
	{
		return false;
	}

	float PawnRadius = 0.f;
	float PawnHalfHeight = 0.f;
	Pawn.GetSimpleCollisionCylinder(PawnRadius, PawnHalfHeight);

	// Sphere vs upright capsule: distance from the sphere center to the capsule's inner segment.
	const FVector SphereCenter = InteractionSphere->GetComponentLocation();
	const FVector PawnCenter = Pawn.GetActorLocation();
	const double SegmentHalfLength = FMath::Max(PawnHalfHeight - PawnRadius, 0.f);
	const FVector ClosestOnSegment(PawnCenter.X, PawnCenter.Y,
		FMath::Clamp(SphereCenter.Z, PawnCenter.Z - SegmentHalfLength, PawnCenter.Z + SegmentHalfLength));

	const double Reach = InteractionSphere->GetScaledSphereRadius() + PawnRadius + FMath::Max(ExtraReach, 0.f);
	return FVector::DistSquared(SphereCenter, ClosestOnSegment) <= FMath::Square(Reach);
}

FVector AMKHPickupItem::GetInteractionFocusPoint() const
{
	// The root lies on the ground while the mesh hovers above it: aim at what the player sees.
	if (IsValid(PickupMesh) && IsValid(PickupMesh->GetStaticMesh()))
	{
		return PickupMesh->Bounds.Origin;
	}

	return GetActorLocation();
}

bool AMKHPickupItem::TryClaimForPickup()
{
	if (bBeingCollected)
	{
		return false;
	}

	bBeingCollected = true;
	return true;
}

void AMKHPickupItem::SetInteractionState(bool bNewIsCandidate, bool bNewIsFocused)
{
	if (bIsInteractionCandidate == bNewIsCandidate && bIsInteractionFocused == bNewIsFocused)
	{
		return;
	}

	const bool bGainedFocus = bNewIsFocused && !bIsInteractionFocused;
	bIsInteractionCandidate = bNewIsCandidate;
	bIsInteractionFocused = bNewIsFocused;
	OnInteractionStateChanged(bIsInteractionCandidate, bIsInteractionFocused);

	// The setting may have changed since the last focus.
	if (bGainedFocus)
	{
		RefreshHoldHint();
	}
}

void AMKHPickupItem::RefreshHoldHint() const
{
	ForEachPickupWidget([this](UMKHItemPickupWidget& Widget)
	{
		Widget.SetHoldHint(Item);
	});
}

void AMKHPickupItem::ForEachPickupWidget(TFunctionRef<void(UMKHItemPickupWidget&)> Function) const
{
	TInlineComponentArray<UWidgetComponent*> WidgetComponents(this);
	for (const UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		UMKHItemPickupWidget* PickupWidget = IsValid(WidgetComponent) ? Cast<UMKHItemPickupWidget>(WidgetComponent->GetUserWidgetObject()) : nullptr;
		if (IsValid(PickupWidget))
		{
			Function(*PickupWidget);
		}
	}
}

void AMKHPickupItem::StartCollect(APawn* Collector)
{
	if (!HasAuthority())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: StartCollect ignored, pickups are collected by the server only."), *GetName());
		return;
	}

	if (IsBeingCollected())
	{
		return;
	}

	FMKHPickupAnimationState NewState;
	NewState.Type = EMKHPickupAnimationType::Collect;
	NewState.Pawn = Collector;
	NewState.Origin = IsValid(Collector) ? Collector->GetActorLocation() : GetActorLocation();
	NewState.ServerStartTime = GetServerTimeSeconds();
	AnimationState = NewState;
	ForceNetUpdate();

	const float DestroyDelay = IsValid(PickupAnimation) ? PickupAnimation->GetCollectDestroyDelay() : 0.f;
	if (DestroyDelay <= 0.f)
	{
		Destroy();
		return;
	}

	SetLifeSpan(DestroyDelay);
	// The server gets no OnRep: plays it here for the listen-server host.
	PlayAnimationState();
}

bool AMKHPickupItem::IsPlayingPickupAnimation() const
{
	return IsValid(PickupAnimation) && PickupAnimation->IsPlaying();
}

bool AMKHPickupItem::IsInteractable() const
{
	return !IsBeingCollected() && !IsPlayingPickupAnimation();
}

bool AMKHPickupItem::HasFinishedAppearing() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) && World->GetTimeSeconds() >= ServerAppearEndTime;
}

void AMKHPickupItem::OnRep_AnimationState()
{
	// Before BeginPlay the animation is started by BeginPlay itself.
	if (HasActorBegunPlay())
	{
		PlayAnimationState();
	}
}

void AMKHPickupItem::InitializeAppearance(EMKHPickupAppearance Appearance, APawn* DroppedBy)
{
	if (Appearance == EMKHPickupAppearance::Drop && !IsValid(DroppedBy))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: Drop appearance without a dropping pawn, the pickup appears instantly."), *GetName());
		return;
	}

	if (Appearance == EMKHPickupAppearance::Instant || !IsValid(PickupAnimation) || !IsValid(GetWorld()))
	{
		return;
	}

	FMKHPickupAnimationState NewState;
	NewState.Type = Appearance == EMKHPickupAppearance::Drop ? EMKHPickupAnimationType::Drop : EMKHPickupAnimationType::RiseFromGround;
	NewState.Pawn = DroppedBy;
	// Clients that do not know the dropping pawn start the drop from where the server saw it.
	NewState.Origin = IsValid(DroppedBy) ? PickupAnimation->ComputeDropOrigin(*DroppedBy) : GetActorLocation();
	NewState.ServerStartTime = GetServerTimeSeconds();
	AnimationState = NewState;

	ServerAppearEndTime = GetWorld()->GetTimeSeconds() + PickupAnimation->GetAnimationDuration(NewState.Type);
}

void AMKHPickupItem::PlayAnimationState()
{
	if (AnimationState.Type == EMKHPickupAnimationType::None || !IsValid(PickupAnimation))
	{
		return;
	}

	// Received too late (e.g. the pickup became relevant long after): show the final state instead of replaying it.
	if (IsAnimationStateExpired())
	{
		PickupAnimation->SkipToEnd(AnimationState.Type, VisualRoot);
		return;
	}

	PickupAnimation->Play(AnimationState.Type, VisualRoot, PickupMesh, AnimationState.Pawn, AnimationState.Origin);
	if (PickupAnimation->IsPlaying())
	{
		OnPickupAnimationStarted(AnimationState.Type);
	}
}

bool AMKHPickupItem::IsAnimationStateExpired() const
{
	const double Now = GetServerTimeSeconds();
	if (Now < 0.0 || !IsValid(PickupAnimation))
	{
		return true;
	}

	const double Lateness = Now - AnimationState.ServerStartTime - PickupAnimation->GetAnimationDuration(AnimationState.Type);
	return Lateness > PickupAnimation->GetLateStartTolerance();
}

void AMKHPickupItem::HandlePickupAnimationFinished(EMKHPickupAnimationType FinishedType)
{
	OnPickupAnimationFinished(FinishedType);
}

double AMKHPickupItem::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return -1.0;
	}

	const AGameStateBase* GameState = World->GetGameState();
	if (IsValid(GameState))
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return HasAuthority() ? World->GetTimeSeconds() : -1.0;
}

void AMKHPickupItem::OnRep_Item()
{
	// Before BeginPlay the item is applied by BeginPlay itself.
	if (HasActorBegunPlay())
	{
		InitializeFromItem();
	}
}

void AMKHPickupItem::InitializeFromItem()
{
	CancelPickupMeshLoad();

	if (!Item.IsValid())
	{
		// A client may begin play before the item replicates: OnRep_Item follows.
		if (HasAuthority())
		{
			UE_LOG(LogMKHInventory, Warning, TEXT("%s: pickup has no item, spawn it with SpawnGeneratedPickup / SpawnPickupFromItem or an item spawn point."), *GetName());
		}
		return;
	}

	const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(this);
	const FMasterItemDefinition* ItemDef = IsValid(DataSubsystem) ? DataSubsystem->FindItemDefinition(Item.ItemTag) : nullptr;
	if (!ItemDef || ItemDef->PickupMesh.IsNull())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: no PickupMesh configured for '%s', keeping the default mesh."), *GetName(), *Item.ItemTag.ToString());
		OnPickupItemInitialized(Item);
		return;
	}

	LoadAndApplyPickupMesh(ItemDef->PickupMesh, ItemDef->PickupMeshTransform);
}

void AMKHPickupItem::LoadAndApplyPickupMesh(const TSoftObjectPtr<UStaticMesh>& MeshAsset, const FTransform& MeshTransform)
{
	if (UStaticMesh* LoadedMesh = MeshAsset.Get())
	{
		ApplyPickupMesh(LoadedMesh, MeshTransform);
		return;
	}

	const TWeakObjectPtr<AMKHPickupItem> WeakThis(this);
	PickupMeshLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MeshAsset.ToSoftObjectPath(),
		[WeakThis, MeshAsset, MeshTransform]
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ApplyPickupMesh(MeshAsset.Get(), MeshTransform);
			}
		});
}

void AMKHPickupItem::ApplyPickupMesh(UStaticMesh* Mesh, const FTransform& MeshTransform)
{
	if (!IsValid(Mesh))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: failed to load the PickupMesh of '%s', keeping the default mesh."), *GetName(), *Item.ItemTag.ToString());
	}
	else if (IsValid(PickupMesh))
	{
		PickupMesh->SetStaticMesh(Mesh);
		PickupMesh->SetRelativeTransform(MeshTransform * GetDefaultPickupMeshTransform());
	}

	OnPickupItemInitialized(Item);
}

FTransform AMKHPickupItem::GetDefaultPickupMeshTransform() const
{
	// The archetype is the mesh component template of the (Blueprint) class, never modified at runtime.
	const USceneComponent* MeshTemplate = IsValid(PickupMesh) ? Cast<USceneComponent>(PickupMesh->GetArchetype()) : nullptr;
	return IsValid(MeshTemplate) ? MeshTemplate->GetRelativeTransform() : FTransform::Identity;
}

void AMKHPickupItem::CancelPickupMeshLoad()
{
	if (PickupMeshLoadHandle.IsValid())
	{
		PickupMeshLoadHandle->CancelHandle();
		PickupMeshLoadHandle.Reset();
	}
}

UClass* AMKHPickupItem::ResolvePickupClass(const UObject* WorldContextObject)
{
	const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(WorldContextObject);
	UClass* PickupClass = IsValid(DataSubsystem) ? DataSubsystem->GetPickupItemClass().Get() : nullptr;
	if (!IsValid(PickupClass))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("AMKHPickupItem: 'PickupItemClass' not set in Project Settings -> Game -> Makhia Data, spawning the native class."));
		return StaticClass();
	}
	return PickupClass;
}
