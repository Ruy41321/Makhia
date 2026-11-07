// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/MKHGameMenuWidget.h"

#include "MKHLogChannels.h"
#include "Player/PlayerController/MKHPlayerController.h"
#include "UI/HUD/GameMenu/MKHCreditsWidget.h"
#include "UI/HUD/GameMenu/MKHLegalWidget.h"
#include "UI/HUD/MainHUD.h"
#include "UI/HUD/MKHLobbyHUD.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHGameMenuWidget::OpenGameMenu()
{
	SetVisibility(ESlateVisibility::Visible);
	bIsOpen = true;
	FOnOpenGameMenuDelegate.Broadcast();
	OnOpenGameMenu();
}

void UMKHGameMenuWidget::CloseGameMenu()
{
	CloseModalWindows();

	SetVisibility(ESlateVisibility::Collapsed);
	bIsOpen = false;
	FOnCloseGameMenuDelegate.Broadcast();
}

void UMKHGameMenuWidget::DestroyGameMenu()
{
	bIsOpen = false;
	FOnDestroyGameMenuDelegate.Broadcast();
	RemoveFromParent();
}

void UMKHGameMenuWidget::UnbindAllEventsFromDelegates()
{
	FOnOpenGameMenuDelegate.Clear();
	FOnCloseGameMenuDelegate.Clear();
	FOnDestroyGameMenuDelegate.Clear();
}

void UMKHGameMenuWidget::OpenCredits()
{
	if (!IsValid(CreditsPopup))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHGameMenuWidget::OpenCredits: %s has no CreditsPopup."), *GetClass()->GetName());
		return;
	}

	CreditsPopup->OpenCredits();
}

void UMKHGameMenuWidget::CloseCredits()
{
	if (IsValid(CreditsPopup))
	{
		CreditsPopup->CloseCredits();
	}
}

void UMKHGameMenuWidget::ToggleSettings()
{
	const APlayerController* PC = GetOwningPlayer();
	AHUD* HUD = IsValid(PC) ? PC->GetHUD() : nullptr;

	if (AMainHUD* MainHUD = Cast<AMainHUD>(HUD); IsValid(MainHUD))
	{
		MainHUD->ToggleSettingsWidget();
	}
	else if (AMKHLobbyHUD* LobbyHUD = Cast<AMKHLobbyHUD>(HUD); IsValid(LobbyHUD))
	{
		LobbyHUD->ToggleSettingsWidget();
	}
	else
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHGameMenuWidget::ToggleSettings: the HUD of this level cannot host the settings screen."));
	}
}

bool UMKHGameMenuWidget::IsCreditsOpen() const
{
	return IsValid(CreditsPopup) && CreditsPopup->IsOpen();
}

void UMKHGameMenuWidget::OpenLegal(const EMKHLegalDocument Document)
{
	if (!IsValid(LegalPopup))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHGameMenuWidget::OpenLegal: %s has no LegalPopup."), *GetClass()->GetName());
		return;
	}

	LegalPopup->OpenLegal(Document);
}

void UMKHGameMenuWidget::CloseLegal()
{
	if (IsValid(LegalPopup))
	{
		LegalPopup->CloseLegal();
	}
}

bool UMKHGameMenuWidget::IsLegalOpen() const
{
	return IsValid(LegalPopup) && LegalPopup->IsOpen();
}

bool UMKHGameMenuWidget::IsModalWindowOpen() const
{
	return IsCreditsOpen() || IsLegalOpen();
}

void UMKHGameMenuWidget::CloseModalWindows()
{
	CloseCredits();
	CloseLegal();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

FReply UMKHGameMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Esc reaches the HUD through the game menu input action; the pad's Back is handled here, where the focus is.
	if (!UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent) || !bIsOpen)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	if (IsModalWindowOpen())
	{
		CloseModalWindows();
	}
	else
	{
		CloseGameMenu();
	}
	return FReply::Handled();
}

void UMKHGameMenuWidget::RestartGame()
{
	AMKHPlayerController* PC = Cast<AMKHPlayerController>(GetOwningPlayer());
	if (!IsValid(PC))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHGameMenuWidget::RestartGame: no owning MKH player controller."));
		return;
	}

	PC->Server_RestartMatch();
}
