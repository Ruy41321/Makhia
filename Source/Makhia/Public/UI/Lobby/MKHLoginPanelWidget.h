// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/Auth/MKHAuthTypes.h"
#include "Types/SlateEnums.h"
#include "MKHLoginPanelWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/**
 * Modal "Play online" window of the lobby, opened by AMKHLobbyHUD from the account chip or from a locked
 * VsPlayer card: sign in with the platform account, or play as a guest with a chosen name.
 *
 * The guest name is validated live (UMKHAuthSubsystem::ValidateGuestName): counter, message, Continue enabled
 * only for a valid name. Buttons are disabled while a login cannot start; failures show a readable message
 * (UMKHAuthSubsystem::GetAuthErrorText). Esc, the Close button and a click outside ask the HUD to close it;
 * a successful login closes it too. The Blueprint (W_LoginPanel) owns the layout.
 * Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHLoginPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Broadcast when the panel wants to be closed (Close, Esc, click outside, signed in). */
	FSimpleMulticastDelegate OnCloseRequested;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Binds the buttons and the name field. */
	void BindWidgetEvents();

	/** Enables the buttons that can act now and updates the status line. */
	void RefreshState();

	/** Validates the guest name field: counter, message and Continue button. */
	void RefreshGuestValidation();

	/** Starts the guest login with the typed name, if valid. */
	void SubmitGuestName();

	/** Returns the trimmed guest name when it is valid, or an empty string. */
	FString GetValidGuestName() const;

	/** Close / backdrop callback. */
	UFUNCTION()
	void HandleCloseClicked();

	/** Platform account callback: starts the platform login. */
	UFUNCTION()
	void HandlePlatformClicked();

	/** Guest callback: shows the name field. */
	UFUNCTION()
	void HandleGuestClicked();

	/** Continue callback: starts the guest login. */
	UFUNCTION()
	void HandleContinueClicked();

	/** Name field callback: live validation. */
	UFUNCTION()
	void HandleGuestNameChanged(const FText& Text);

	/** Name field callback: Enter submits the name. */
	UFUNCTION()
	void HandleGuestNameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	/** Auth callback: shows why a login failed. */
	UFUNCTION()
	void HandleLoginFailed(EMKHAuthMethod Method, EMKHAuthError Error);

	/** Auth callback: closes the panel once signed in. */
	UFUNCTION()
	void HandleAuthStateChanged(EMKHAuthState NewState, EMKHAuthState OldState);

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Full-screen invisible button behind the window: a click outside closes the panel. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackdropButton;

	/** Closes the panel. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** Signs in with the platform account. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> PlatformButton;

	/** Label of PlatformButton ("Sign in with <Platform>", from the provider). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlatformButtonText;

	/** Guest part of the window; collapsed when the provider has no guest login. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GuestSection;

	/** Shows the name field. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> GuestButton;

	/** Name field, counter, message and Continue; collapsed until GuestButton is clicked. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GuestForm;

	/** Guest display name, prefilled with the last one used. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> GuestNameBox;

	/** "7/16" counter of the name field. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GuestNameCounter;

	/** Validation message of the name field. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GuestNameError;

	/** Starts the guest login; enabled only for a valid name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton;

	/** "Signing in..." or the last error. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	/** Message of the last failed login, shown until another login starts. */
	FText LastErrorText;
};
