// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "UI/HUD/GameMenu/MKHLegalDocument.h"
#include "MKHGameMenuWidget.generated.h"

class UMKHCreditsWidget;
class UMKHLegalWidget;

DECLARE_MULTICAST_DELEGATE(FGameMenuDelegateSignature);

/**
 * In-game menu opened with the game menu input action (Esc): a side column with the friend list,
 * the category and exit panels, plus the modal credits and legal (privacy policy / EULA) windows on top.
 *
 * Panel switching and layout live in the Blueprint (W_GameMenu); this class owns the visibility
 * state the HUD relies on and the modal windows. Leaving and quitting live in UMKHExitPanelWidget.
 * Structure and styling: see Documentation/GameMenuArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHGameMenuWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Broadcast when the menu opens. The HUD switches the input focus to the UI. */
	FGameMenuDelegateSignature FOnOpenGameMenuDelegate;

	/** Broadcast when the menu closes. The HUD gives the input focus back to the game. */
	FGameMenuDelegateSignature FOnCloseGameMenuDelegate;

	/** Broadcast right before the menu is removed from the viewport. The HUD drops its references. */
	FGameMenuDelegateSignature FOnDestroyGameMenuDelegate;

	/** Returns true while the menu is shown. */
	bool IsOpen() const { return bIsOpen; }

	/** Shows the menu and notifies the HUD and the Blueprint (OnOpenGameMenu). */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Visibility")
	void OpenGameMenu();

	/** Blueprint hook fired every time the menu opens, to pick the panel to show. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GameMenu|Visibility")
	void OnOpenGameMenu();

	/** Hides the menu, closing the modal windows with it. */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Visibility")
	void CloseGameMenu();

	/** Notifies the HUD and removes the menu from the viewport. */
	void DestroyGameMenu();

	/** Removes every native listener bound to the menu delegates. */
	void UnbindAllEventsFromDelegates();

	/** Opens the modal credits window over the menu. Does nothing if the Blueprint has no CreditsPopup. */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Credits")
	void OpenCredits();

	/** Closes the credits window. */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Credits")
	void CloseCredits();

	/**
	 * Opens the settings screen (or closes it) through the HUD of the current level:
	 * AMainHUD in a match, AMKHLobbyHUD in the lobby.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Settings")
	void ToggleSettings();

	/** Returns true while the credits window is shown. */
	UFUNCTION(BlueprintPure, Category = "GameMenu|Credits")
	bool IsCreditsOpen() const;

	/**
	 * Opens the modal privacy policy / EULA window over the menu. Does nothing if the Blueprint has no LegalPopup.
	 *
	 * @param Document  Document shown first; the window has a tab for each.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Legal")
	void OpenLegal(EMKHLegalDocument Document = EMKHLegalDocument::PrivacyPolicy);

	/** Closes the privacy policy / EULA window. */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Legal")
	void CloseLegal();

	/** Returns true while the privacy policy / EULA window is shown. */
	UFUNCTION(BlueprintPure, Category = "GameMenu|Legal")
	bool IsLegalOpen() const;

	/** Returns true while a modal window (credits, legal) is shown; the HUD closes it before the menu on Esc. */
	UFUNCTION(BlueprintPure, Category = "GameMenu|Visibility")
	bool IsModalWindowOpen() const;

	/** Closes every modal window (credits, legal) and leaves the menu open. */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Visibility")
	void CloseModalWindows();

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Gamepad Back (B): closes the modal window, else the menu, like Esc. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/**
	 * Restarts the match for every player.
	 * Delegated to AMKHPlayerController::Server_RestartMatch because a UFUNCTION(Server) declared
	 * on a UUserWidget never leaves the client (UUserWidget has no RPC callspace resolution).
	 */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Actions")
	void RestartGame();

	// ============================================================
	// Properties
	// ============================================================

	/** Modal credits window, placed full screen above every other element of the menu. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHCreditsWidget> CreditsPopup;

	/** Modal privacy policy / EULA window, placed full screen above every other element of the menu. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHLegalWidget> LegalPopup;

private:
	/** True while the menu is shown. */
	bool bIsOpen = false;
};
