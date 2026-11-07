// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Loading/MKHTravelError.h"
#include "MKHTravelErrorWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * Modal shown in the lobby after an error sent the player back (UMKHLoadingScreenSubsystem): title, a clear
 * sentence for the player and the developer code ("Error code: NET-CONNECTION-LOST").
 *
 * AMKHLobbyHUD creates it once the loading screen is gone; OK, Esc and Enter close it. The Blueprint (W_TravelError)
 * owns the layout. Full model: see Documentation/LoadingScreen.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHTravelErrorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows an error.
	 *
	 * @param ErrorInfo  Error to show.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loading|Error")
	void SetError(const FMKHTravelErrorInfo& ErrorInfo);

	/** Broadcast when the dialog wants to be closed (OK, Esc, Enter). */
	FSimpleMulticastDelegate OnCloseRequested;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** OK button callback. */
	UFUNCTION()
	void HandleOkClicked();

	// ============================================================
	// Properties
	// ============================================================

	/** Designer preview only: error shown in the designer. */
	UPROPERTY(EditAnywhere, Category = "Loading|Error|Preview")
	EMKHTravelError PreviewError = EMKHTravelError::ConnectionLost;

	/** Short title. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	/** Sentence for the player. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	/** Developer code line. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CodeText;

	/** Closes the dialog. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> OkButton;
};
