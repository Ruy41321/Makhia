// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHAccountChipWidget.generated.h"

class UButton;
class UMKHAuthSubsystem;
class UTextBlock;
class UWidget;

/**
 * Account chip in a corner of the lobby: who is signed in and how, with the matching actions.
 *
 * - Signed in: display name, method badge (platform name, "Guest"...), Sign out when the backend allows it (not with
 *   Steam); a guest also gets "Sign in with <platform account>" (UMKHAuthSubsystem::SwitchToPlatformAccount).
 * - Signing in: a status line only.
 * - Offline: "Offline" and why (no online services, no internet connection, not signed in), Sign in (opens the
 *   login panel of AMKHLobbyHUD) when a login can start.
 * - Always: a button that opens the game menu, with a badge while the friend list has pending lobby invites or
 *   friend requests (UMKHFriendsSubsystem::GetPendingNotificationCount).
 * Refreshed every tick while shown. The Blueprint (W_AccountChip) owns the layout.
 * Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHAccountChipWidget : public UUserWidget
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

	/** Updates texts and buttons from the sign-in state. */
	void Refresh();

	/** Sign in callback: opens the login panel. */
	UFUNCTION()
	void HandleSignInClicked();

	/** Sign out callback: logs the player out. */
	UFUNCTION()
	void HandleSignOutClicked();

	/** Switch account callback: moves a guest to the platform account. */
	UFUNCTION()
	void HandleSwitchAccountClicked();

	/** Game menu callback: opens or closes the game menu of AMKHLobbyHUD. */
	UFUNCTION()
	void HandleGameMenuClicked();

	/** Returns the line explaining why the player is offline. */
	static FText GetOfflineReasonText(const UMKHAuthSubsystem* Auth);

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Display name, or the sign-in status. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** Method badge when signed in, reason when offline; collapsed when empty. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MethodText;

	/** Opens the login panel (offline only). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SignInButton;

	/** Logs the player out (signed in, and only when the backend allows it: never with Steam). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SignOutButton;

	/** Moves a guest to the platform account (guests only). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SwitchAccountButton;

	/** Label of SwitchAccountButton ("Sign in with <Platform>"). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SwitchAccountText;

	/** Icon button that opens the game menu (friends, settings...). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> GameMenuButton;

	/** Red "!" badge on GameMenuButton, shown while something in the friend list waits for an answer. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GameMenuBadge;
};
