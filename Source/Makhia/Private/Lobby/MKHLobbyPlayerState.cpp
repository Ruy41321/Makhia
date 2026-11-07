// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Lobby/MKHLobbyPlayerState.h"

#include "Career/MKHCareerSubsystem.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameInstance/MKHGameInstance.h"
#include "Lobby/MKHLobbyLineupSubsystem.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "Net/UnrealNetwork.h"

// ============================================================
// Lifecycle
// ============================================================

void AMKHLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMKHLobbyPlayerState, LobbyDisplayName);
	DOREPLIFETIME(AMKHLobbyPlayerState, bLobbyReady);
	DOREPLIFETIME(AMKHLobbyPlayerState, LobbySkinId);
	DOREPLIFETIME(AMKHLobbyPlayerState, LobbyCareerLevels);
}

void AMKHLobbyPlayerState::ClientInitialize(AController* C)
{
	Super::ClientInitialize(C);

	// On a client the owning controller may get its player state after BeginPlay.
	InitLocalLobbyInfo();
	InitLocalSkin();
	if (UMKHLobbyLineupSubsystem* Lineup = UMKHLobbyLineupSubsystem::Get(this); IsValid(Lineup))
	{
		Lineup->RequestRebuild();
	}
}

void AMKHLobbyPlayerState::BeginPlay()
{
	Super::BeginPlay();

	InitLocalLobbyInfo();
	InitLocalSkin();
}

void AMKHLobbyPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bLocalInfoInitialized)
	{
		if (UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this); IsValid(GameInstance))
		{
			GameInstance->OnUsernameChanged.RemoveDynamic(this, &AMKHLobbyPlayerState::HandleUsernameChanged);
		}
		if (UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this); IsValid(Lobby))
		{
			Lobby->OnReadyStateChanged.RemoveDynamic(this, &AMKHLobbyPlayerState::HandleReadyStateChanged);
		}
	}
	if (UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this); IsValid(Skins))
	{
		Skins->OnSelectedSkinChanged.Remove(SkinChangedHandle);
	}
	SkinChangedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AMKHLobbyPlayerState::InitLocalLobbyInfo()
{
	if (bLocalInfoInitialized || !HasActorBegunPlay() || !IsOwnedByLocalPlayer())
	{
		return;
	}

	// The offline lobby shows no name and has no ready state.
	UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this);
	UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this);
	if (!IsValid(GameInstance) || !IsValid(Lobby) || !GameInstance->GetIsLoggedIn())
	{
		return;
	}

	bLocalInfoInitialized = true;
	GameInstance->OnUsernameChanged.AddDynamic(this, &AMKHLobbyPlayerState::HandleUsernameChanged);
	Lobby->OnReadyStateChanged.AddDynamic(this, &AMKHLobbyPlayerState::HandleReadyStateChanged);
	ServerInitLobbyInfo(GameInstance->GetUsername(), Lobby->GetIsReady());

	// The progress only changes in a match, and the lobby is rebuilt on return: once is enough.
	if (const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this); IsValid(Career))
	{
		ServerSetLobbyCareerLevels(Career->GetCompletedLevels());
	}
}

void AMKHLobbyPlayerState::InitLocalSkin()
{
	if (SkinChangedHandle.IsValid() || !HasActorBegunPlay() || !IsOwnedByLocalPlayer())
	{
		return;
	}

	UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	if (!IsValid(Skins))
	{
		return;
	}

	SkinChangedHandle = Skins->OnSelectedSkinChanged.AddUObject(this, &AMKHLobbyPlayerState::HandleSkinChanged);
	ServerSetLobbySkin(Skins->GetSelectedSkinId());
}

bool AMKHLobbyPlayerState::IsOwnedByLocalPlayer() const
{
	const APlayerController* PlayerController = GetPlayerController();
	return IsValid(PlayerController) && PlayerController->IsLocalController();
}

void AMKHLobbyPlayerState::HandleUsernameChanged(const FString& NewUsername)
{
	ServerSetLobbyName(NewUsername);
}

void AMKHLobbyPlayerState::HandleReadyStateChanged(const bool bNewReady)
{
	ServerSetLobbyReady(bNewReady);
}

void AMKHLobbyPlayerState::HandleSkinChanged(const FName NewSkinId)
{
	ServerSetLobbySkin(NewSkinId);
}

void AMKHLobbyPlayerState::ServerInitLobbyInfo_Implementation(const FString& NewName, const bool bNewReady)
{
	LobbyDisplayName = NewName;
	bLobbyReady = bNewReady;
	BroadcastLobbyInfoChanged();
}

void AMKHLobbyPlayerState::ServerSetLobbyName_Implementation(const FString& NewName)
{
	if (LobbyDisplayName != NewName)
	{
		LobbyDisplayName = NewName;
		BroadcastLobbyInfoChanged();
	}
}

void AMKHLobbyPlayerState::ServerSetLobbyReady_Implementation(const bool bNewReady)
{
	ApplyLobbyReady(bNewReady);
}

void AMKHLobbyPlayerState::ServerSetLobbySkin_Implementation(const FName NewSkinId)
{
	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	if (!IsValid(Skins) || Skins->ResolveSkinId(NewSkinId) != NewSkinId || LobbySkinId == NewSkinId)
	{
		return;
	}

	LobbySkinId = NewSkinId;
	BroadcastLobbyInfoChanged();
}

void AMKHLobbyPlayerState::ServerSetLobbyCareerLevels_Implementation(const int32 NewCareerLevels)
{
	const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
	const int32 Clamped = FMath::Clamp(NewCareerLevels, 0, IsValid(Career) ? Career->GetLevelCount() : 0);
	if (LobbyCareerLevels != Clamped)
	{
		LobbyCareerLevels = Clamped;
		BroadcastLobbyInfoChanged();
	}
}

void AMKHLobbyPlayerState::ApplyLobbyReady(const bool bNewReady)
{
	// Counting a repeated state would unbalance the host's ready counter.
	if (bLobbyReady == bNewReady)
	{
		return;
	}

	bLobbyReady = bNewReady;
	BroadcastLobbyInfoChanged();

	if (UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this); IsValid(Lobby))
	{
		Lobby->HandlePlayerReady(bNewReady);
	}
}

void AMKHLobbyPlayerState::OnRep_LobbyInfo()
{
	BroadcastLobbyInfoChanged();
}

void AMKHLobbyPlayerState::BroadcastLobbyInfoChanged()
{
	OnLobbyInfoChanged.Broadcast(this);
}
