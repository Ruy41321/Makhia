// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventorySortMenuWidget.h"

#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "MKHLogChannels.h"
#include "UI/HUD/Inventory/InventoryDashboardWidget.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

#define LOCTEXT_NAMESPACE "MKHInventorySortMenu"

// ============================================================
// Public Interface
// ============================================================

void UMKHInventorySortMenuWidget::InitSortMenu(UInventoryDashboardWidget* InDashboard, const UWidget* InAnchor)
{
	Dashboard = InDashboard;
	Anchor = InAnchor;
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHInventorySortMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	CategoryButton->OnClicked.AddDynamic(this, &ThisClass::HandleCategoryClicked);
	RarityAscendingButton->OnClicked.AddDynamic(this, &ThisClass::HandleRarityAscendingClicked);
	RarityDescendingButton->OnClicked.AddDynamic(this, &ThisClass::HandleRarityDescendingClicked);
}

void UMKHInventorySortMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!Dashboard.IsValid() || !Anchor.IsValid())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHInventorySortMenuWidget: opened without a dashboard or an anchor."));
		return;
	}

	// Hit testable on the whole screen, so a click outside the buttons reaches NativeOnMouseButtonDown.
	SetVisibility(ESlateVisibility::Visible);

	if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(SortBox->Slot))
	{
		BoxSlot->SetPosition(UMKHMenuNavigationLibrary::GetPopupPosition(Anchor.Get()));
	}

	CategoryText->SetText(LOCTEXT("Category", "Category"));
	RarityAscendingText->SetText(LOCTEXT("RarityAscending", "Rarity: lowest first"));
	RarityDescendingText->SetText(LOCTEXT("RarityDescending", "Rarity: highest first"));

	const auto ShowMark = [SortMode = Dashboard->GetBagSortMode()](UWidget* Mark, EMKHBagSortMode MarkMode)
	{
		Mark->SetVisibility(SortMode == MarkMode ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	};
	ShowMark(CategoryMark, EMKHBagSortMode::Category);
	ShowMark(RarityAscendingMark, EMKHBagSortMode::RarityAscending);
	ShowMark(RarityDescendingMark, EMKHBagSortMode::RarityDescending);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

FReply UMKHInventorySortMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent) || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Top)
	{
		CloseSelf();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UMKHInventorySortMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	CloseSelf();
	return FReply::Handled();
}

void UMKHInventorySortMenuWidget::ChooseSortMode(EMKHBagSortMode SortMode) const
{
	if (Dashboard.IsValid())
	{
		Dashboard->RequestBagSort(SortMode);
	}
}

void UMKHInventorySortMenuWidget::CloseSelf() const
{
	if (Dashboard.IsValid())
	{
		Dashboard->CloseSortMenu();
	}
}

// ============================================================
// Button handlers
// ============================================================

void UMKHInventorySortMenuWidget::HandleCategoryClicked()
{
	ChooseSortMode(EMKHBagSortMode::Category);
}

void UMKHInventorySortMenuWidget::HandleRarityAscendingClicked()
{
	ChooseSortMode(EMKHBagSortMode::RarityAscending);
}

void UMKHInventorySortMenuWidget::HandleRarityDescendingClicked()
{
	ChooseSortMode(EMKHBagSortMode::RarityDescending);
}

#undef LOCTEXT_NAMESPACE
