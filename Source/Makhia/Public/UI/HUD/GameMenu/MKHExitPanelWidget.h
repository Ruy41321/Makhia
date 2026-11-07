// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHExitPanelWidget.generated.h"

class UButton;

/**
 * Exit panel of the game menu: decides which "leave" button is shown and runs it.
 *
 * - Restart Game: only in a match against the bot; reopens it from scratch (UMKHGameInstance::RestartOfflineMatch).
 * - Exit Lobby: only in the lobby level with at least one other player; leaves for a new lobby alone.
 * - Return to Lobby: only outside the lobby level (during a match).
 * Both go through UMKHGameInstance::LeaveToNewLobby (host when logged in, offline otherwise).
 * - Quit: always shown; quits the application.
 * The Blueprint (W_ExitPanel) owns only the layout.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHExitPanelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Shows the buttons that fit the current level, match mode and lobby population, hides the others. */
	void RefreshButtons();

	/** Returns true when another player shares the lobby with the local one. */
	bool HasOtherPlayersInLobby() const;

	/** Restart Game callback: restarts the match against the bot from scratch. */
	UFUNCTION()
	void HandleRestartClicked();

	/** Exit Lobby and Return to Lobby callback: leaves for a new lobby. */
	UFUNCTION()
	void HandleLeaveClicked();

	/** Quit callback: quits the application. */
	UFUNCTION()
	void HandleQuitClicked();

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Restarts the match against the bot from scratch. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RestartGameButton;

	/** Leaves a shared lobby for a new one. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExitLobbyButton;

	/** Leaves the match and goes back to the lobby. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReturnLobbyButton;

	/** Quits the application. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;
};
