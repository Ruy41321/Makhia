// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/Spawn/MKHItemSpawnSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Inventory/Pickup/MKHPickupItem.h"
#include "Inventory/Spawn/MKHItemSpawnGroup.h"
#include "Inventory/Spawn/MKHItemSpawnPoint.h"
#include "MKHLogChannels.h"

bool UMKHItemSpawnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UMKHItemSpawnSubsystem* UMKHItemSpawnSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(GEngine)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return IsValid(World) ? World->GetSubsystem<UMKHItemSpawnSubsystem>() : nullptr;
}

void UMKHItemSpawnSubsystem::RegisterSpawnPoint(AMKHItemSpawnPoint* SpawnPoint)
{
	if (IsValid(SpawnPoint))
	{
		SpawnPoints.AddUnique(SpawnPoint);
	}
}

void UMKHItemSpawnSubsystem::UnregisterSpawnPoint(AMKHItemSpawnPoint* SpawnPoint)
{
	SpawnPoints.RemoveSwap(SpawnPoint);
}

void UMKHItemSpawnSubsystem::HandleMatchPhaseChanged(EMKHMatchPhase NewPhase, int32 Round)
{
	// Updated first: the points read IsMatchRunning / GetCurrentRound while reacting.
	CurrentPhase = NewPhase;
	CurrentRound = Round;

	const TArray<AMKHItemSpawnPoint*> Points = GetLiveSpawnPoints();
	for (AMKHItemSpawnPoint* Point : Points)
	{
		switch (NewPhase)
		{
		case EMKHMatchPhase::InitialPreparation:		Point->HandleMatchStarted(); break;
		case EMKHMatchPhase::IntermissionPreparation:	Point->HandleRoundEnded(); break;
		case EMKHMatchPhase::MatchOver:
		case EMKHMatchPhase::None:						Point->HandleMatchEnded(); break;
		default:										break;
		}
	}

	if (IsMatchRunning())
	{
		TriggerPhaseSpawns(NewPhase, Points);
	}
}

bool UMKHItemSpawnSubsystem::IsMatchRunning() const
{
	return CurrentPhase != EMKHMatchPhase::None && CurrentPhase != EMKHMatchPhase::MatchOver;
}

bool UMKHItemSpawnSubsystem::HasGroupCapacity(const AMKHItemSpawnPoint& SpawnPoint) const
{
	const AMKHItemSpawnGroup* Group = SpawnPoint.GetSpawnGroup();
	return !IsValid(Group) || CountOccupiedMembers(*Group, &SpawnPoint, false) < Group->GetMaxActivePickups();
}

TArray<AMKHItemSpawnPoint*> UMKHItemSpawnSubsystem::GetLiveSpawnPoints()
{
	SpawnPoints.RemoveAllSwap([](const TWeakObjectPtr<AMKHItemSpawnPoint>& Point) { return !Point.IsValid(); });

	TArray<AMKHItemSpawnPoint*> Points;
	Points.Reserve(SpawnPoints.Num());
	for (const TWeakObjectPtr<AMKHItemSpawnPoint>& Point : SpawnPoints)
	{
		Points.Add(Point.Get());
	}
	return Points;
}

void UMKHItemSpawnSubsystem::TriggerPhaseSpawns(EMKHMatchPhase Phase, const TArray<AMKHItemSpawnPoint*>& Points)
{
	TMap<const AMKHItemSpawnGroup*, TArray<AMKHItemSpawnPoint*>> TriggeredByGroup;

	for (AMKHItemSpawnPoint* Point : Points)
	{
		if (!IsValid(Point) || !Point->IsTriggeredByPhase(Phase))
		{
			continue;
		}

		if (const AMKHItemSpawnGroup* Group = Point->GetSpawnGroup())
		{
			TriggeredByGroup.FindOrAdd(Group).Add(Point);
			continue;
		}
		Point->BeginSpawnAttempt();
	}

	for (TPair<const AMKHItemSpawnGroup*, TArray<AMKHItemSpawnPoint*>>& Pair : TriggeredByGroup)
	{
		if (IsValid(Pair.Key))
		{
			StartGroupAttempts(*Pair.Key, Pair.Value);
		}
	}
}

void UMKHItemSpawnSubsystem::StartGroupAttempts(const AMKHItemSpawnGroup& Group, TArray<AMKHItemSpawnPoint*>& Triggered)
{
	// Fisher-Yates: any triggered member can get one of the free slots.
	for (int32 Index = Triggered.Num() - 1; Index > 0; --Index)
	{
		Triggered.Swap(Index, FMath::RandRange(0, Index));
	}

	// Pending attempts count here: they will spawn after their delay and must not be oversubscribed.
	int32 FreeSlots = Group.GetMaxActivePickups() - CountOccupiedMembers(Group, nullptr, true);
	for (AMKHItemSpawnPoint* Point : Triggered)
	{
		if (FreeSlots <= 0)
		{
			UE_LOG(LogMKHInventory, Verbose, TEXT("%s: not selected, spawn group %s is full."), *GetNameSafe(Point), *Group.GetName());
			continue;
		}

		if (Point->BeginSpawnAttempt())
		{
			--FreeSlots;
		}
	}
}

int32 UMKHItemSpawnSubsystem::CountOccupiedMembers(const AMKHItemSpawnGroup& Group, const AMKHItemSpawnPoint* Excluded,
	bool bCountPendingAttempts) const
{
	int32 Occupied = 0;
	for (const TWeakObjectPtr<AMKHItemSpawnPoint>& WeakPoint : SpawnPoints)
	{
		const AMKHItemSpawnPoint* Point = WeakPoint.Get();
		if (!IsValid(Point) || Point == Excluded || Point->GetSpawnGroup() != &Group)
		{
			continue;
		}

		const bool bOccupied = bCountPendingAttempts ? Point->IsOccupied() : IsValid(Point->GetActivePickup());
		Occupied += bOccupied ? 1 : 0;
	}
	return Occupied;
}
