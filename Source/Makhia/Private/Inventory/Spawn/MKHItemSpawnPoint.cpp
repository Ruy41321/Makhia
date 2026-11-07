// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/Spawn/MKHItemSpawnPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/MKHGameMode.h"
#include "Inventory/MKHItemSubsystem.h"
#include "Inventory/Pickup/MKHPickupItem.h"
#include "Inventory/Spawn/MKHItemSpawnConditions.h"
#include "Inventory/Spawn/MKHItemSpawnSubsystem.h"
#include "MKHLogChannels.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "Data/MKHDataSettings.h"
#include "Data/MKHDataSubsystem.h"
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "MKHItemSpawnPoint"

namespace
{
	/** Height above the point where the placement traces start, so a point lying on the floor never starts inside it. */
	constexpr float PlacementTraceHeight = 50.f;

	/** How far below the trace start the ground is searched for. */
	constexpr float PlacementGroundTraceDistance = 500.f;

	/** Gap kept between a scattered pickup and a wall between the point and the scattered location. */
	constexpr float PlacementWallClearance = 30.f;

	/** Makes the placement traces ignore every pawn and what is attached to it (weapons). */
	void IgnorePawns(const UWorld& World, FCollisionQueryParams& QueryParams)
	{
		for (TActorIterator<APawn> It(&World); It; ++It)
		{
			TArray<AActor*> AttachedActors;
			It->GetAttachedActors(AttachedActors, true, true);
			QueryParams.AddIgnoredActor(*It);
			QueryParams.AddIgnoredActors(AttachedActors);
		}
	}
}

// ============================================================
// Lifecycle
// ============================================================

AMKHItemSpawnPoint::AMKHItemSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bNetLoadOnClient = false;
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	// Spawn logic runs on the server for the whole match: never stream it out.
	bIsSpatiallyLoaded = false;

	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (IsValid(SpriteComponent))
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture(TEXT("/Engine/EditorResources/Spawn_Point"));
		SpriteComponent->Sprite = SpriteTexture.Get();
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->SetupAttachment(SceneRoot);
	}

	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (IsValid(ArrowComponent))
	{
		ArrowComponent->ArrowSize = 0.5f;
		ArrowComponent->SetupAttachment(SceneRoot);
	}

	PreviewMesh = CreateEditorOnlyDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	if (IsValid(PreviewMesh))
	{
		PreviewMesh->SetupAttachment(SceneRoot);
		PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewMesh->SetGenerateOverlapEvents(false);
		PreviewMesh->SetCanEverAffectNavigation(false);
		PreviewMesh->SetCastShadow(false);
		PreviewMesh->SetHiddenInGame(true);
		PreviewMesh->bIsEditorOnly = true;
	}
#endif
}

void AMKHItemSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	UMKHItemSpawnSubsystem* SpawnSubsystem = UMKHItemSpawnSubsystem::Get(this);
	if (HasAuthority() && IsValid(SpawnSubsystem))
	{
		SpawnSubsystem->RegisterSpawnPoint(this);
	}
}

void AMKHItemSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingAttempt();
	StopTrackingPickup();

	if (UMKHItemSpawnSubsystem* SpawnSubsystem = UMKHItemSpawnSubsystem::Get(this))
	{
		SpawnSubsystem->UnregisterSpawnPoint(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ============================================================
// Public Interface
// ============================================================

bool AMKHItemSpawnPoint::TrySpawn()
{
	if (!HasAuthority())
	{
		return false;
	}

	return BeginSpawnAttempt();
}

APlayerState* AMKHItemSpawnPoint::ResolveExclusiveCollector() const
{
	if (!IsExclusive())
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	const AMKHGameMode* GameMode = IsValid(World) ? World->GetAuthGameMode<AMKHGameMode>() : nullptr;
	return IsValid(GameMode) ? GameMode->FindParticipantAtStartSpot(OwnerPlayerStart) : nullptr;
}

bool AMKHItemSpawnPoint::IsOccupied() const
{
	return bAttemptPending || ActivePickup.IsValid();
}

void AMKHItemSpawnPoint::HandleMatchStarted()
{
	CancelPendingAttempt();
	DestroyActivePickup();
	SpawnCount = 0;
}

void AMKHItemSpawnPoint::HandleRoundEnded()
{
	if (bKeepPickupBetweenRounds)
	{
		return;
	}

	CancelPendingAttempt();
	DestroyActivePickup();
}

void AMKHItemSpawnPoint::HandleMatchEnded()
{
	CancelPendingAttempt();
}

bool AMKHItemSpawnPoint::IsTriggeredByPhase(EMKHMatchPhase Phase) const
{
	return Trigger == EMKHItemSpawnTrigger::PhaseStart && TriggerPhases.Contains(Phase);
}

bool AMKHItemSpawnPoint::BeginSpawnAttempt()
{
	if (!CanScheduleAttempt())
	{
		UE_LOG(LogMKHInventory, Verbose, TEXT("%s: spawn attempt not started (match not running, occupied or spawn limit reached)."), *GetName());
		return false;
	}

	ScheduleAttempt(FMath::FRandRange(SpawnDelayMin, FMath::Max(SpawnDelayMin, SpawnDelayMax)));
	return true;
}

// ============================================================
// Protected / Internal Logic (attempt pipeline)
// ============================================================

bool AMKHItemSpawnPoint::CanScheduleAttempt() const
{
	const bool bBelowLimit = MaxSpawnsPerMatch <= 0 || SpawnCount < MaxSpawnsPerMatch;
	return IsMatchRunning() && !IsOccupied() && bBelowLimit && !IsReservedToBot();
}

bool AMKHItemSpawnPoint::IsReservedToBot() const
{
	if (!IsExclusive())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const AMKHGameMode* GameMode = IsValid(World) ? World->GetAuthGameMode<AMKHGameMode>() : nullptr;
	return IsValid(GameMode) && GameMode->IsStartSpotHeldByBot(OwnerPlayerStart);
}

void AMKHItemSpawnPoint::ScheduleAttempt(float Delay)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	bAttemptPending = true;
	if (Delay <= 0.f)
	{
		ExecuteAttempt();
		return;
	}

	World->GetTimerManager().SetTimer(AttemptTimerHandle, this, &AMKHItemSpawnPoint::ExecuteAttempt, Delay, false);
}

void AMKHItemSpawnPoint::ExecuteAttempt()
{
	bAttemptPending = false;
	if (!IsMatchRunning() || !IsRoundAllowed())
	{
		UE_LOG(LogMKHInventory, Verbose, TEXT("%s: attempt skipped, match not running or round out of range."), *GetName());
		return;
	}

	if (!IsSpawnAllowed())
	{
		if (ConditionFailPolicy == EMKHSpawnConditionFailPolicy::RetryUntilSatisfied)
		{
			ScheduleAttempt(FMath::Max(RetryInterval, 0.1f));
			return;
		}
		UE_LOG(LogMKHInventory, Verbose, TEXT("%s: attempt skipped, a condition failed or the spawn group is full."), *GetName());
		return;
	}

	if (!RollSpawnChance())
	{
		UE_LOG(LogMKHInventory, Verbose, TEXT("%s: attempt skipped by the spawn chance (%.0f%%)."), *GetName(), SpawnChance);
		return;
	}

	SpawnPickup();
}

bool AMKHItemSpawnPoint::IsRoundAllowed() const
{
	const UMKHItemSpawnSubsystem* SpawnSubsystem = UMKHItemSpawnSubsystem::Get(this);
	const int32 Round = IsValid(SpawnSubsystem) ? SpawnSubsystem->GetCurrentRound() : 0;
	return (MinRound <= 0 || Round >= MinRound) && (MaxRound <= 0 || Round <= MaxRound);
}

bool AMKHItemSpawnPoint::IsSpawnAllowed() const
{
	for (const UMKHItemSpawnCondition* Condition : Conditions)
	{
		if (IsValid(Condition) && !Condition->Evaluate(*this))
		{
			return false;
		}
	}

	const UMKHItemSpawnSubsystem* SpawnSubsystem = UMKHItemSpawnSubsystem::Get(this);
	return !IsValid(SpawnSubsystem) || SpawnSubsystem->HasGroupCapacity(*this);
}

bool AMKHItemSpawnPoint::RollSpawnChance() const
{
	return SpawnChance >= 100.f || FMath::FRandRange(0.f, 100.f) < SpawnChance;
}

AMKHPickupItem* AMKHItemSpawnPoint::SpawnPickup()
{
	// Resolved before generating anything, so a missing owner does not consume an ItemID.
	APlayerState* Collector = ResolveExclusiveCollector();
	if (IsExclusive() && !IsValid(Collector))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: no participant started at '%s', nothing spawned."), *GetName(), *GetNameSafe(OwnerPlayerStart));
		return nullptr;
	}

	UMKHItemSubsystem* ItemSubsystem = UMKHItemSubsystem::Get(this);
	const FMKHLootRoll Roll = UMKHLootTable::RollEntries(this, GetLootEntries());
	if (!IsValid(ItemSubsystem) || !Roll.IsValid())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: nothing could be rolled from the loot list."), *GetName());
		return nullptr;
	}

	// GenerateItem logs its own failures (e.g. no definition for the tag).
	const FMKHItemEntry NewItem = ItemSubsystem->GenerateItem(Roll.ItemTag, Roll.Quantity, Roll.RarityRule);
	AMKHPickupItem* Pickup = NewItem.IsValid()
		? AMKHPickupItem::SpawnPickupFromItem(this, NewItem, ComputeSpawnTransform(), Collector, EMKHPickupAppearance::RiseFromGround)
		: nullptr;
	if (!IsValid(Pickup))
	{
		return nullptr;
	}

	TrackPickup(Pickup);
	++SpawnCount;

	const FString CollectorName = IsValid(Collector) ? Collector->GetPlayerName() : TEXT("anyone");
	UE_LOG(LogMKHInventory, Log, TEXT("%s: spawned '%s' x%d (%s, ID %lld) for %s."), *GetName(), *NewItem.ItemTag.ToString(),
		NewItem.Quantity, *NewItem.RarityTag.ToString(), NewItem.ItemID, *CollectorName);
	OnItemSpawned(Pickup);
	return Pickup;
}

FTransform AMKHItemSpawnPoint::ComputeSpawnTransform() const
{
	const FVector PointLocation = GetActorLocation();
	const float Yaw = bRandomYaw ? FMath::FRandRange(0.f, 360.f) : GetActorRotation().Yaw;
	const FRotator Rotation(0.f, Yaw, 0.f);

	const UWorld* World = GetWorld();
	if (!IsValid(World) || (!bSnapToGround && ScatterRadius <= 0.f))
	{
		return FTransform(Rotation, PointLocation);
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ItemSpawnPoint), false, this);
	IgnorePawns(*World, QueryParams);

	// The scatter is traced at a small height, so walls stop it; without ground snapping the point's height is kept.
	const FVector2D Scatter = FMath::RandPointInCircle(ScatterRadius);
	const FVector Origin = PointLocation + FVector::UpVector * PlacementTraceHeight;
	const FVector Target = Origin + FVector(Scatter.X, Scatter.Y, 0.f);
	const float GroundTraceDistance = bSnapToGround ? PlacementGroundTraceDistance : 0.f;
	const FVector Location = AMKHPickupItem::FindRestLocation(*World, Origin, Target, PlacementWallClearance,
		GroundTraceDistance, PointLocation.Z, QueryParams);

	return FTransform(Rotation, Location);
}

// ============================================================
// Protected / Internal Logic (pickup tracking)
// ============================================================

void AMKHItemSpawnPoint::TrackPickup(AMKHPickupItem* Pickup)
{
	StopTrackingPickup();
	if (!IsValid(Pickup))
	{
		return;
	}

	ActivePickup = Pickup;
	Pickup->OnDestroyed.AddDynamic(this, &AMKHItemSpawnPoint::HandleActivePickupDestroyed);
}

void AMKHItemSpawnPoint::HandleActivePickupDestroyed(AActor* DestroyedActor)
{
	ActivePickup.Reset();

	// At match end the phase is already MatchOver here, so the cleanup never schedules a respawn.
	if (!bRespawnAfterCollected || !CanScheduleAttempt())
	{
		return;
	}

	ScheduleAttempt(FMath::FRandRange(RespawnDelayMin, FMath::Max(RespawnDelayMin, RespawnDelayMax)));
}

void AMKHItemSpawnPoint::DestroyActivePickup()
{
	AMKHPickupItem* Pickup = ActivePickup.Get();
	StopTrackingPickup();

	if (IsValid(Pickup))
	{
		Pickup->Destroy();
	}
}

void AMKHItemSpawnPoint::StopTrackingPickup()
{
	if (AMKHPickupItem* Pickup = ActivePickup.Get())
	{
		Pickup->OnDestroyed.RemoveDynamic(this, &AMKHItemSpawnPoint::HandleActivePickupDestroyed);
	}
	ActivePickup.Reset();
}

void AMKHItemSpawnPoint::CancelPendingAttempt()
{
	bAttemptPending = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttemptTimerHandle);
	}
}

const TArray<FMKHLootEntry>& AMKHItemSpawnPoint::GetLootEntries() const
{
	return IsValid(LootTable) ? LootTable->GetEntries() : LootEntries;
}

bool AMKHItemSpawnPoint::IsMatchRunning() const
{
	const UMKHItemSpawnSubsystem* SpawnSubsystem = UMKHItemSpawnSubsystem::Get(this);
	return IsValid(SpawnSubsystem) && SpawnSubsystem->IsMatchRunning();
}

// ============================================================
// Editor
// ============================================================

#if WITH_EDITOR

void AMKHItemSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshPreview();
}

void AMKHItemSpawnPoint::RefreshPreview()
{
	if (!IsValid(PreviewMesh))
	{
		return;
	}

	UStaticMesh* Mesh = nullptr;
	FTransform MeshTransform = FTransform::Identity;
	const UWorld* World = GetWorld();
	if (IsValid(World) && !World->IsGameWorld())
	{
		ResolvePreviewMesh(Mesh, MeshTransform);
	}

	PreviewMesh->SetStaticMesh(Mesh);
	PreviewMesh->SetRelativeTransform(MeshTransform);
}

bool AMKHItemSpawnPoint::ResolvePreviewMesh(UStaticMesh*& OutMesh, FTransform& OutTransform) const
{
	for (const FMKHLootEntry& Entry : GetLootEntries())
	{
		const FMasterItemDefinition* ItemDef = UMKHDataSubsystem::FindItemDefinitionForContext(this, Entry.ItemTag);
		OutMesh = (ItemDef && !ItemDef->PickupMesh.IsNull()) ? ItemDef->PickupMesh.LoadSynchronous() : nullptr;
		if (!IsValid(OutMesh))
		{
			continue;
		}

		// Same composition as AMKHPickupItem: the item transform on top of the pickup class mesh layout.
		const UMKHDataSettings* Settings = UMKHDataSettings::Get();
		const UClass* PickupClass = IsValid(Settings) ? Settings->PickupItemClass.LoadSynchronous() : nullptr;
		const AMKHPickupItem* PickupDefaults = IsValid(PickupClass) ? GetDefault<AMKHPickupItem>(PickupClass) : nullptr;
		const UStaticMeshComponent* LayoutMesh = IsValid(PickupDefaults) ? PickupDefaults->GetPickupMesh() : nullptr;
		OutTransform = ItemDef->PickupMeshTransform * (IsValid(LayoutMesh) ? LayoutMesh->GetRelativeTransform() : FTransform::Identity);
		return true;
	}

	return false;
}

EDataValidationResult AMKHItemSpawnPoint::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const bool bTimingValid = ValidateTiming(Context);
	const bool bRulesValid = ValidateLootAndRules(Context);
	const EDataValidationResult OwnResult = (bTimingValid && bRulesValid) ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
	return CombineDataValidationResults(Result, OwnResult);
}

bool AMKHItemSpawnPoint::ValidateTiming(FDataValidationContext& Context) const
{
	const FText Name = FText::FromString(GetActorNameOrLabel());
	bool bValid = true;

	if (Trigger == EMKHItemSpawnTrigger::PhaseStart && TriggerPhases.IsEmpty())
	{
		Context.AddError(FText::Format(LOCTEXT("NoTriggerPhase", "{0}: the trigger is PhaseStart but no phase is listed, it never spawns."), Name));
		bValid = false;
	}

	if (TriggerPhases.Contains(EMKHMatchPhase::None) || TriggerPhases.Contains(EMKHMatchPhase::MatchOver))
	{
		Context.AddWarning(FText::Format(LOCTEXT("DeadPhase", "{0}: None and MatchOver never trigger a spawn (the match is not running)."), Name));
	}

	if (SpawnDelayMax < SpawnDelayMin || RespawnDelayMax < RespawnDelayMin)
	{
		Context.AddError(FText::Format(LOCTEXT("DelayRange", "{0}: a maximum delay is lower than its minimum."), Name));
		bValid = false;
	}

	if (MaxRound > 0 && MaxRound < MinRound)
	{
		Context.AddError(FText::Format(LOCTEXT("RoundRange", "{0}: MaxRound is lower than MinRound, it never spawns."), Name));
		bValid = false;
	}

	if (SpawnChance <= 0.f)
	{
		Context.AddWarning(FText::Format(LOCTEXT("ZeroChance", "{0}: SpawnChance is 0%, it never spawns."), Name));
	}

	return bValid;
}

bool AMKHItemSpawnPoint::ValidateLootAndRules(FDataValidationContext& Context) const
{
	const FString Name = GetActorNameOrLabel();
	// Class defaults (Blueprint compile) legitimately have no loot yet and never an OwnerPlayerStart (instance only).
	const bool bIsTemplate = IsTemplate();
	bool bValid = true;
	if (!bIsTemplate || !GetLootEntries().IsEmpty())
	{
		bValid = UMKHLootTable::ValidateEntries(GetLootEntries(), Name, Context) != EDataValidationResult::Invalid;
	}

	if (IsValid(LootTable) && !LootEntries.IsEmpty())
	{
		Context.AddWarning(FText::Format(LOCTEXT("InlineIgnored", "{0}: LootTable is set, the inline LootEntries are ignored."), FText::FromString(Name)));
	}

	if (!bIsTemplate && IsExclusive() && !IsValid(OwnerPlayerStart))
	{
		Context.AddError(FText::Format(LOCTEXT("NoOwnerStart", "{0}: the pickups are reserved to a player but OwnerPlayerStart is not set."), FText::FromString(Name)));
		bValid = false;
	}

	for (const UMKHItemSpawnCondition* Condition : Conditions)
	{
		if (!IsValid(Condition))
		{
			Context.AddWarning(FText::Format(LOCTEXT("EmptyCondition", "{0}: an entry of Conditions is empty."), FText::FromString(Name)));
			continue;
		}
		bValid &= Condition->ValidateForSpawnPoint(*this, Context) != EDataValidationResult::Invalid;
	}

	return bValid;
}

#endif

#undef LOCTEXT_NAMESPACE
