// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Voice/MKHVoiceAuthoritySubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameInstance/MKHGameInstance.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "MKHLogChannels.h"
#include "Online/MKHFriendsSubsystem.h"
#include "Voice/MKHVoicePermissionPolicy.h"
#include "Voice/MKHVoicePlayerComponent.h"

// ============================================================
// Lifecycle
// ============================================================

bool UMKHVoiceAuthoritySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UMKHVoiceAuthoritySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UMKHVoiceAuthoritySubsystem::HandlePostLogin);
}

void UMKHVoiceAuthoritySubsystem::Deinitialize()
{
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(EvaluateTimerHandle);
	}
	Components.Reset();
	AppliedPairs.Reset();
	Super::Deinitialize();
}

void UMKHVoiceAuthoritySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (IsNetworkAuthority())
	{
		InWorld.GetTimerManager().SetTimer(EvaluateTimerHandle, this, &UMKHVoiceAuthoritySubsystem::EvaluateAllPairs, EvaluateInterval, true);
	}
}

// ============================================================
// Public Interface
// ============================================================

UMKHVoiceAuthoritySubsystem* UMKHVoiceAuthoritySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	return IsValid(World) ? World->GetSubsystem<UMKHVoiceAuthoritySubsystem>() : nullptr;
}

void UMKHVoiceAuthoritySubsystem::RegisterVoiceComponent(UMKHVoicePlayerComponent* Component)
{
	if (!IsValid(Component))
	{
		return;
	}
	// A matchmaking lobby is public: whoever joins it could claim to be a friend, so only a passive one counts.
	Component->SetArrivedInPassiveLobby(IsPassiveLobby());
	Components.AddUnique(Component);
	EvaluateAllPairs();
}

void UMKHVoiceAuthoritySubsystem::UnregisterVoiceComponent(UMKHVoicePlayerComponent* Component)
{
	if (Components.Remove(Component) > 0)
	{
		EvaluateAllPairs();
	}
}

void UMKHVoiceAuthoritySubsystem::HandlePreferencesChanged(UMKHVoicePlayerComponent* Component)
{
	if (IsValid(Component))
	{
		Components.AddUnique(Component);
	}
	EvaluateAllPairs();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHVoiceAuthoritySubsystem::IsNetworkAuthority() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer);
}

void UMKHVoiceAuthoritySubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (IsValid(GameMode) && GameMode->GetWorld() == GetWorld())
	{
		EvaluateAllPairs();
	}
}

void UMKHVoiceAuthoritySubsystem::EvaluateAllPairs()
{
	if (!IsNetworkAuthority())
	{
		return;
	}
	PruneComponents();
	AcceptPartyClaims();

	for (const TWeakObjectPtr<UMKHVoicePlayerComponent>& ListenerPtr : Components)
	{
		UMKHVoicePlayerComponent* Listener = ListenerPtr.Get();
		APlayerController* ListenerController = Listener->GetPlayerController();
		const FMKHVoicePeerView ListenerView = MakePeerView(*Listener);
		if (ListenerView.PlayerId.IsEmpty() || !IsValid(ListenerController))
		{
			continue;
		}

		TArray<FMKHVoicePeerStatus> Statuses;
		for (const TWeakObjectPtr<UMKHVoicePlayerComponent>& OtherPtr : Components)
		{
			const UMKHVoicePlayerComponent* Other = OtherPtr.Get();
			const FMKHVoicePeerView OtherView = MakePeerView(*Other);
			if (Other == Listener || OtherView.PlayerId.IsEmpty())
			{
				continue;
			}
			FMKHVoicePeerStatus& Status = Statuses.AddDefaulted_GetRef();
			Status.PlayerId = OtherView.PlayerId;
			Status.Reason = FMKHVoicePermissionPolicy::Evaluate(ListenerView, OtherView, IsKnownPair(*Listener, *Other));
			ApplyMute(*ListenerController, *Other, Status.Reason == EMKHVoiceBlockReason::None);
		}
		Listener->SetPeerStatuses(Statuses);
	}
}

void UMKHVoiceAuthoritySubsystem::AcceptPartyClaims() const
{
	UMKHLobbySubsystem* Lobby = GetWorld()->GetGameInstance()->GetSubsystem<UMKHLobbySubsystem>();
	if (!IsValid(Lobby))
	{
		return;
	}
	for (const TWeakObjectPtr<UMKHVoicePlayerComponent>& ComponentPtr : Components)
	{
		const UMKHVoicePlayerComponent* Component = ComponentPtr.Get();
		const APlayerController* Controller = Component->GetPlayerController();
		const FString PlayerId = Component->GetPlayerId();
		if (Component->HasArrivedInPassiveLobby() && Component->HasServerPreferences() && Component->GetServerPreferences().bJoinedAsParty
			&& IsValid(Controller) && !Controller->IsLocalController() && !PlayerId.IsEmpty() && !Lobby->IsVoicePartyMember(PlayerId))
		{
			UE_LOG(LogMKHVoice, Log, TEXT("Voice: %s joined the lobby as party."), *PlayerId);
			Lobby->AddVoicePartyMember(PlayerId);
		}
	}
}

bool UMKHVoiceAuthoritySubsystem::IsPassiveLobby() const
{
	const UMKHGameInstance* GameInstance = Cast<UMKHGameInstance>(GetWorld()->GetGameInstance());
	const UMKHLobbySubsystem* Lobby = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHLobbySubsystem>() : nullptr;
	return IsValid(Lobby) && GameInstance->IsInLobbyLevel() && !Lobby->GetIsMatchmaking();
}

FMKHVoicePeerView UMKHVoiceAuthoritySubsystem::MakePeerView(const UMKHVoicePlayerComponent& Component)
{
	FMKHVoicePeerView View;
	View.PlayerId = Component.GetPlayerId();
	View.Preferences = Component.GetServerPreferences();
	View.bHasPreferences = Component.HasServerPreferences();
	return View;
}

bool UMKHVoiceAuthoritySubsystem::IsKnownPair(const UMKHVoicePlayerComponent& A, const UMKHVoicePlayerComponent& B) const
{
	const FString IdA = A.GetPlayerId();
	const FString IdB = B.GetPlayerId();
	const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	const UMKHLobbySubsystem* Lobby = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHLobbySubsystem>() : nullptr;
	if (IsValid(Lobby) && (Lobby->IsVoicePartyMember(IdA) || Lobby->IsVoicePartyMember(IdB)))
	{
		return true;
	}

	// In a 1v1 the listen server is one of the two: its own platform friend list is trusted, never a client's word.
	const UMKHFriendsSubsystem* Friends = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHFriendsSubsystem>() : nullptr;
	if (!IsValid(Friends))
	{
		return false;
	}
	const APlayerController* ControllerA = A.GetPlayerController();
	const APlayerController* ControllerB = B.GetPlayerController();
	return (IsValid(ControllerA) && ControllerA->IsLocalController() && Friends->IsFriend(IdB))
		|| (IsValid(ControllerB) && ControllerB->IsLocalController() && Friends->IsFriend(IdA));
}

void UMKHVoiceAuthoritySubsystem::ApplyMute(APlayerController& Listener, const UMKHVoicePlayerComponent& Other, bool bAllowed)
{
	const APlayerController* OtherController = Other.GetPlayerController();
	const APlayerState* OtherState = IsValid(OtherController) ? OtherController->PlayerState.Get() : nullptr;
	if (!IsValid(OtherState) || !OtherState->GetUniqueId().IsValid())
	{
		return;
	}
	const FUniqueNetIdRepl& OtherId = OtherState->GetUniqueId();
	if (bAllowed)
	{
		Listener.GameplayUnmutePlayer(OtherId);
		if (Listener.IsLocalController())
		{
			// The host's own controller also got the "Muted" echo of ClientMutePlayer on the same mute list.
			Listener.ServerUnmutePlayer(OtherId);
		}
	}
	else
	{
		Listener.GameplayMutePlayer(OtherId);
	}

	const FString PairKey = FString::Printf(TEXT("%s|%s"), *GetNameSafe(&Listener), *OtherId.ToString());
	const bool* Previous = AppliedPairs.Find(PairKey);
	if (!Previous || *Previous != bAllowed)
	{
		UE_LOG(LogMKHVoice, Log, TEXT("Voice server: %s %s %s."), *GetNameSafe(&Listener), bAllowed ? TEXT("hears") : TEXT("does not hear"),
			*OtherId.ToString());
		AppliedPairs.Add(PairKey, bAllowed);
	}
}

void UMKHVoiceAuthoritySubsystem::PruneComponents()
{
	Components.RemoveAll([](const TWeakObjectPtr<UMKHVoicePlayerComponent>& Component)
	{
		return !Component.IsValid();
	});
}
