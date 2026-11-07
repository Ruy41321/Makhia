// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventoryPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "UI/HUD/Inventory/InventoryDashboardWidget.h"
#include "UI/HUD/Inventory/MKHDropHighlightWidget.h"
#include "UI/HUD/Inventory/MKHInventoryEntryWidget.h"

void UMKHInventoryPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Entries.Reset();
	if (IsValid(WidgetTree))
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (UMKHInventoryEntryWidget* Entry = Cast<UMKHInventoryEntryWidget>(Widget))
			{
				Entries.Add(Entry);
			}
		});
	}
}

void UMKHInventoryPanelWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	SetDropHighlight(false);
}

void UMKHInventoryPanelWidget::SetDashboard(UInventoryDashboardWidget* InDashboard)
{
	Dashboard = InDashboard;
	for (UMKHInventoryEntryWidget* Entry : Entries)
	{
		if (IsValid(Entry))
		{
			Entry->SetDashboard(InDashboard);
		}
	}
}

void UMKHInventoryPanelWidget::SetDropHighlight(bool bActive) const
{
	if (IsValid(DropHighlight))
	{
		DropHighlight->SetHighlightActive(bActive);
	}
}

UMKHInventoryEntryWidget* UMKHInventoryPanelWidget::FindEntryBySlotTag(const FGameplayTag& InSlotTag) const
{
	for (UMKHInventoryEntryWidget* Entry : Entries)
	{
		if (IsValid(Entry) && Entry->GetSlotTag().MatchesTagExact(InSlotTag))
		{
			return Entry;
		}
	}
	return nullptr;
}

UMKHInventoryEntryWidget* UMKHInventoryPanelWidget::FindEntryShowingItem(int64 ItemID) const
{
	for (UMKHInventoryEntryWidget* Entry : Entries)
	{
		if (IsValid(Entry) && Entry->GetItemID() != 0 && Entry->GetItemID() == ItemID)
		{
			return Entry;
		}
	}
	return nullptr;
}
