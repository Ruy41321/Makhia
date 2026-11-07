// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/MKHExitPanelWidget.h"

#include "Components/Button.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameInstance/MKHGameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MKHLogChannels.h"

namespace MKHExitPanel
{
	/** Shows or collapses an optional button, touching it only when its state changes. */
	static void SetShown(UButton* Button, const bool bShown)
	{
		const ESlateVisibility Wanted = bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (IsValid(Button) && Button->GetVisibility() != Wanted)
		{
			Button->SetVisibility(Wanted);
		}
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHExitPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(RestartGameButton))
	{
		RestartGameButton->OnClicked.AddUniqueDynamic(this, &UMKHExitPanelWidget::HandleRestartClicked);
	}

	if (IsValid(ExitLobbyButton))
	{
		ExitLobbyButton->OnClicked.AddUniqueDynamic(this, &UMKHExitPanelWidget::HandleLeaveClicked);
	}

	if (IsValid(ReturnLobbyButton))
	{
		ReturnLobbyButton->OnClicked.AddUniqueDynamic(this, &UMKHExitPanelWidget::HandleLeaveClicked);
	}

	if (IsValid(QuitButton))
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &UMKHExitPanelWidget::HandleQuitClicked);
	}
}

void UMKHExitPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshButtons();
}

void UMKHExitPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Only ticks while shown: keeps Exit Lobby in sync when a player joins or leaves.
	RefreshButtons();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHExitPanelWidget::RefreshButtons()
{
	const UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this);
	const bool bInLobby = IsValid(GameInstance) && GameInstance->IsInLobbyLevel();

	MKHExitPanel::SetShown(RestartGameButton, IsValid(GameInstance) && GameInstance->IsInOfflineMatch());
	MKHExitPanel::SetShown(ExitLobbyButton, bInLobby && HasOtherPlayersInLobby());
	MKHExitPanel::SetShown(ReturnLobbyButton, !bInLobby);
}

bool UMKHExitPanelWidget::HasOtherPlayersInLobby() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	return IsValid(GameState) && GameState->PlayerArray.Num() > 1;
}

void UMKHExitPanelWidget::HandleRestartClicked()
{
	UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this);
	if (!IsValid(GameInstance))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHExitPanelWidget::HandleRestartClicked: no MKH game instance."));
		return;
	}

	GameInstance->RestartOfflineMatch();
}

void UMKHExitPanelWidget::HandleLeaveClicked()
{
	UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this);
	if (!IsValid(GameInstance))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHExitPanelWidget::HandleLeaveClicked: no MKH game instance."));
		return;
	}

	GameInstance->LeaveToNewLobby();
}

void UMKHExitPanelWidget::HandleQuitClicked()
{
	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHExitPanelWidget::HandleQuitClicked: no owning player controller."));
		return;
	}

	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
}
