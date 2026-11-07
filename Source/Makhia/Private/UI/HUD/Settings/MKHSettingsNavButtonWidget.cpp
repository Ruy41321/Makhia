// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHSettingsNavButtonWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHSettingsNavButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(NavButton))
	{
		NavButton->OnClicked.AddDynamic(this, &UMKHSettingsNavButtonWidget::HandleButtonClicked);
	}
}

void UMKHSettingsNavButtonWidget::NativeDestruct()
{
	if (IsValid(NavButton))
	{
		NavButton->OnClicked.RemoveDynamic(this, &UMKHSettingsNavButtonWidget::HandleButtonClicked);
	}

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHSettingsNavButtonWidget::SetupNavButton(int32 InPageIndex, const FText& InDisplayName)
{
	PageIndex = InPageIndex;

	if (IsValid(NavLabelText))
	{
		NavLabelText->SetText(InDisplayName);
	}
}

void UMKHSettingsNavButtonWidget::SetSelected(bool bInSelected)
{
	bIsSelected = bInSelected;

	if (IsValid(NavLabelText))
	{
		NavLabelText->SetColorAndOpacity(bInSelected ? SelectedLabelColor : NormalLabelColor);
	}

	if (IsValid(SelectionIndicator))
	{
		SelectionIndicator->SetVisibility(bInSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (IsValid(SelectionBackground))
	{
		SelectionBackground->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	BP_OnSelectionChanged(bInSelected);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSettingsNavButtonWidget::HandleButtonClicked()
{
	OnNavButtonClicked.Broadcast(PageIndex);
}
