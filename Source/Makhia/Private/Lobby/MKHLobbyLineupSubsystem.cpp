// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Lobby/MKHLobbyLineupSubsystem.h"

#include "Algo/Sort.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Lobby/MKHLobbyPlayerPlatform.h"
#include "MKHLogChannels.h"
#include "TimerManager.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHLobbyLineupSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	GatherPlatforms();
	RequestRebuild();
}

void UMKHLobbyLineupSubsystem::Deinitialize()
{
	Platforms.Reset();
	bLineupReady = false;

	Super::Deinitialize();
}

bool UMKHLobbyLineupSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ============================================================
// Public Interface
// ============================================================

UMKHLobbyLineupSubsystem* UMKHLobbyLineupSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return IsValid(World) ? World->GetSubsystem<UMKHLobbyLineupSubsystem>() : nullptr;
}

void UMKHLobbyLineupSubsystem::RequestRebuild()
{
	UWorld* World = GetWorld();
	if (bRebuildPending || Platforms.IsEmpty() || !IsValid(World))
	{
		return;
	}

	bRebuildPending = true;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UMKHLobbyLineupSubsystem::Rebuild));
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHLobbyLineupSubsystem::GatherPlatforms()
{
	Platforms.Reset();
	for (TActorIterator<AMKHLobbyPlayerPlatform> It(GetWorld()); It; ++It)
	{
		Platforms.Add(*It);
	}

	Algo::Sort(Platforms, [](const TObjectPtr<AMKHLobbyPlayerPlatform>& A, const TObjectPtr<AMKHLobbyPlayerPlatform>& B)
	{
		return A->GetSlotIndex() < B->GetSlotIndex();
	});
}

void UMKHLobbyLineupSubsystem::Rebuild()
{
	bRebuildPending = false;

	// Nobody is placed before the local player: on a joining client the others arrive first.
	APlayerState* LocalPlayerState = GetLocalPlayerState();
	const TArray<APlayerState*> Order = IsValid(LocalPlayerState) ? BuildPlayerOrder(LocalPlayerState) : TArray<APlayerState*>();

	for (int32 Slot = 0; Slot < Platforms.Num(); ++Slot)
	{
		AMKHLobbyPlayerPlatform* Platform = Platforms[Slot];
		if (IsValid(Platform))
		{
			Platform->ShowPlayer(Order.IsValidIndex(Slot) ? Order[Slot] : nullptr, Slot == 0);
		}
	}

	if (Order.Num() > Platforms.Num())
	{
		UE_LOG(LogMKHUI, Warning, TEXT("Lobby lineup: %d players but only %d platforms."), Order.Num(), Platforms.Num());
	}

	bLineupReady = IsValid(LocalPlayerState);
	OnLineupChanged.Broadcast();
}

APlayerState* UMKHLobbyLineupSubsystem::GetLocalPlayerState() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	return IsValid(PlayerController) ? PlayerController->PlayerState.Get() : nullptr;
}

TArray<APlayerState*> UMKHLobbyLineupSubsystem::BuildPlayerOrder(APlayerState* LocalPlayerState) const
{
	TArray<APlayerState*> Others;
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr; IsValid(GameState))
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (IsValid(PlayerState) && PlayerState != LocalPlayerState && !PlayerState->IsInactive())
			{
				Others.Add(PlayerState);
			}
		}
	}

	// PlayerId comes from the server: the same order on every machine.
	Others.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });

	TArray<APlayerState*> Order;
	Order.Reserve(Others.Num() + 1);
	Order.Add(LocalPlayerState);
	Order.Append(Others);
	return Order;
}
