// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventoryActionMenuWidget.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "MKHLogChannels.h"
#include "UI/HUD/Inventory/InventoryDashboardWidget.h"
#include "UI/HUD/Inventory/MKHInventoryEntryWidget.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

#define LOCTEXT_NAMESPACE "MKHInventoryActionMenu"

// ============================================================
// Lifecycle
// ============================================================

void UMKHInventoryActionMenuWidget::InitActionMenu(UMKHInventoryEntryWidget* InEntry)
{
	Entry = InEntry;
}

void UMKHInventoryActionMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	MainActionButton->OnClicked.AddDynamic(this, &ThisClass::HandleMainActionClicked);
	QuickSlotButton->OnClicked.AddDynamic(this, &ThisClass::HandleQuickSlotClicked);
	DropButton->OnClicked.AddDynamic(this, &ThisClass::HandleDropClicked);
	if (IsValid(QuickSlot1Button))
	{
		QuickSlot1Button->OnClicked.AddDynamic(this, &ThisClass::HandleQuickSlot1Clicked);
	}
	if (IsValid(QuickSlot2Button))
	{
		QuickSlot2Button->OnClicked.AddDynamic(this, &ThisClass::HandleQuickSlot2Clicked);
	}
	if (IsValid(QuickSlot3Button))
	{
		QuickSlot3Button->OnClicked.AddDynamic(this, &ThisClass::HandleQuickSlot3Clicked);
	}
	if (IsValid(SplitButton))
	{
		SplitButton->OnClicked.AddDynamic(this, &ThisClass::HandleSplitClicked);
	}
}

void UMKHInventoryActionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsValid(GetItem()) || !IsValid(GetDashboard()))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHInventoryActionMenuWidget: opened without an entry, item or dashboard."));
		return;
	}

	if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(ActionBox->Slot))
	{
		BoxSlot->SetPosition(UMKHMenuNavigationLibrary::GetPopupPosition(GetEntry()));
	}
	SetupMainAction();
	SetupQuickSlotButton();
	SetupSplitButton();
	SetupGamepadQuickSlotButtons();
}

FReply UMKHInventoryActionMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent))
	{
		CloseSelf();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHInventoryActionMenuWidget::SetupMainAction() const
{
	using namespace MKHGameplayTags::Equip;

	const FGameplayTag ItemTag = GetItem()->GetItemTag();
	FText Label = LOCTEXT("Use", "Use");
	if (GetEntry()->IsEquipEntry())
	{
		Label = LOCTEXT("Unequip", "Unequip");
	}
	else if (ItemTag.MatchesTag(Category_Equipment))
	{
		Label = LOCTEXT("Equip", "Equip");
	}

	MainActionText->SetText(Label);
	MainActionButton->SetVisibility(ItemTag.MatchesTag(Category_Weapon) ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UMKHInventoryActionMenuWidget::SetupQuickSlotButton() const
{
	using namespace MKHGameplayTags::Equip;

	const UInventoryItem* Item = GetItem();
	const FGameplayTag ItemTag = Item->GetItemTag();
	const bool bQuickSlottable = ItemTag.MatchesTag(Category_Consumable) || ItemTag.MatchesTag(Category_Weapon);

	QuickSlotText->SetText(Item->IsQuickSlotted() ? LOCTEXT("RemoveQuickSlot", "Remove QuickSlot") : LOCTEXT("AddQuickSlot", "Add QuickSlot"));
	QuickSlotButton->SetVisibility(bQuickSlottable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMKHInventoryActionMenuWidget::SetupSplitButton() const
{
	if (IsValid(SplitButton))
	{
		SplitButton->SetVisibility(GetDashboard()->CanSplitItem(GetItem()) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMKHInventoryActionMenuWidget::SetupGamepadQuickSlotButtons() const
{
	if (!GetDashboard()->ShowsGenericQuickSlotButton(GetItem()))
	{
		QuickSlotButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetupQuickSlotChoiceButton(QuickSlot1Button, QuickSlot1Text, 0);
	SetupQuickSlotChoiceButton(QuickSlot2Button, QuickSlot2Text, 1);
	SetupQuickSlotChoiceButton(QuickSlot3Button, QuickSlot3Text, 2);
}

void UMKHInventoryActionMenuWidget::SetupQuickSlotChoiceButton(UButton* Button, UTextBlock* Label, int32 ChoiceIndex) const
{
	if (!IsValid(Button))
	{
		return;
	}

	const FGameplayTag Choice = GetDashboard()->GetGamepadQuickSlotChoice(GetItem(), ChoiceIndex);
	if (IsValid(Label) && Choice.IsValid())
	{
		Label->SetText(UInventoryDashboardWidget::GetQuickSlotChoiceLabel(Choice));
	}
	Button->SetVisibility(Choice.IsValid() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMKHInventoryActionMenuWidget::AssignQuickSlotChoice(int32 ChoiceIndex)
{
	UInventoryDashboardWidget* Dashboard = GetDashboard();
	const UInventoryItem* Item = GetItem();
	if (IsValid(Dashboard) && IsValid(Item))
	{
		Dashboard->RequestAddToQuickSlot(Item->GetItemID(), Dashboard->GetGamepadQuickSlotChoice(Item, ChoiceIndex));
	}
	CloseSelf();
}

void UMKHInventoryActionMenuWidget::CloseSelf() const
{
	if (UInventoryDashboardWidget* Dashboard = GetDashboard())
	{
		Dashboard->CloseActionMenu();
	}
}

UInventoryDashboardWidget* UMKHInventoryActionMenuWidget::GetDashboard() const
{
	const UMKHInventoryEntryWidget* EntryWidget = GetEntry();
	return IsValid(EntryWidget) ? EntryWidget->GetDashboard() : nullptr;
}

UInventoryItem* UMKHInventoryActionMenuWidget::GetItem() const
{
	const UMKHInventoryEntryWidget* EntryWidget = GetEntry();
	return IsValid(EntryWidget) ? EntryWidget->GetInventoryItem() : nullptr;
}

// ============================================================
// Button handlers
// ============================================================

void UMKHInventoryActionMenuWidget::HandleMainActionClicked()
{
	UInventoryDashboardWidget* Dashboard = GetDashboard();
	const UInventoryItem* Item = GetItem();
	if (IsValid(Dashboard) && IsValid(Item))
	{
		if (GetEntry()->IsEquipEntry())
		{
			Dashboard->RequestUnequipItem(Item->GetSlotTag());
		}
		else
		{
			Dashboard->RequestUseItem(Item->GetItemID(), 1);
		}
	}
	CloseSelf();
}

void UMKHInventoryActionMenuWidget::HandleQuickSlotClicked()
{
	UInventoryDashboardWidget* Dashboard = GetDashboard();
	UInventoryItem* Item = GetItem();
	if (IsValid(Dashboard) && IsValid(Item))
	{
		if (GetEntry()->IsQuickSlotEntry())
		{
			Dashboard->RequestRemoveFromQuickSlot(Item->GetItemID());
		}
		else
		{
			Dashboard->RequestAddToQuickSlot(Item->GetItemID(), Dashboard->FindQuickSlotForItem(Item));
		}
	}
	CloseSelf();
}

void UMKHInventoryActionMenuWidget::HandleQuickSlot1Clicked()
{
	AssignQuickSlotChoice(0);
}

void UMKHInventoryActionMenuWidget::HandleQuickSlot2Clicked()
{
	AssignQuickSlotChoice(1);
}

void UMKHInventoryActionMenuWidget::HandleQuickSlot3Clicked()
{
	AssignQuickSlotChoice(2);
}

void UMKHInventoryActionMenuWidget::HandleSplitClicked()
{
	if (UInventoryDashboardWidget* Dashboard = GetDashboard())
	{
		Dashboard->RequestSplitItem(GetItem());
	}
	CloseSelf();
}

void UMKHInventoryActionMenuWidget::HandleDropClicked()
{
	if (UInventoryDashboardWidget* Dashboard = GetDashboard())
	{
		Dashboard->RequestDropItem(GetItem());
	}
	CloseSelf();
}

#undef LOCTEXT_NAMESPACE
