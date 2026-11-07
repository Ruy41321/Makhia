// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/MKHLobbyHUD.h"

#include "GameFramework/PlayerController.h"
#include "GameInstance/MKHGameInstance.h"
#include "MKHLogChannels.h"
#include "Online/Auth/MKHAuthSubsystem.h"
#include "UI/HUD/GameMenu/MKHGameMenuWidget.h"
#include "UI/HUD/Settings/MKHSettingsScreenWidget.h"
#include "UI/Lobby/MKHBootOverlayWidget.h"
#include "UI/Loading/MKHLoadingScreenSubsystem.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "UI/Lobby/MKHLobbyNoticeWidget.h"
#include "UI/Lobby/MKHLoginPanelWidget.h"
#include "UI/Lobby/MKHTravelErrorWidget.h"

// ============================================================
// Lifecycle
// ============================================================

void AMKHLobbyHUD::BeginPlay()
{
	Super::BeginPlay();

	CreateBootOverlay();
	BindLoadingScreen();
	ShowPendingMessages();
}

void AMKHLobbyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(this); IsValid(Loading))
	{
		Loading->OnLoadingScreenHidden.Remove(LoadingHiddenHandle);
		Loading->OnTravelErrorReported.Remove(TravelErrorHandle);
	}
	if (UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this); IsValid(Lobby))
	{
		Lobby->OnLobbyNoticePosted.Remove(LobbyNoticeHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// ============================================================
// Public Interface
// ============================================================

AMKHLobbyHUD* AMKHLobbyHUD::GetForPlayer(const APlayerController* PlayerController)
{
	return IsValid(PlayerController) ? PlayerController->GetHUD<AMKHLobbyHUD>() : nullptr;
}

void AMKHLobbyHUD::ToggleGameMenu()
{
	if (IsTravelErrorOpen())
	{
		CloseTravelError();
		return;
	}

	// The settings screen sits on top of the menu, so the action dismisses it first.
	if (IsValid(SettingsScreenWidget))
	{
		SettingsScreenWidget->RequestClose();
		return;
	}

	if (IsLoginPanelOpen())
	{
		CloseLoginPanel();
		return;
	}

	CreateGameMenuWidget();
	if (!IsValid(GameMenuWidget))
	{
		return;
	}

	if (GameMenuWidget->IsModalWindowOpen())
	{
		GameMenuWidget->CloseModalWindows();
	}
	else if (GameMenuWidget->IsOpen())
	{
		GameMenuWidget->CloseGameMenu();
	}
	else
	{
		GameMenuWidget->OpenGameMenu();
	}
}

void AMKHLobbyHUD::ToggleSettingsWidget()
{
	if (IsValid(SettingsScreenWidget))
	{
		DestroySettingsWidget();
	}
	else
	{
		CreateSettingsWidget();
	}
}

void AMKHLobbyHUD::OpenLoginPanel()
{
	if (IsLoginPanelOpen())
	{
		return;
	}

	if (UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this); IsValid(Auth) && !Auth->HasSignInChoice())
	{
		Auth->LoginWithPlatformAccount();
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(PC) || !IsValid(LoginPanelWidgetClass))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("AMKHLobbyHUD: no owning controller or LoginPanelWidgetClass is not set."));
		return;
	}

	LoginPanelWidget = CreateWidget<UMKHLoginPanelWidget>(PC, LoginPanelWidgetClass);
	if (!IsValid(LoginPanelWidget))
	{
		return;
	}

	LoginPanelWidget->OnCloseRequested.AddUObject(this, &AMKHLobbyHUD::CloseLoginPanel);
	LoginPanelWidget->AddToViewport(LoginPanelZOrder);
}

void AMKHLobbyHUD::CloseLoginPanel()
{
	if (!IsValid(LoginPanelWidget))
	{
		return;
	}

	// The panel may be broadcasting OnCloseRequested right now: no unbind (it dies with the panel), just forget it.
	UMKHLoginPanelWidget* Panel = LoginPanelWidget;
	LoginPanelWidget = nullptr;
	Panel->RemoveFromParent();
}

bool AMKHLobbyHUD::IsLoginPanelOpen() const
{
	return IsValid(LoginPanelWidget);
}

void AMKHLobbyHUD::ShowPendingTravelError()
{
	UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(this);
	APlayerController* PC = GetOwningPlayerController();
	if (IsTravelErrorOpen() || !IsValid(Loading) || !Loading->HasPendingError() || Loading->IsLoadingScreenActive()
		|| !IsValid(PC) || !PC->IsLocalController())
	{
		return;
	}

	if (!IsValid(TravelErrorWidgetClass))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("AMKHLobbyHUD: TravelErrorWidgetClass is not set, the travel error is only logged."));
		Loading->ConsumePendingError();
		return;
	}

	TravelErrorWidget = CreateWidget<UMKHTravelErrorWidget>(PC, TravelErrorWidgetClass);
	if (!IsValid(TravelErrorWidget))
	{
		return;
	}

	TravelErrorWidget->SetError(Loading->ConsumePendingError());
	TravelErrorWidget->OnCloseRequested.AddUObject(this, &AMKHLobbyHUD::CloseTravelError);
	TravelErrorWidget->AddToViewport(TravelErrorZOrder);
}

void AMKHLobbyHUD::CloseTravelError()
{
	if (!IsValid(TravelErrorWidget))
	{
		return;
	}

	// The dialog may be broadcasting OnCloseRequested right now: just forget it.
	UMKHTravelErrorWidget* Dialog = TravelErrorWidget;
	TravelErrorWidget = nullptr;
	Dialog->RemoveFromParent();
}

bool AMKHLobbyHUD::IsTravelErrorOpen() const
{
	return IsValid(TravelErrorWidget);
}

void AMKHLobbyHUD::ShowPendingLobbyNotice()
{
	UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this);
	const UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(this);
	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(Lobby) || !Lobby->HasPendingLobbyNotice() || (IsValid(Loading) && Loading->IsLoadingScreenActive())
		|| !IsValid(PC) || !PC->IsLocalController())
	{
		return;
	}

	if (!IsValid(LobbyNoticeWidgetClass))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("AMKHLobbyHUD: LobbyNoticeWidgetClass is not set, the lobby notice is only logged."));
		Lobby->ConsumeLobbyNotice();
		return;
	}

	if (!IsValid(LobbyNoticeWidget))
	{
		LobbyNoticeWidget = CreateWidget<UMKHLobbyNoticeWidget>(PC, LobbyNoticeWidgetClass);
	}
	if (!IsValid(LobbyNoticeWidget))
	{
		return;
	}

	// The toast removes itself when it has faded out: add it back for every new notice.
	LobbyNoticeWidget->ShowNotice(Lobby->ConsumeLobbyNotice());
	if (!LobbyNoticeWidget->IsInViewport())
	{
		LobbyNoticeWidget->AddToViewport(LobbyNoticeZOrder);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AMKHLobbyHUD::CreateBootOverlay()
{
	const UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this);
	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(GameInstance) || !GameInstance->IsBootInProgress() || !IsValid(PC) || !PC->IsLocalController())
	{
		return;
	}

	if (!IsValid(BootOverlayWidgetClass))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("AMKHLobbyHUD: BootOverlayWidgetClass is not set, the lobby shows during the start-up login."));
		return;
	}

	// The overlay removes itself once the boot is over.
	if (UMKHBootOverlayWidget* BootOverlay = CreateWidget<UMKHBootOverlayWidget>(PC, BootOverlayWidgetClass); IsValid(BootOverlay))
	{
		BootOverlay->AddToViewport(BootOverlayZOrder);
		UE_LOG(LogMKHUI, Log, TEXT("AMKHLobbyHUD: boot overlay shown (%s)."), *UEnum::GetValueAsString(GameInstance->GetBootPhase()));
	}
}

void AMKHLobbyHUD::BindLoadingScreen()
{
	UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(this);
	if (!IsValid(Loading))
	{
		return;
	}

	LoadingHiddenHandle = Loading->OnLoadingScreenHidden.AddUObject(this, &AMKHLobbyHUD::ShowPendingMessages);
	TravelErrorHandle = Loading->OnTravelErrorReported.AddUObject(this, &AMKHLobbyHUD::ShowPendingTravelError);
	if (UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this); IsValid(Lobby))
	{
		LobbyNoticeHandle = Lobby->OnLobbyNoticePosted.AddUObject(this, &AMKHLobbyHUD::ShowPendingLobbyNotice);
	}
}

void AMKHLobbyHUD::ShowPendingMessages()
{
	ShowPendingTravelError();
	ShowPendingLobbyNotice();
}

void AMKHLobbyHUD::CreateGameMenuWidget()
{
	if (IsValid(GameMenuWidget))
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(PC) || !IsValid(GameMenuWidgetClass))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("AMKHLobbyHUD: no owning controller or GameMenuWidgetClass is not set."));
		return;
	}

	GameMenuWidget = CreateWidget<UMKHGameMenuWidget>(PC, GameMenuWidgetClass);
	if (IsValid(GameMenuWidget))
	{
		GameMenuWidget->AddToViewport();
		GameMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AMKHLobbyHUD::CreateSettingsWidget()
{
	if (IsValid(SettingsScreenWidget))
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(PC) || !IsValid(SettingsScreenWidgetClass))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("AMKHLobbyHUD: no owning controller or SettingsScreenWidgetClass is not set."));
		return;
	}

	SettingsScreenWidget = CreateWidget<UMKHSettingsScreenWidget>(PC, SettingsScreenWidgetClass);
	if (!IsValid(SettingsScreenWidget))
	{
		return;
	}

	SettingsScreenWidget->OnSettingsClosed.AddUObject(this, &AMKHLobbyHUD::OnSettingsClosed);
	SettingsScreenWidget->AddToViewport();
}

void AMKHLobbyHUD::DestroySettingsWidget()
{
	if (!IsValid(SettingsScreenWidget))
	{
		return;
	}

	SettingsScreenWidget->OnSettingsClosed.RemoveAll(this);
	SettingsScreenWidget->RemoveFromParent();
	SettingsScreenWidget = nullptr;
}

void AMKHLobbyHUD::OnSettingsClosed()
{
	DestroySettingsWidget();

	// Same behaviour as in a match: leaving the settings also leaves the menu underneath.
	if (IsValid(GameMenuWidget) && GameMenuWidget->IsOpen())
	{
		GameMenuWidget->CloseGameMenu();
	}
}
