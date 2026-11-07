// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHKeybindCategoryWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

void UMKHKeybindCategoryWidget::SetCategoryName(const FText& InCategoryName, int32 InActionCount)
{
	if (IsValid(CategoryNameText))
	{
		CategoryNameText->SetText(InCategoryName);
	}

	if (IsValid(CategoryCountText))
	{
		CategoryCountText->SetText(FText::AsNumber(InActionCount));
	}
}

void UMKHKeybindCategoryWidget::SetColumnHeadersVisible(bool bVisible)
{
	if (IsValid(ColumnHeaderBox))
	{
		ColumnHeaderBox->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
