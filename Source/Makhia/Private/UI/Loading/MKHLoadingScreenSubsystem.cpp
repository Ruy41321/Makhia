// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Loading/MKHLoadingScreenSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameInstance/MKHGameInstance.h"
#include "GameMode/MKHGameState.h"
#include "Lobby/MKHLobbyGameState.h"
#include "Lobby/MKHLobbyLineupSubsystem.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"
#include "MKHLogChannels.h"
#include "MoviePlayer.h"
#include "UI/Loading/MKHLoadingScreenSettings.h"
#include "UI/Loading/SMKHLoadingScreen.h"
#include "UI/Text/MKHTextStyle.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "MKHLoadingScreen"

namespace MKHLoadingScreen
{
	/** Viewport Z order of the screen: above every lobby and game widget. */
	constexpr int32 ViewportZOrder = 1000;

	/** Longest frame step used by the fades (the first frame after a load can be very long). */
	constexpr float MaxFadeStep = 0.1f;
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (IsRunningDedicatedServer())
	{
		return;
	}

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddLambda([this](const FWorldContext& Context, const FString& MapName)
	{
		if (Context.OwningGameInstance == GetGameInstance())
		{
			HandlePreLoadMap(MapName);
		}
	});
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMKHLoadingScreenSubsystem::HandlePostLoadMap);
	SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &UMKHLoadingScreenSubsystem::HandleSeamlessTravelStart);
	if (IsValid(GEngine))
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UMKHLoadingScreenSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UMKHLoadingScreenSubsystem::HandleTravelFailure);
	}
	if (IsMoviePlayerEnabled())
	{
		PrepareLoadingScreenHandle = GetMoviePlayer()->OnPrepareLoadingScreen().AddUObject(this, &UMKHLoadingScreenSubsystem::HandlePrepareLoadingScreen);
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMKHLoadingScreenSubsystem::Tick));
}

void UMKHLoadingScreenSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
	if (IsValid(GEngine))
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	if (PrepareLoadingScreenHandle.IsValid() && IsMoviePlayerEnabled())
	{
		GetMoviePlayer()->OnPrepareLoadingScreen().Remove(PrepareLoadingScreenHandle);
	}

	RemoveOverlayFromViewport();
	OverlayWidget.Reset();
	PendingActions.Reset();

	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHLoadingScreenSubsystem* UMKHLoadingScreenSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHLoadingScreenSubsystem>() : nullptr;
}

void UMKHLoadingScreenSubsystem::ShowLoadingScreen(const EMKHLoadingReason InReason, const EMKHLoadingDestination InDestination)
{
	if (!CanShowLoadingScreen())
	{
		return;
	}

	const bool bNewTravel = !bActive || bFadingOut || InDestination != Destination;
	UE_LOG(LogMKHLoading, Log, TEXT("Loading screen %s: %s -> %s."), bActive ? TEXT("updated") : TEXT("shown"),
		*UEnum::GetValueAsString(InReason), *UEnum::GetValueAsString(InDestination));

	if (!bActive)
	{
		bActive = true;
		Opacity = 0.f;
		LoadStyleAssets();
	}

	Reason = InReason;
	Destination = InDestination;
	bFadingOut = false;
	if (bNewTravel)
	{
		bMapLoadedSinceShow = false;
		ShowTime = FPlatformTime::Seconds();
		ReadySince = -1.0;
		WaitSince = -1.0;
	}

	AddOverlayToViewport();
	RefreshStatusText();
}

void UMKHLoadingScreenSubsystem::ShowMatchLoadingScreen()
{
	const UMKHLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UMKHLobbySubsystem>();
	const bool bMatchmaking = IsValid(Lobby) && Lobby->GetIsMatchmaking();
	ShowLoadingScreen(bMatchmaking ? EMKHLoadingReason::MatchFound : EMKHLoadingReason::MatchStarting, EMKHLoadingDestination::Game);
}

void UMKHLoadingScreenSubsystem::HideLoadingScreen()
{
	FlushPendingActions();
	BeginFadeOut();
}

void UMKHLoadingScreenSubsystem::RunWhenCovered(TFunction<void()>&& Action)
{
	if (!bActive || bFadingOut || Opacity >= 1.f || !CanShowLoadingScreen())
	{
		Action();
		return;
	}

	PendingActions.Add(MoveTemp(Action));
}

void UMKHLoadingScreenSubsystem::ReportTravelError(const EMKHTravelError Error)
{
	ReportTravelErrorWithDetail(Error, FString());
}

void UMKHLoadingScreenSubsystem::ReportTravelErrorWithDetail(const EMKHTravelError Error, const FString& Detail)
{
	if (Error == EMKHTravelError::None)
	{
		return;
	}

	if (IsMatchmakingRetry(Error))
	{
		UE_LOG(LogMKHLoading, Log, TEXT("Join failed during matchmaking (%s): the search goes on, no error shown."), Detail.IsEmpty() ? TEXT("-") : *Detail);
		if (UMKHLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UMKHLobbySubsystem>(); IsValid(Lobby))
		{
			Lobby->MarkJoinTargetUnreachable();
		}
		if (bActive)
		{
			HideLoadingScreen();
		}
		return;
	}

	// The first error is the cause; the ones that follow (a closing connection repeats its failure) are consequences.
	if (PendingError.IsSet())
	{
		UE_LOG(LogMKHLoading, Verbose, TEXT("Travel error %s after %s: %s"), *UMKHTravelErrorLibrary::GetErrorCode(Error),
			*UMKHTravelErrorLibrary::GetErrorCode(PendingError.Error), Detail.IsEmpty() ? TEXT("-") : *Detail);
	}
	else
	{
		UE_LOG(LogMKHLoading, Warning, TEXT("Travel error %s (loading screen %s, %s -> %s): %s"),
			*UMKHTravelErrorLibrary::GetErrorCode(Error), bActive ? TEXT("up") : TEXT("down"),
			*UEnum::GetValueAsString(Reason), *UEnum::GetValueAsString(Destination), Detail.IsEmpty() ? TEXT("-") : *Detail);
		PendingError.Error = Error;
		PendingError.Detail = Detail;
	}

	// The host could not start the match: the clients waiting under their screen for the countdown give up too.
	if (Error == EMKHTravelError::LobbyStartFailed)
	{
		const UWorld* World = GetGameInstance()->GetWorld();
		if (AMKHLobbyGameState* LobbyGameState = IsValid(World) ? World->GetGameState<AMKHLobbyGameState>() : nullptr; IsValid(LobbyGameState))
		{
			LobbyGameState->SetMatchCountdown(-1.f);
		}
	}

	if (bActive)
	{
		const bool bStillHome = !bMapLoadedSinceShow && IsValid(GetMKHGameInstance()) && GetMKHGameInstance()->IsInLobbyLevel();
		const bool bLocalFailure = Error == EMKHTravelError::JoinFailed || Error == EMKHTravelError::LobbyStartFailed;
		if (bLocalFailure && bStillHome)
		{
			HideLoadingScreen();
		}
		else
		{
			ShowLoadingScreen(EMKHLoadingReason::ReturningToLobby, EMKHLoadingDestination::Lobby);
		}
	}

	OnTravelErrorReported.Broadcast();
}

FMKHTravelErrorInfo UMKHLoadingScreenSubsystem::ConsumePendingError()
{
	FMKHTravelErrorInfo Error = PendingError;
	PendingError = FMKHTravelErrorInfo();
	return Error;
}

// ============================================================
// Protected / Internal Logic - Display
// ============================================================

bool UMKHLoadingScreenSubsystem::CanShowLoadingScreen() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (!IsValid(GameInstance) || !IsValid(GameInstance->GetGameViewportClient()))
	{
		return false;
	}

	// The boot overlay covers the start-up travels.
	const UMKHGameInstance* MKHGameInstance = GetMKHGameInstance();
	return !IsValid(MKHGameInstance) || !MKHGameInstance->IsBootInProgress();
}

void UMKHLoadingScreenSubsystem::LoadStyleAssets()
{
	if (IsValid(TitleStyle))
	{
		return;
	}

	const UMKHLoadingScreenSettings* Settings = UMKHLoadingScreenSettings::Get();
	BackgroundMaterial = Settings->BackgroundMaterial.LoadSynchronous();
	TitleStyle = Settings->TitleStyle.LoadSynchronous();
	StatusStyle = Settings->StatusStyle.LoadSynchronous();

	if (IsValid(BackgroundMaterial))
	{
		BackgroundBrush.SetResourceObject(BackgroundMaterial);
		BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
		BackgroundBrush.ImageSize = FVector2D(64.f, 64.f);
	}
	if (!IsValid(TitleStyle) || !IsValid(StatusStyle) || !IsValid(BackgroundMaterial))
	{
		UE_LOG(LogMKHLoading, Warning, TEXT("Loading screen: missing style assets in Project Settings -> Makhia Loading Screen."));
	}
}

TSharedRef<SMKHLoadingScreen> UMKHLoadingScreenSubsystem::CreateScreenWidget() const
{
	const UMKHLoadingScreenSettings* Settings = UMKHLoadingScreenSettings::Get();

	SMKHLoadingScreen::FStyle Style;
	Style.BackgroundBrush = IsValid(BackgroundMaterial) ? &BackgroundBrush : nullptr;
	Style.TitleFont = IsValid(TitleStyle) ? TitleStyle->Font : FCoreStyle::GetDefaultFontStyle("Regular", 48);
	Style.StatusFont = IsValid(StatusStyle) ? StatusStyle->Font : FCoreStyle::GetDefaultFontStyle("Regular", 18);
	Style.TitleColor = Settings->TitleColor;
	Style.DividerColor = Settings->DividerColor;
	Style.StatusColor = Settings->StatusColor;
	Style.TitleTransform = IsValid(TitleStyle) ? TitleStyle->TextTransform : ETextTransformPolicy::None;
	Style.StatusTransform = IsValid(StatusStyle) ? StatusStyle->TextTransform : ETextTransformPolicy::None;

	return SNew(SMKHLoadingScreen, Style).StatusText(GetReasonText(Reason));
}

void UMKHLoadingScreenSubsystem::AddOverlayToViewport()
{
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (bOverlayInViewport || !IsValid(Viewport))
	{
		return;
	}

	if (!OverlayWidget.IsValid())
	{
		OverlayWidget = CreateScreenWidget();
	}

	OverlayWidget->SetVisibility(EVisibility::Visible);
	OverlayWidget->SetRenderOpacity(Opacity);
	Viewport->AddViewportWidgetContent(OverlayWidget.ToSharedRef(), MKHLoadingScreen::ViewportZOrder);
	bOverlayInViewport = true;
}

void UMKHLoadingScreenSubsystem::RemoveOverlayFromViewport()
{
	const UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* Viewport = IsValid(GameInstance) ? GameInstance->GetGameViewportClient() : nullptr;
	if (bOverlayInViewport && OverlayWidget.IsValid() && IsValid(Viewport))
	{
		Viewport->RemoveViewportWidgetContent(OverlayWidget.ToSharedRef());
	}
	bOverlayInViewport = false;
}

void UMKHLoadingScreenSubsystem::RefreshStatusText() const
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->SetStatusText(GetReasonText(Reason));
	}
}

FText UMKHLoadingScreenSubsystem::GetReasonText(const EMKHLoadingReason InReason)
{
	switch (InReason)
	{
	case EMKHLoadingReason::MatchFound:
		return LOCTEXT("MatchFound", "Opponent found! Entering the arena...");
	case EMKHLoadingReason::MatchStarting:
		return LOCTEXT("MatchStarting", "The duel is about to begin!");
	case EMKHLoadingReason::OfflineMatch:
		return LOCTEXT("OfflineMatch", "Preparing the arena...");
	case EMKHLoadingReason::ReturningToLobby:
		return LOCTEXT("ReturningToLobby", "Returning to the lobby...");
	case EMKHLoadingReason::OpeningLobby:
		return LOCTEXT("OpeningLobby", "Opening your lobby...");
	default:
		return LOCTEXT("Loading", "Loading...");
	}
}

// ============================================================
// Protected / Internal Logic - Progress
// ============================================================

bool UMKHLoadingScreenSubsystem::Tick(const float DeltaTime)
{
	UpdateLobbyCountdown();
	if (!bActive)
	{
		return true;
	}

	UpdateFade(FMath::Min(DeltaTime, MKHLoadingScreen::MaxFadeStep));
	if (!bActive || bFadingOut)
	{
		return true;
	}

	if (Opacity >= 1.f)
	{
		FlushPendingActions();
	}
	CheckTimeouts();
	UpdateArrival();
	return true;
}

void UMKHLoadingScreenSubsystem::UpdateFade(const float DeltaTime)
{
	const UMKHLoadingScreenSettings* Settings = UMKHLoadingScreenSettings::Get();
	const float Duration = bFadingOut ? Settings->FadeOutDuration : Settings->FadeInDuration;
	const float Step = Duration > 0.f ? DeltaTime / Duration : 1.f;
	Opacity = FMath::Clamp(Opacity + (bFadingOut ? -Step : Step), 0.f, 1.f);

	if (OverlayWidget.IsValid())
	{
		OverlayWidget->SetRenderOpacity(Opacity);
	}
	if (bFadingOut && Opacity <= 0.f)
	{
		FinishHide();
	}
}

void UMKHLoadingScreenSubsystem::FlushPendingActions()
{
	// An action may travel and queue another one: run a snapshot.
	TArray<TFunction<void()>> Actions = MoveTemp(PendingActions);
	PendingActions.Reset();
	for (TFunction<void()>& Action : Actions)
	{
		Action();
	}
}

void UMKHLoadingScreenSubsystem::UpdateArrival()
{
	if (!PendingActions.IsEmpty() || !IsDestinationReached())
	{
		ReadySince = -1.0;
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (ReadySince < 0.0)
	{
		ReadySince = Now;
	}

	const UMKHLoadingScreenSettings* Settings = UMKHLoadingScreenSettings::Get();
	if (Now - ReadySince >= Settings->HoldAfterReady && Now - ShowTime >= Settings->MinimumDisplayTime)
	{
		BeginFadeOut();
	}
}

bool UMKHLoadingScreenSubsystem::IsDestinationReached() const
{
	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	if (!bMapLoadedSinceShow || !IsWorldSettled() || !IsValid(GameInstance))
	{
		return false;
	}

	UWorld* World = GameInstance->GetWorld();
	const bool bInLobby = GameInstance->IsInLobbyLevel();
	if ((Destination == EMKHLoadingDestination::Game && bInLobby) || (Destination == EMKHLoadingDestination::Lobby && !bInLobby))
	{
		return false;
	}

	const APlayerController* PlayerController = GameInstance->GetFirstLocalPlayerController(World);
	if (!IsValid(PlayerController))
	{
		return false;
	}

	if (bInLobby)
	{
		const UMKHLobbyLineupSubsystem* Lineup = UMKHLobbyLineupSubsystem::Get(World);
		return !IsValid(Lineup) || !Lineup->HasPlatforms() || Lineup->IsLineupReady();
	}

	// An online duel starts once the opponent arrived too (seamless travel brings the clients a bit later).
	const AGameStateBase* GameState = World->GetGameState();
	const bool bWaitsForOpponent = World->GetNetMode() != NM_Standalone && !GameInstance->IsInOfflineMatch();
	const bool bOpponentHere = !bWaitsForOpponent || (IsValid(GameState) && GameState->PlayerArray.Num() >= 2);
	return IsValid(PlayerController->GetPawn()) && bOpponentHere;
}

bool UMKHLoadingScreenSubsystem::IsWorldSettled() const
{
	const UWorld* World = GetGameInstance()->GetWorld();
	return IsValid(World) && World->HasBegunPlay() && !World->IsInSeamlessTravel();
}

void UMKHLoadingScreenSubsystem::CheckTimeouts()
{
	UMKHGameInstance* GameInstance = GetMKHGameInstance();
	const UMKHLoadingScreenSettings* Settings = UMKHLoadingScreenSettings::Get();
	const double Now = FPlatformTime::Seconds();

	if (Now - ShowTime > Settings->TravelTimeout)
	{
		ReportTravelErrorWithDetail(EMKHTravelError::TravelTimeout, FString::Printf(TEXT("screen up for %.0f s"), Now - ShowTime));
		ShowTime = Now;
		if (IsValid(GameInstance))
		{
			GameInstance->LeaveToNewLobby();
		}
		return;
	}

	// A level loaded on the way to a match (the opponent's lobby, the arena without the opponent) waits so long only.
	const bool bWaiting = Destination == EMKHLoadingDestination::Game && bMapLoadedSinceShow && IsWorldSettled() && !IsDestinationReached();
	if (!bWaiting)
	{
		WaitSince = -1.0;
		return;
	}

	if (WaitSince < 0.0)
	{
		WaitSince = Now;
	}
	else if (Now - WaitSince > Settings->MatchStartTimeout)
	{
		ReportTravelErrorWithDetail(EMKHTravelError::MatchStartTimeout, FString::Printf(TEXT("waited %.0f s in %s"),
			Now - WaitSince, IsValid(GameInstance) ? *GetNameSafe(GameInstance->GetWorld()) : TEXT("?")));
		if (IsValid(GameInstance))
		{
			GameInstance->LeaveToNewLobby();
		}
	}
}

void UMKHLoadingScreenSubsystem::UpdateLobbyCountdown()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	const AMKHLobbyGameState* LobbyGameState = IsValid(World) ? World->GetGameState<AMKHLobbyGameState>() : nullptr;
	if (!IsValid(LobbyGameState) || LobbyGameState->HasAuthority())
	{
		bShownForCountdown = false;
		return;
	}

	// The host keeps the countdown at zero when the match starts and clears it when somebody cancelled.
	const float Remaining = LobbyGameState->GetMatchCountdownRemaining();
	if (Remaining == 0.f && !bShownForCountdown)
	{
		bShownForCountdown = true;
		ShowLoadingScreen(EMKHLoadingReason::MatchStarting, EMKHLoadingDestination::Game);
	}
	else if (Remaining < 0.f && bShownForCountdown)
	{
		bShownForCountdown = false;
		if (bActive && Reason == EMKHLoadingReason::MatchStarting && !bMapLoadedSinceShow)
		{
			UE_LOG(LogMKHLoading, Log, TEXT("Lobby countdown cancelled by the host: loading screen hidden."));
			HideLoadingScreen();
		}
	}
}

void UMKHLoadingScreenSubsystem::BeginFadeOut()
{
	if (!bActive || bFadingOut)
	{
		return;
	}

	UE_LOG(LogMKHLoading, Log, TEXT("Loading screen fading out after %.2f s (%s)."), FPlatformTime::Seconds() - ShowTime,
		*UEnum::GetValueAsString(Destination));
	bFadingOut = true;
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->SetVisibility(EVisibility::HitTestInvisible);
	}
}

void UMKHLoadingScreenSubsystem::FinishHide()
{
	RemoveOverlayFromViewport();
	bActive = false;
	bFadingOut = false;
	FlushPendingActions();
	OnLoadingScreenHidden.Broadcast();
}

UMKHGameInstance* UMKHLoadingScreenSubsystem::GetMKHGameInstance() const
{
	return Cast<UMKHGameInstance>(GetGameInstance());
}

// ============================================================
// Protected / Internal Logic - Engine Hooks
// ============================================================

void UMKHLoadingScreenSubsystem::HandlePrepareLoadingScreen()
{
	// Runs inside the map load, before HandlePreLoadMap may have run.
	if (!bActive)
	{
		ShowLoadingScreen(EMKHLoadingReason::Loading, EMKHLoadingDestination::Any);
	}
	if (!bActive)
	{
		return;
	}

	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = false;
	Attributes.bWaitForManualStop = false;
	Attributes.MinimumLoadingScreenDisplayTime = -1.f;
	Attributes.WidgetLoadingScreen = CreateScreenWidget();
	GetMoviePlayer()->SetupLoadingScreen(Attributes);
	UE_LOG(LogMKHLoading, Log, TEXT("Blocking map load: loading screen handed to the MoviePlayer (%s)."), *UEnum::GetValueAsString(Reason));
}

void UMKHLoadingScreenSubsystem::HandlePreLoadMap(const FString& MapName)
{
	if (!bActive && CanShowLoadingScreen())
	{
		UE_LOG(LogMKHLoading, Log, TEXT("Map load of %s without a loading screen: showing the default one."), *MapName);
		ShowLoadingScreen(EMKHLoadingReason::Loading, EMKHLoadingDestination::Any);
	}
}

void UMKHLoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!IsValid(LoadedWorld) || !IsOwnWorld(LoadedWorld))
	{
		return;
	}

	bMapLoadedSinceShow = true;
	ReadySince = -1.0;
	WaitSince = -1.0;
	if (bActive)
	{
		AddOverlayToViewport();
	}
}

void UMKHLoadingScreenSubsystem::HandleSeamlessTravelStart(UWorld* CurrentWorld, const FString& MapName)
{
	if (IsOwnWorld(CurrentWorld))
	{
		HandlePreLoadMap(MapName);
	}
}

void UMKHLoadingScreenSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!IsOwnWorld(World))
	{
		return;
	}

	// The opponent leaving once the match is over is the normal end of a match.
	if (IsMatchOver())
	{
		UE_LOG(LogMKHLoading, Log, TEXT("Network failure %s after the match ended: not an error."), ENetworkFailure::ToString(FailureType));
		return;
	}

	// The failure repeats every tick while the connection closes: once recorded and on the way back, nothing to add.
	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	if (PendingError.IsSet() && IsValid(GameInstance) && GameInstance->IsReturningToLobby())
	{
		UE_LOG(LogMKHLoading, Verbose, TEXT("Network failure %s repeated during the return to the lobby."), ENetworkFailure::ToString(FailureType));
		return;
	}

	// The host closing its lobby is a normal way to leave a room: a notice, not an error.
	if (IsLobbyHostLeaving(FailureType))
	{
		UE_LOG(LogMKHLoading, Log, TEXT("Network failure %s in a lobby: the host left (%s)."), ENetworkFailure::ToString(FailureType), *ErrorString);
		PostHostLeftNotice();
		return;
	}

	ReportTravelErrorWithDetail(MapNetworkFailure(FailureType),
		FString::Printf(TEXT("%s: %s (map %s)"), ENetworkFailure::ToString(FailureType), *ErrorString, *GetNameSafe(World)));
}

void UMKHLoadingScreenSubsystem::HandleTravelFailure(UWorld* World, const ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!IsOwnWorld(World))
	{
		return;
	}

	const bool bLoadFailure = FailureType == ETravelFailure::NoLevel || FailureType == ETravelFailure::LoadMapFailure
		|| FailureType == ETravelFailure::PackageMissing;
	ReportTravelErrorWithDetail(bLoadFailure ? EMKHTravelError::LoadFailed : EMKHTravelError::TravelFailed,
		FString::Printf(TEXT("%s: %s (map %s)"), ETravelFailure::ToString(FailureType), *ErrorString, *GetNameSafe(World)));
}

EMKHTravelError UMKHLoadingScreenSubsystem::MapNetworkFailure(const ENetworkFailure::Type FailureType)
{
	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::FailureReceived:
		return EMKHTravelError::ConnectionLost;
	case ENetworkFailure::ConnectionTimeout:
		return EMKHTravelError::ConnectionTimeout;
	case ENetworkFailure::PendingConnectionFailure:
		return EMKHTravelError::ConnectFailed;
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		return EMKHTravelError::VersionMismatch;
	case ENetworkFailure::NetDriverListenFailure:
		return EMKHTravelError::ListenFailed;
	default:
		return EMKHTravelError::NetworkFailure;
	}
}

bool UMKHLoadingScreenSubsystem::IsOwnWorld(const UWorld* World) const
{
	// A failure without a world can only be told apart outside Play In Editor.
	return IsValid(World) ? World->GetGameInstance() == GetGameInstance() : !GIsEditor;
}

bool UMKHLoadingScreenSubsystem::IsLobbyHostLeaving(const ENetworkFailure::Type FailureType) const
{
	const bool bConnectionClosed = FailureType == ENetworkFailure::ConnectionLost || FailureType == ENetworkFailure::FailureReceived;
	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	const bool bOnTheWayToMatch = bActive && Destination == EMKHLoadingDestination::Game;
	return bConnectionClosed && IsValid(GameInstance) && GameInstance->IsInLobbyLevel() && !bOnTheWayToMatch;
}

void UMKHLoadingScreenSubsystem::PostHostLeftNotice() const
{
	// The failure repeats while the connection closes: the first one, with the host still known, wins.
	UMKHLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UMKHLobbySubsystem>();
	if (!IsValid(Lobby) || Lobby->HasPendingLobbyNotice())
	{
		return;
	}

	const UMKHGameInstance* GameInstance = GetMKHGameInstance();
	UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	const APlayerController* LocalController = IsValid(World) ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	const APlayerState* LocalPlayerState = IsValid(LocalController) ? LocalController->PlayerState.Get() : nullptr;

	// The host joined its own lobby first, so it has the lowest PlayerId.
	const APlayerState* Host = nullptr;
	if (IsValid(GameState))
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (IsValid(PlayerState) && PlayerState != LocalPlayerState && (!Host || PlayerState->GetPlayerId() < Host->GetPlayerId()))
			{
				Host = PlayerState;
			}
		}
	}

	// HandleDisconnect sends this machine back to its own lobby right after: the notice is for that lobby.
	Lobby->PostPlayerLeftNotice(Host, true);
}

bool UMKHLoadingScreenSubsystem::IsMatchmakingRetry(const EMKHTravelError Error) const
{
	const UMKHLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UMKHLobbySubsystem>();
	return Error == EMKHTravelError::JoinFailed && IsValid(Lobby) && Lobby->GetIsMatchmaking();
}

bool UMKHLoadingScreenSubsystem::IsMatchOver() const
{
	const UWorld* World = GetGameInstance()->GetWorld();
	const AMKHGameState* GameState = IsValid(World) ? World->GetGameState<AMKHGameState>() : nullptr;
	return IsValid(GameState) && GameState->GetMatchPhase() == EMKHMatchPhase::MatchOver;
}

#undef LOCTEXT_NAMESPACE
