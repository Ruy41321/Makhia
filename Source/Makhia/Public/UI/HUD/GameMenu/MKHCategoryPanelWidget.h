// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHCategoryPanelWidget.generated.h"

class UButton;

/**
 * Category panel of the game menu (W_CategoryPanel): Credits and Privacy & EULA.
 *
 * The Privacy & EULA button is handled here and opens the legal window of the game menu that owns the panel.
 * The Credits button still goes through the Blueprint dispatcher (OnCreditsButtonClicked → W_GameMenu).
 * See Documentation/GameMenuArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHCategoryPanelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Privacy & EULA button callback: opens the legal window on the privacy policy. */
	UFUNCTION()
	void HandleLegalClicked();

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Button opening the privacy policy / EULA window. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LegalButton;
};
