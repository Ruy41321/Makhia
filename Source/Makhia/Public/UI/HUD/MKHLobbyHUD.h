// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MKHLobbyHUD.generated.h"

class APlayerController;
class UMKHBootOverlayWidget;
class UMKHGameMenuWidget;
class UMKHLobbyNoticeWidget;
class UMKHLoginPanelWidget;
class UMKHSettingsScreenWidget;
class UMKHTravelErrorWidget;

/**
 * HUD of the lobby level: hosts the game menu, the settings screen, the login panel, the boot overlay, the
 * travel error dialog and the lobby toasts.
 *
 * The lobby has no gameplay HUD, so this is the small counterpart of AMainHUD for the screens both levels
 * share, plus the two lobby-only ones (Documentation/OfflineModeArchitecture.md). The lobby keeps the cursor
 * and the UI input mode at all times, so opening or closing a screen changes no input focus. The Blueprint
 * (WBP_LobbyHUD) adds the lobby overlay.
 */
UCLASS()
class MAKHIA_API AMKHLobbyHUD : public AHUD
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/**
	 * Covers the lobby with the boot overlay while the start-up login is not settled, and shows a travel error that
	 * sent the player back here once the loading screen is gone.
	 */
	virtual void BeginPlay() override;

	/** Stops following the loading screen. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the lobby HUD of a player controller.
	 *
	 * @param PlayerController  Local player controller.
	 * @return The HUD, or null outside the lobby.
	 */
	static AMKHLobbyHUD* GetForPlayer(const APlayerController* PlayerController);

	/**
	 * Game menu input action handler: closes the topmost screen, or opens the menu.
	 * Order: travel error, settings screen, login panel, credits window, then the menu itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "LobbyHUD|GameMenu")
	void ToggleGameMenu();

	/** Opens the settings screen, or closes it when it is already open. */
	UFUNCTION(BlueprintCallable, Category = "LobbyHUD|Settings")
	void ToggleSettingsWidget();

	/**
	 * Opens the modal login panel, if it is not open yet. When the backend offers no choice (platform account only,
	 * see UMKHAuthSubsystem::HasSignInChoice) the platform sign-in starts at once instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "LobbyHUD|Login")
	void OpenLoginPanel();

	/** Closes the login panel, if it is open. */
	UFUNCTION(BlueprintCallable, Category = "LobbyHUD|Login")
	void CloseLoginPanel();

	/** Returns true while the login panel is open. */
	UFUNCTION(BlueprintPure, Category = "LobbyHUD|Login")
	bool IsLoginPanelOpen() const;

	/** Shows the travel error waiting in UMKHLoadingScreenSubsystem, once no loading screen covers the lobby. */
	UFUNCTION(BlueprintCallable, Category = "LobbyHUD|Error")
	void ShowPendingTravelError();

	/** Closes the travel error dialog, if it is open. */
	UFUNCTION(BlueprintCallable, Category = "LobbyHUD|Error")
	void CloseTravelError();

	/** Returns true while the travel error dialog is open. */
	UFUNCTION(BlueprintPure, Category = "LobbyHUD|Error")
	bool IsTravelErrorOpen() const;

	/** Shows the notice waiting in UMKHLobbySubsystem as a toast, once no loading screen covers the lobby. */
	UFUNCTION(BlueprintCallable, Category = "LobbyHUD|Notice")
	void ShowPendingLobbyNotice();

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Creates the boot overlay on top of every other screen when the boot is in progress. */
	void CreateBootOverlay();

	/** Creates the game menu, collapsed, the first time it is needed. */
	void CreateGameMenuWidget();

	/** Creates the settings screen and adds it to the viewport. */
	void CreateSettingsWidget();

	/** Removes the settings screen from the viewport. */
	void DestroySettingsWidget();

	/** Settings screen callback: the player asked to leave the screen. */
	void OnSettingsClosed();

	/** Follows the loading screen and the lobby notices to show errors and notices once the screen is gone. */
	void BindLoadingScreen();

	/** Shows what waits once the loading screen is gone: the travel error, then the lobby notice. */
	void ShowPendingMessages();

	// ============================================================
	// Properties
	// ============================================================

	/** Game menu Blueprint (W_GameMenu). */
	UPROPERTY(EditDefaultsOnly, Category = "LobbyHUD|GameMenu")
	TSubclassOf<UMKHGameMenuWidget> GameMenuWidgetClass;

	/** Settings screen Blueprint (W_SettingsScreen). */
	UPROPERTY(EditDefaultsOnly, Category = "LobbyHUD|Settings")
	TSubclassOf<UMKHSettingsScreenWidget> SettingsScreenWidgetClass;

	/** Game menu instance, created on first use. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHGameMenuWidget> GameMenuWidget;

	/** Settings screen instance, alive only while the screen is open. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHSettingsScreenWidget> SettingsScreenWidget;

	/** Boot overlay Blueprint (W_BootOverlay). */
	UPROPERTY(EditDefaultsOnly, Category = "LobbyHUD|Boot")
	TSubclassOf<UMKHBootOverlayWidget> BootOverlayWidgetClass;

	/** Login panel Blueprint (W_LoginPanel). */
	UPROPERTY(EditDefaultsOnly, Category = "LobbyHUD|Login")
	TSubclassOf<UMKHLoginPanelWidget> LoginPanelWidgetClass;

	/** Login panel instance, alive only while the panel is open. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHLoginPanelWidget> LoginPanelWidget;

	/** Travel error dialog Blueprint (W_TravelError). */
	UPROPERTY(EditDefaultsOnly, Category = "LobbyHUD|Error")
	TSubclassOf<UMKHTravelErrorWidget> TravelErrorWidgetClass;

	/** Travel error dialog, alive only while it is open. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHTravelErrorWidget> TravelErrorWidget;

	/** Lobby toast Blueprint (W_LobbyNotice). */
	UPROPERTY(EditDefaultsOnly, Category = "LobbyHUD|Notice")
	TSubclassOf<UMKHLobbyNoticeWidget> LobbyNoticeWidgetClass;

	/** Lobby toast, kept to reuse it while it is on screen. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHLobbyNoticeWidget> LobbyNoticeWidget;

	/** Viewport Z order of the login panel: above the lobby overlay and the game menu. */
	static constexpr int32 LoginPanelZOrder = 50;

	/** Viewport Z order of the travel error dialog: above the login panel, below the boot overlay. */
	static constexpr int32 TravelErrorZOrder = 60;

	/** Viewport Z order of the lobby toast: above the lobby screens, below the error dialog. */
	static constexpr int32 LobbyNoticeZOrder = 55;

	/** Viewport Z order of the boot overlay: above everything. */
	static constexpr int32 BootOverlayZOrder = 100;

private:
	/** Registration of UMKHLoadingScreenSubsystem::OnLoadingScreenHidden. */
	FDelegateHandle LoadingHiddenHandle;

	/** Registration of UMKHLoadingScreenSubsystem::OnTravelErrorReported. */
	FDelegateHandle TravelErrorHandle;

	/** Registration of UMKHLobbySubsystem::OnLobbyNoticePosted. */
	FDelegateHandle LobbyNoticeHandle;
};
