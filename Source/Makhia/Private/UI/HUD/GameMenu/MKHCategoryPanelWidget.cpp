// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/MKHCategoryPanelWidget.h"

#include "Components/Button.h"
#include "MKHLogChannels.h"
#include "UI/HUD/GameMenu/MKHGameMenuWidget.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHCategoryPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(LegalButton))
	{
		LegalButton->OnClicked.AddUniqueDynamic(this, &UMKHCategoryPanelWidget::HandleLegalClicked);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHCategoryPanelWidget::HandleLegalClicked()
{
	// The panel lives in the widget tree of the game menu, which owns the modal windows.
	UMKHGameMenuWidget* GameMenu = GetTypedOuter<UMKHGameMenuWidget>();
	if (!IsValid(GameMenu))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHCategoryPanelWidget::HandleLegalClicked: %s is not inside a game menu."), *GetName());
		return;
	}

	GameMenu->OpenLegal(EMKHLegalDocument::PrivacyPolicy);
}
