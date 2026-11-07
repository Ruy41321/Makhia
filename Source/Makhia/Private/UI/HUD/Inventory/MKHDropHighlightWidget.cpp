// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHDropHighlightWidget.h"

#include "Components/TextBlock.h"

void UMKHDropHighlightWidget::SetHighlightActive(bool bActive)
{
	if (IsValid(FadeVFX))
	{
		FadeVFX->SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMKHDropHighlightWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(FadedText))
	{
		FadedText->SetText(Text);
		FadedText->SetVisibility(bShowText ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
