// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventoryBagWidget.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Components/ListView.h"
#include "Data/MKHDataSubsystem.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "MKHLogChannels.h"
#include "UI/HUD/Inventory/InventoryDashboardWidget.h"
#include "UI/HUD/Inventory/MKHInventoryEntryDragOperation.h"
#include "UI/HUD/Inventory/MKHInventoryEntryWidget.h"

namespace
{
	/** What dropping an item on the bag does. */
	enum class EMKHBagReturn : uint8
	{
		None,
		Armor,
		Weapon,
		Consumable
	};

	/** An equipped or quick-slotted item can return to the bag; anything else cannot. */
	EMKHBagReturn ClassifyBagReturn(const UInventoryItem* Item)
	{
		using namespace MKHGameplayTags::Equip;

		if (!IsValid(Item) || !(Item->IsEquipped() || Item->IsQuickSlotted()))
		{
			return EMKHBagReturn::None;
		}

		const FGameplayTag ItemTag = Item->GetItemTag();
		if (ItemTag.MatchesTag(Category_Armor))
		{
			return EMKHBagReturn::Armor;
		}
		if (ItemTag.MatchesTag(Category_Weapon))
		{
			return EMKHBagReturn::Weapon;
		}
		return ItemTag.MatchesTag(Category_Consumable) ? EMKHBagReturn::Consumable : EMKHBagReturn::None;
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHInventoryBagWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!IsValid(InventorySlots))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHInventoryBagWidget: no InventorySlots list on %s."), *GetClass()->GetName());
		return;
	}
	InventorySlots->OnEntryWidgetGenerated().AddUObject(this, &ThisClass::HandleEntryGenerated);
	InventorySlots->OnEntryWidgetReleased().AddUObject(this, &ThisClass::HandleEntryReleased);
	InventorySlots->OnItemDoubleClicked().AddUObject(this, &ThisClass::HandleItemDoubleClicked);
}

// ============================================================
// Public Interface
// ============================================================

void UMKHInventoryBagWidget::SetDashboard(UInventoryDashboardWidget* InDashboard)
{
	Super::SetDashboard(InDashboard);

	if (IsValid(InventorySlots))
	{
		for (UUserWidget* EntryWidget : InventorySlots->GetDisplayedEntryWidgets())
		{
			HandleEntryGenerated(*EntryWidget);
		}
	}
}

void UMKHInventoryBagWidget::SyncItem(UInventoryItem* Item)
{
	if (!IsValid(Item) || !IsValid(InventorySlots))
	{
		return;
	}

	const int64 ItemID = Item->GetItemID();
	if (const TObjectPtr<UInventoryItem>* ExistingItem = ItemsMap.Find(ItemID); ExistingItem && IsValid(*ExistingItem))
	{
		(*ExistingItem)->CopyFrom(Item);
		return;
	}

	InventorySlots->AddItem(Item);
	ItemsMap.Add(ItemID, Item);
	ApplySort();
}

void UMKHInventoryBagWidget::RemoveItemByID(int64 ItemID)
{
	TObjectPtr<UInventoryItem> RemovedItem;
	if (!ItemsMap.RemoveAndCopyValue(ItemID, RemovedItem))
	{
		return;
	}
	if (IsValid(InventorySlots))
	{
		InventorySlots->RemoveItem(RemovedItem);
	}
}

void UMKHInventoryBagWidget::HandleQuickSlotRelocated(UInventoryItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	if (Item->IsQuickSlotted())
	{
		RemoveItemByID(Item->GetItemID());
	}
	else
	{
		SyncItem(Item);
	}
}

void UMKHInventoryBagWidget::SetSortMode(EMKHBagSortMode InSortMode)
{
	SortMode = InSortMode;
	ApplySort();
}

// ============================================================
// Protected / Internal Logic (drag & drop)
// ============================================================

void UMKHInventoryBagWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	// A quick-slotted weapon keeps the current highlight, as before the move to C++.
	switch (ClassifyBagReturn(UMKHInventoryEntryDragOperation::GetDraggedItem(InOperation)))
	{
	case EMKHBagReturn::Armor:
	case EMKHBagReturn::Consumable:
		SetDropHighlight(true);
		break;
	case EMKHBagReturn::None:
		SetDropHighlight(false);
		break;
	default:
		break;
	}
}

bool UMKHInventoryBagWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetDropHighlight(false);

	UInventoryDashboardWidget* OwningDashboard = GetDashboard();
	const UInventoryItem* Item = UMKHInventoryEntryDragOperation::GetDraggedItem(InOperation);
	const EMKHBagReturn Return = ClassifyBagReturn(Item);
	if (!IsValid(OwningDashboard) || Return == EMKHBagReturn::None)
	{
		return false;
	}

	if (Return == EMKHBagReturn::Armor)
	{
		OwningDashboard->RequestUnequipItem(Item->GetSlotTag());
	}
	else
	{
		OwningDashboard->RequestRemoveFromQuickSlot(Item->GetItemID());
	}
	return true;
}

// ============================================================
// Private helpers
// ============================================================

void UMKHInventoryBagWidget::HandleEntryGenerated(UUserWidget& EntryWidget) const
{
	if (UMKHInventoryEntryWidget* Entry = Cast<UMKHInventoryEntryWidget>(&EntryWidget))
	{
		Entry->SetDashboard(GetDashboard());
	}
}

void UMKHInventoryBagWidget::HandleEntryReleased(UUserWidget& EntryWidget) const
{
	if (UInventoryDashboardWidget* OwningDashboard = GetDashboard())
	{
		OwningDashboard->CloseActionMenu();
	}
}

void UMKHInventoryBagWidget::HandleItemDoubleClicked(UObject* ListItem) const
{
	UInventoryDashboardWidget* OwningDashboard = GetDashboard();
	UInventoryItem* Item = Cast<UInventoryItem>(ListItem);
	if (!IsValid(OwningDashboard) || !IsValid(Item))
	{
		return;
	}

	if (Item->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Armor))
	{
		OwningDashboard->RequestUseItem(Item->GetItemID(), 1);
	}
	else
	{
		OwningDashboard->RequestAddToQuickSlot(Item->GetItemID(), OwningDashboard->FindQuickSlotForItem(Item));
	}
}

void UMKHInventoryBagWidget::ApplySort()
{
	if (SortMode == EMKHBagSortMode::None || !IsValid(InventorySlots))
	{
		return;
	}

	TArray<TPair<FMKHBagSortKey, UInventoryItem*>> SortedItems;
	SortedItems.Reserve(ItemsMap.Num());
	for (const TPair<int64, TObjectPtr<UInventoryItem>>& Pair : ItemsMap)
	{
		if (IsValid(Pair.Value))
		{
			SortedItems.Emplace(MakeSortKey(*Pair.Value), Pair.Value.Get());
		}
	}

	const EMKHBagSortMode Mode = SortMode;
	SortedItems.Sort([Mode](const TPair<FMKHBagSortKey, UInventoryItem*>& A, const TPair<FMKHBagSortKey, UInventoryItem*>& B)
	{
		return FMKHBagSorting::IsBefore(A.Key, B.Key, Mode);
	});

	TArray<UInventoryItem*> Rows;
	Rows.Reserve(SortedItems.Num());
	for (const TPair<FMKHBagSortKey, UInventoryItem*>& Sorted : SortedItems)
	{
		Rows.Add(Sorted.Value);
	}
	InventorySlots->SetListItems(Rows);
}

FMKHBagSortKey UMKHInventoryBagWidget::MakeSortKey(const UInventoryItem& Item) const
{
	const UMKHDataSubsystem* Data = UMKHDataSubsystem::Get(this);

	FMKHBagSortKey Key;
	Key.CategoryRank = FMKHBagSorting::GetCategoryRank(Item.GetItemTag());
	Key.RarityRank = IsValid(Data) ? Data->GetRarityRank(Item.GetRarityTag()) : INDEX_NONE;
	Key.ItemID = Item.GetItemID();
	return Key;
}
