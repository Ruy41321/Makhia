// Fill out your copyright notice in the Description page of Project Settings.


#include "GameInstance/MKHGameInstance.h"
#include "Career/MKHCareerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameInstance/MKHOnlineSession.h"
#include "GameMode/MKHGameMode.h"
#include "GameMode/MKHMatchMode.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "MKHLogChannels.h"
#include "Misc/PackageName.h"
#include "Online/Auth/MKHAuthSubsystem.h"
#include "Online/Backend/MKHOnlineServicesSubsystem.h"
#include "Online/MKHSessionSubsystem.h"
#include "TimerManager.h"
#include "UI/Loading/MKHLoadingScreenSubsystem.h"

namespace MKHGameInstance
{
	/** Seconds a travel waits after shutting the net driver down (sockets of some transports close a tick later). */
	static constexpr float NetworkReleaseDelay = 0.15f;
}

void UMKHGameInstance::Init()
{
	Super::Init();

	if (UMKHAuthSubsystem* Auth = GetSubsystem<UMKHAuthSubsystem>(); IsValid(Auth))
	{
		Auth->OnAuthStateChangedNative.AddUObject(this, &UMKHGameInstance::HandleAuthStateChanged);
	}
}

void UMKHGameInstance::OnStart()
{
	Super::OnStart();

	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		const UMKHOnlineServicesSubsystem* OnlineServices = GetSubsystem<UMKHOnlineServicesSubsystem>();
		UserSettings->RunFirstLaunchAutoDetect(IsValid(OnlineServices) && OnlineServices->IsRunningOnHandheld());
	}

	if (UMKHAuthSubsystem* Auth = GetSubsystem<UMKHAuthSubsystem>(); IsValid(Auth))
	{
		Auth->TryAutoLogin();
	}

	// The auth state changes above move the phase on; without a subsystem nothing will.
	if (BootPhase == EMKHBootPhase::Pending)
	{
		SetBootPhase(EMKHBootPhase::Done);
	}
}

void UMKHGameInstance::Shutdown()
{
	// The backend would otherwise keep the lobby listed until it times out.
	if (bIsInSession)
	{
		if (UMKHSessionSubsystem* Sessions = GetSubsystem<UMKHSessionSubsystem>(); IsValid(Sessions))
		{
			Sessions->DestroyCurrentSessionDetached();
		}
	}

	Super::Shutdown();
}

void UMKHGameInstance::LoadComplete(const float LoadTime, const FString& MapName)
{
	Super::LoadComplete(LoadTime, MapName);

	bReturningToLobby = false;
	bOpeningOfflineMatch = false;
	if (BootPhase == EMKHBootPhase::Travelling)
	{
		SetBootPhase(EMKHBootPhase::Done);
	}

	// A login that completed while the lobby was travelling offline (e.g. switching from guest to account).
	if (GetIsLoggedIn() && IsInOfflineLobby())
	{
		UE_LOG(LogMKHSession, Log, TEXT("Signed in but the lobby opened offline: reopening it as host."));
		OpenLevel(EMKHLevelType::Lobby, true);
	}
}

TSubclassOf<UOnlineSession> UMKHGameInstance::GetOnlineSessionClass()
{
	return UMKHOnlineSession::StaticClass();
}

UMKHGameInstance* UMKHGameInstance::GetMKHGameInstance(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UMKHGameInstance* MKHGameInstance = Cast<UMKHGameInstance>(World->GetGameInstance()))
		{
			return MKHGameInstance;
		}
	}
	return nullptr;
}

bool UMKHGameInstance::GetIsLoggedIn() const
{
	const UMKHAuthSubsystem* Auth = GetSubsystem<UMKHAuthSubsystem>();
	return IsValid(Auth) && Auth->IsLoggedIn();
}

void UMKHGameInstance::HandleAuthStateChanged(EMKHAuthState NewState, EMKHAuthState OldState)
{
	bool bTravelling = false;
	if (NewState == EMKHAuthState::LoggedIn)
	{
		bTravelling = ApplySignedInUser();
	}
	else if (OldState == EMKHAuthState::LoggedIn)
	{
		HandleSignedOut();
	}

	AdvanceBootPhase(NewState, bTravelling);
}

bool UMKHGameInstance::ApplySignedInUser()
{
	const UMKHAuthSubsystem* Auth = GetSubsystem<UMKHAuthSubsystem>();
	if (!IsValid(Auth))
	{
		return false;
	}

	const FMKHAuthUser User = Auth->GetUser();
	CurrentSessionName = FName(User.UserId + TEXT("_LOBBY"));
	SetUsername(User.DisplayName);

	// Only the offline lobby switches to hosting now: a match keeps running and the next return hosts the lobby.
	if (!IsInOfflineLobby() || bReturningToLobby)
	{
		return false;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Signed in from the offline lobby: reopening it as host."));
	OpenLevel(EMKHLevelType::Lobby, true);
	return true;
}

void UMKHGameInstance::AdvanceBootPhase(EMKHAuthState NewState, bool bTravelling)
{
	if (BootPhase == EMKHBootPhase::Done || BootPhase == EMKHBootPhase::Travelling)
	{
		return;
	}

	switch (NewState)
	{
	case EMKHAuthState::LoggingIn:
		SetBootPhase(EMKHBootPhase::Connecting);
		break;
	case EMKHAuthState::LoggedIn:
		SetBootPhase(bTravelling ? EMKHBootPhase::Travelling : EMKHBootPhase::Done);
		break;
	case EMKHAuthState::Offline:
		SetBootPhase(EMKHBootPhase::Done);
		break;
	default:
		break;
	}
}

void UMKHGameInstance::SetBootPhase(EMKHBootPhase NewPhase)
{
	if (BootPhase == NewPhase)
	{
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Boot phase: %s -> %s."), *UEnum::GetValueAsString(BootPhase), *UEnum::GetValueAsString(NewPhase));
	BootPhase = NewPhase;
	OnBootPhaseChanged.Broadcast(NewPhase);
}

void UMKHGameInstance::HandleSignedOut()
{
	if (bIsInSession)
	{
		HoldAuthLogout();
	}

	const UWorld* World = GetWorld();
	const bool bInOnlineLobby = IsInLobbyLevel() && IsValid(World) && World->GetNetMode() != NM_Standalone;
	if (bInOnlineLobby)
	{
		UE_LOG(LogMKHSession, Log, TEXT("Signed out from an online lobby: reopening the lobby offline."));
		LeaveToNewLobby();
		return;
	}

	// In a session (online match) the name is still needed to leave it; FinishBackToLobby forgets it then.
	if (!bIsInSession)
	{
		CurrentSessionName = NAME_None;
	}
}

void UMKHGameInstance::HoldAuthLogout()
{
	UMKHAuthSubsystem* Auth = GetSubsystem<UMKHAuthSubsystem>();
	if (bHoldingAuthLogout || !IsValid(Auth))
	{
		return;
	}

	bHoldingAuthLogout = true;
	Auth->AddLogoutHold();
}

void UMKHGameInstance::ReleaseAuthLogoutHold()
{
	UMKHAuthSubsystem* Auth = GetSubsystem<UMKHAuthSubsystem>();
	if (!bHoldingAuthLogout || !IsValid(Auth))
	{
		return;
	}

	bHoldingAuthLogout = false;
	Auth->ReleaseLogoutHold();
}

bool UMKHGameInstance::IsInOfflineLobby() const
{
	const UWorld* World = GetWorld();
	return IsInLobbyLevel() && IsValid(World) && World->GetNetMode() == NM_Standalone;
}

void UMKHGameInstance::OnJoinSession()
{
	bIsInSession = true;
	bIsListenServer = false;
}

void UMKHGameInstance::OnLeaveSession()
{
	bIsInSession = false;
	bIsHost = false;
	if (UMKHLobbySubsystem* Lobby = GetLobby(); IsValid(Lobby))
	{
		Lobby->HandleSessionLeft();
	}
}

bool UMKHGameInstance::IsClient() const
{
	return bIsInSession && !bIsHost;
}

void UMKHGameInstance::SetUsername(const FString& NewUsername)
{
	if (Username != NewUsername)
	{
		Username = NewUsername;
		OnUsernameChanged.Broadcast(Username);
	}
}

void UMKHGameInstance::OnLobbyCreated()
{
	bIsHost = true;
	bIsInSession = true;
	if (UMKHLobbySubsystem* Lobby = GetLobby(); IsValid(Lobby))
	{
		Lobby->HandleLobbyHosted();
	}
}

void UMKHGameInstance::BackToLobby(bool bAsHost)
{
	// AGameModeBase::Logout fires once per controller, so a disconnect lands here repeatedly.
	if (bReturningToLobby)
	{
		return;
	}

	if (bOpeningOfflineMatch)
	{
		UE_LOG(LogMKHSession, Log, TEXT("BackToLobby ignored: a match against the bot is opening."));
		return;
	}

	bReturningToLobby = true;
	bPendingReturnAsHost = bAsHost;
	// The other player learns it now, not after the session destroy.
	ReleaseWorldNetDriver();
	if (UMKHLoadingScreenSubsystem* Loading = GetSubsystem<UMKHLoadingScreenSubsystem>(); IsValid(Loading))
	{
		Loading->ShowLoadingScreen(EMKHLoadingReason::ReturningToLobby, EMKHLoadingDestination::Lobby);
	}
	if (UMKHLobbySubsystem* Lobby = GetLobby(); IsValid(Lobby))
	{
		Lobby->ResetLobbyState();
	}

	if (!bIsInSession || !TryDestroyCurrentSession())
	{
		FinishBackToLobby();
		return;
	}

	// A backend that answered inline already travelled: no fallback needed.
	if (!bAwaitingSessionDestroy)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReturnToLobbyTimeoutHandle, this, &UMKHGameInstance::FinishBackToLobby,
			FMath::Max(ReturnToLobbyTimeout, 0.1f), false);
	}
}

bool UMKHGameInstance::TryDestroyCurrentSession()
{
	UMKHSessionSubsystem* Sessions = GetSubsystem<UMKHSessionSubsystem>();
	if (!IsValid(Sessions) || CurrentSessionName == NAME_None)
	{
		UE_LOG(LogMKHSession, Warning,
			TEXT("BackToLobby: no session subsystem or no session name, returning to the lobby without a destroy."));
		return false;
	}

	UE_LOG(LogMKHSession, Log, TEXT("BackToLobby: destroying session '%s' before returning."), *CurrentSessionName.ToString());
	bAwaitingSessionDestroy = true;
	if (!Sessions->DestroyCurrentSession(FMKHSessionOperationComplete::CreateUObject(this, &UMKHGameInstance::HandleDestroySessionComplete)))
	{
		bAwaitingSessionDestroy = false;
		UE_LOG(LogMKHSession, Warning, TEXT("BackToLobby: DestroySession('%s') was refused."), *CurrentSessionName.ToString());
		return false;
	}
	return true;
}

void UMKHGameInstance::HandleDestroySessionComplete(bool bWasSuccessful)
{
	// The timeout may have resumed the return already.
	if (!bAwaitingSessionDestroy)
	{
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("BackToLobby: session '%s' destroy completed (success: %d)."),
		*CurrentSessionName.ToString(), bWasSuccessful ? 1 : 0);

	FinishBackToLobby();
}

void UMKHGameInstance::FinishBackToLobby()
{
	// Whichever of the two resume paths runs first disarms the other, so this runs once per return.
	bAwaitingSessionDestroy = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnToLobbyTimeoutHandle);
	}

	// Clears bIsInSession/bIsHost so the lobby re-hosts itself on arrival.
	OnLeaveSession();
	if (!GetIsLoggedIn())
	{
		CurrentSessionName = NAME_None;
	}
	ReleaseAuthLogoutHold();
	OpenLevel(EMKHLevelType::Lobby, bPendingReturnAsHost);
}

void UMKHGameInstance::LeaveToNewLobby()
{
	// Hosting needs a logged-in player: without one the lobby is reopened offline.
	BackToLobby(GetIsLoggedIn());
}

bool UMKHGameInstance::IsInLobbyLevel() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const FString LobbyLevel = FPackageName::GetShortName(LobbyLevelName);
	return !LobbyLevel.IsEmpty() && UGameplayStatics::GetCurrentLevelName(World, true) == LobbyLevel;
}

void UMKHGameInstance::ServerTravel(EMKHLevelType LevelType)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FString URL = FString::Printf(TEXT("%s?listen"), *ResolveLevelName(LevelType));
	TWeakObjectPtr<UWorld> WeakWorld = World;
	auto Travel = [WeakWorld, URL]()
	{
		if (UWorld* TravelWorld = WeakWorld.Get(); IsValid(TravelWorld))
		{
			TravelWorld->ServerTravel(URL);
		}
	};

	if (UMKHLoadingScreenSubsystem* Loading = ShowLoadingScreenForTravel(LevelType); IsValid(Loading))
	{
		Loading->RunWhenCovered(MoveTemp(Travel));
		return;
	}
	Travel();
}

void UMKHGameInstance::OpenLevel(EMKHLevelType LevelType, bool bAsListen)
{
	OpenLevelWithOptions(LevelType, bAsListen, FString());
}

void UMKHGameInstance::StartOfflineMatch(const FMKHOfflineMatchRequest& Request)
{
	UMKHLobbySubsystem* Lobby = GetLobby();
	if (IsValid(Lobby))
	{
		Lobby->CancelOfflineMatchRequest();
		if (Lobby->IsOfflineMatchRefused())
		{
			return;
		}
	}

	if (bIsInSession)
	{
		// Nothing is created on arrival that the pending destroy could collide with (unlike BackToLobby).
		if (UMKHSessionSubsystem* Sessions = GetSubsystem<UMKHSessionSubsystem>(); IsValid(Sessions))
		{
			UE_LOG(LogMKHSession, Log, TEXT("StartOfflineMatch: leaving session '%s'."), *CurrentSessionName.ToString());
			Sessions->DestroyCurrentSessionDetached();
		}
		OnLeaveSession();
	}
	if (IsValid(Lobby))
	{
		Lobby->ResetLobbyState();
	}

	UE_LOG(LogMKHSession, Log, TEXT("StartOfflineMatch: opening the game level against the bot."));
	bOpeningOfflineMatch = true;
	if (UMKHLoadingScreenSubsystem* Loading = GetSubsystem<UMKHLoadingScreenSubsystem>(); IsValid(Loading))
	{
		Loading->ShowLoadingScreen(EMKHLoadingReason::OfflineMatch, EMKHLoadingDestination::Game);
	}
	FString Options = FString::Printf(TEXT("?%s=%s"), MKHMatchMode::OptionKey, MKHMatchMode::OfflineBotValue);
	if (Request.Mode == EMKHOfflineMode::Training)
	{
		Options += FString::Printf(TEXT("?%s=%s"), MKHMatchMode::OfflineModeOptionKey, MKHMatchMode::TrainingValue);
	}
	else if (Request.Mode == EMKHOfflineMode::Career)
	{
		Options += FString::Printf(TEXT("?%s=%s?%s=%d"), MKHMatchMode::OfflineModeOptionKey, MKHMatchMode::CareerValue,
			MKHMatchMode::CareerLevelOptionKey, Request.CareerLevelIndex);
	}
	OpenLevelWithOptions(EMKHLevelType::Game, false, Options);
}

bool UMKHGameInstance::CanStartNextCareerLevel() const
{
	const UWorld* World = GetWorld();
	const AMKHGameMode* GameMode = IsValid(World) ? World->GetAuthGameMode<AMKHGameMode>() : nullptr;
	const UMKHCareerSubsystem* Career = GetSubsystem<UMKHCareerSubsystem>();
	if (!IsInOfflineMatch() || !IsValid(Career) || GameMode->GetOfflineRequest().Mode != EMKHOfflineMode::Career)
	{
		return false;
	}

	return Career->IsLevelUnlocked(GameMode->GetOfflineRequest().CareerLevelIndex + 1);
}

void UMKHGameInstance::StartNextCareerLevel()
{
	if (!CanStartNextCareerLevel())
	{
		UE_LOG(LogMKHSession, Warning, TEXT("StartNextCareerLevel: ignored, there is no next level to play."));
		return;
	}

	FMKHOfflineMatchRequest Request = GetWorld()->GetAuthGameMode<AMKHGameMode>()->GetOfflineRequest();
	++Request.CareerLevelIndex;
	StartOfflineMatch(Request);
}

void UMKHGameInstance::RestartOfflineMatch()
{
	if (!IsInOfflineMatch())
	{
		UE_LOG(LogMKHSession, Warning, TEXT("RestartOfflineMatch: ignored, the current level is not a match against the bot."));
		return;
	}

	// Same path as leaving and starting again from the lobby, minus the lobby.
	UE_LOG(LogMKHSession, Log, TEXT("RestartOfflineMatch: restarting the match against the bot from scratch."));
	StartOfflineMatch(GetWorld()->GetAuthGameMode<AMKHGameMode>()->GetOfflineRequest());
}

bool UMKHGameInstance::IsInOfflineMatch() const
{
	const UWorld* World = GetWorld();
	const AMKHGameMode* GameMode = IsValid(World) ? World->GetAuthGameMode<AMKHGameMode>() : nullptr;
	return IsValid(GameMode) && GameMode->GetMatchMode() == EMKHMatchMode::OfflineBot;
}

void UMKHGameInstance::MKHPlayVsBot()
{
	StartOfflineMatch(FMKHOfflineMatchRequest());
}

void UMKHGameInstance::OpenLevelWithOptions(EMKHLevelType LevelType, bool bAsListen, const FString& Options)
{
	FString FinalUrl = ResolveLevelName(LevelType);
	bIsListenServer = bAsListen;
	if (bAsListen)
	{
		FinalUrl += TEXT("?listen");
	}
	FinalUrl += Options;

	ReleaseWorldNetDriver();
	TWeakObjectPtr<UMKHGameInstance> WeakThis = this;
	auto Travel = [WeakThis, FinalUrl]()
	{
		if (UMKHGameInstance* GameInstance = WeakThis.Get(); IsValid(GameInstance))
		{
			GameInstance->TravelAfterNetworkRelease(FinalUrl);
		}
	};

	if (UMKHLoadingScreenSubsystem* Loading = ShowLoadingScreenForTravel(LevelType); IsValid(Loading))
	{
		Loading->RunWhenCovered(MoveTemp(Travel));
		return;
	}
	Travel();
}

void UMKHGameInstance::ReleaseWorldNetDriver()
{
	UWorld* World = GetWorld();
	if (bNetDriverReleasePending || !IsValid(World) || !IsValid(World->GetNetDriver()))
	{
		return;
	}

	// Next tick: callers may run inside the engine's net code (network failure handlers, UWorld::Listen).
	bNetDriverReleasePending = true;
	GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UMKHGameInstance::ShutdownReleasedNetDriver,
		TWeakObjectPtr<UWorld>(World)));
}

void UMKHGameInstance::ShutdownReleasedNetDriver(TWeakObjectPtr<UWorld> ReleasedWorld)
{
	bNetDriverReleasePending = false;
	UWorld* World = ReleasedWorld.Get();
	const bool bLevelRunning = IsValid(World) && World == GetWorld() && World->HasBegunPlay() && !World->bIsTearingDown;
	if (!bLevelRunning || !IsValid(World->GetNetDriver()) || !GEngine)
	{
		return;
	}

	UE_LOG(LogMKHSession, Log, TEXT("Travel: closing the connections of the current level before leaving it."));
	GEngine->ShutdownWorldNetDriver(World);
	LastNetDriverReleaseTime = FPlatformTime::Seconds();
}

float UMKHGameInstance::GetNetworkReleaseWait() const
{
	if (bNetDriverReleasePending)
	{
		return MKHGameInstance::NetworkReleaseDelay;
	}
	const double Elapsed = FPlatformTime::Seconds() - LastNetDriverReleaseTime;
	return static_cast<float>(FMath::Max(0.0, MKHGameInstance::NetworkReleaseDelay - Elapsed));
}

void UMKHGameInstance::TravelAfterNetworkRelease(const FString& Url)
{
	const float Wait = GetNetworkReleaseWait();
	if (Wait <= 0.f)
	{
		UGameplayStatics::OpenLevel(this, FName(*Url));
		return;
	}

	const TWeakObjectPtr<UWorld> SourceWorld = GetWorld();
	FTimerHandle TravelHandle;
	GetTimerManager().SetTimer(TravelHandle, FTimerDelegate::CreateWeakLambda(this, [this, Url, SourceWorld]()
	{
		if (GetWorld() != SourceWorld.Get())
		{
			UE_LOG(LogMKHSession, Log, TEXT("Travel to %s dropped: another travel already left the level."), *Url);
			return;
		}
		TravelAfterNetworkRelease(Url);
	}), Wait, false);
}

FString UMKHGameInstance::ResolveLevelName(EMKHLevelType LevelType) const
{
	switch (LevelType)
	{
	case EMKHLevelType::Lobby:
		return LobbyLevelName;
	case EMKHLevelType::Game:
		return GameLevelName;
	default:
		UE_LOG(LogTemp, Warning, TEXT("UMKHGameInstance::ResolveLevelName: Unhandled EMKHLevelType, returning empty level name."));
		return FString();
	}
}

UMKHLobbySubsystem* UMKHGameInstance::GetLobby() const
{
	return GetSubsystem<UMKHLobbySubsystem>();
}

UMKHLoadingScreenSubsystem* UMKHGameInstance::ShowLoadingScreenForTravel(const EMKHLevelType LevelType) const
{
	UMKHLoadingScreenSubsystem* Loading = GetSubsystem<UMKHLoadingScreenSubsystem>();
	if (IsValid(Loading) && !Loading->IsLoadingScreenActive())
	{
		const bool bToLobby = LevelType == EMKHLevelType::Lobby;
		Loading->ShowLoadingScreen(bToLobby ? EMKHLoadingReason::OpeningLobby : EMKHLoadingReason::Loading,
			bToLobby ? EMKHLoadingDestination::Lobby : EMKHLoadingDestination::Game);
	}
	return Loading;
}
