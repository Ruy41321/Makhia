// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHQuantityPopupWidget.h"

#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "MKHLogChannels.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

void UMKHQuantityPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Visible (not self-hit-test-invisible) so clicks outside the panel reach NativeOnMouseButtonDown.
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	if (IsValid(QuantitySlider))
	{
		QuantitySlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderValueChanged);
	}
	if (IsValid(ConfirmButton))
	{
		ConfirmButton->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (IsValid(CancelButton))
	{
		CancelButton->OnClicked.AddDynamic(this, &ThisClass::HandleCancelClicked);
	}
}

void UMKHQuantityPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetKeyboardFocus();
}

FReply UMKHQuantityPopupWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent))
	{
		CancelQuantity();
		return FReply::Handled();
	}
	if (Key == EKeys::Enter || UMKHMenuNavigationLibrary::IsGamepadAcceptKey(InKeyEvent))
	{
		ConfirmQuantity();
		return FReply::Handled();
	}
	if (Key == EKeys::Left || Key == EKeys::Right)
	{
		SetSelectedQuantity(SelectedQuantity + (Key == EKeys::Right ? 1 : -1));
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FNavigationReply UMKHQuantityPopupWidget::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent,
	const FNavigationReply& InDefaultReply)
{
	const EUINavigation Direction = InNavigationEvent.GetNavigationType();
	if (Direction == EUINavigation::Left || Direction == EUINavigation::Right)
	{
		SetSelectedQuantity(SelectedQuantity + (Direction == EUINavigation::Right ? 1 : -1));
	}
	return FNavigationReply::Stop();
}

FReply UMKHQuantityPopupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bInsidePanel = IsValid(PanelBox)
		&& PanelBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
	if (!bInsidePanel)
	{
		CancelQuantity();
	}
	return FReply::Handled();
}

void UMKHQuantityPopupWidget::InitQuantityPopup(const UInventoryItem* Item, const FText& ConfirmLabel)
{
	if (!IsValid(Item))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHQuantityPopupWidget::InitQuantityPopup: invalid item, closing."));
		CancelQuantity();
		return;
	}

	ItemID = Item->GetItemID();
	MaxQuantity = FMath::Max(Item->GetQuantity(), 1);

	if (IsValid(ItemNameText))
	{
		ItemNameText->SetText(FText::Format(NSLOCTEXT("MKHQuantityPopup", "ItemTitle", "{0}  x{1}"),
			Item->GetItemName(), FText::AsNumber(MaxQuantity)));
	}
	if (IsValid(ConfirmText))
	{
		ConfirmText->SetText(ConfirmLabel);
	}
	if (IsValid(QuantitySlider))
	{
		QuantitySlider->SetMinValue(1.f);
		QuantitySlider->SetMaxValue(static_cast<float>(MaxQuantity));
		QuantitySlider->SetStepSize(1.f);
	}

	SetSelectedQuantity(1);
}

void UMKHQuantityPopupWidget::SetSelectedQuantity(int32 NewQuantity)
{
	SelectedQuantity = FMath::Clamp(NewQuantity, 1, MaxQuantity);

	if (IsValid(QuantitySlider))
	{
		QuantitySlider->SetValue(static_cast<float>(SelectedQuantity));
	}
	if (IsValid(QuantityText))
	{
		QuantityText->SetText(FText::Format(NSLOCTEXT("MKHQuantityPopup", "Quantity", "{0} / {1}"),
			FText::AsNumber(SelectedQuantity), FText::AsNumber(MaxQuantity)));
	}
}

void UMKHQuantityPopupWidget::ConfirmQuantity()
{
	if (ItemID != 0)
	{
		OnQuantityConfirmed.Broadcast(ItemID, SelectedQuantity);
	}
	RemoveFromParent();
}

void UMKHQuantityPopupWidget::CancelQuantity()
{
	RemoveFromParent();
}

void UMKHQuantityPopupWidget::HandleSliderValueChanged(float Value)
{
	SetSelectedQuantity(FMath::RoundToInt(Value));
}

void UMKHQuantityPopupWidget::HandleConfirmClicked()
{
	ConfirmQuantity();
}

void UMKHQuantityPopupWidget::HandleCancelClicked()
{
	CancelQuantity();
}
