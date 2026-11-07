// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Lobby/MKHLobbyGameState.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Lobby/MKHLobbyLineupSubsystem.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "UI/Loading/MKHLoadingScreenSubsystem.h"

// ============================================================
// Lifecycle
// ============================================================

void AMKHLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMKHLobbyGameState, MatchCountdownEndTime);
}

void AMKHLobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	if (UMKHLobbyLineupSubsystem* Lineup = UMKHLobbyLineupSubsystem::Get(this); IsValid(Lineup))
	{
		Lineup->RequestRebuild();
	}
}

void AMKHLobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);

	if (IsPlayerLeavingRoom(PlayerState))
	{
		if (UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this); IsValid(Lobby))
		{
			Lobby->PostPlayerLeftNotice(PlayerState);
		}
	}

	if (UMKHLobbyLineupSubsystem* Lineup = UMKHLobbyLineupSubsystem::Get(this); IsValid(Lineup))
	{
		Lineup->RequestRebuild();
	}
}

// ============================================================
// Public Interface
// ============================================================

void AMKHLobbyGameState::SetMatchCountdown(float Duration)
{
	if (!HasAuthority())
	{
		return;
	}

	MatchCountdownEndTime = Duration < 0.f ? -1.0 : GetServerWorldTimeSeconds() + Duration;
}

float AMKHLobbyGameState::GetMatchCountdownRemaining() const
{
	if (MatchCountdownEndTime < 0.0)
	{
		return -1.f;
	}

	return static_cast<float>(FMath::Max(0.0, MatchCountdownEndTime - GetServerWorldTimeSeconds()));
}

// ============================================================
// Internal Logic
// ============================================================

bool AMKHLobbyGameState::IsPlayerLeavingRoom(const APlayerState* PlayerState) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(PlayerState) || !IsValid(World) || !HasActorBegunPlay() || World->bIsTearingDown || World->IsInSeamlessTravel())
	{
		return false;
	}

	// The server's inactive copy of a leaving player (AGameMode::AddInactivePlayer) has no owner: not a second departure.
	if (HasAuthority() && !IsValid(PlayerState->GetOwner()))
	{
		return false;
	}

	// The local player never "leaves" its own lobby, and a travel under the loading screen is not a departure.
	const APlayerController* LocalController = World->GetFirstPlayerController();
	const UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(this);
	const bool bLocal = IsValid(LocalController) && LocalController->PlayerState == PlayerState;
	return !bLocal && !PlayerState->IsInactive() && (!IsValid(Loading) || !Loading->IsLoadingScreenActive());
}
