// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventoryEntryWidget.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/MKHDataSubsystem.h"
#include "Equipment/Rarity/RarityDefinition.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "Materials/MaterialInstance.h"
#include "MKHLogChannels.h"
#include "UI/HUD/Inventory/InventoryDashboardWidget.h"
#include "UI/HUD/Inventory/MKHDropHighlightWidget.h"
#include "UI/HUD/Inventory/MKHInventoryEntryDragOperation.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHInventoryEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The item tooltip needs no designer binding: the entry itself builds it (FindToolTip reads this delegate too).
	if (!ToolTipWidgetDelegate.IsBound())
	{
		ToolTipWidgetDelegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UMKHInventoryEntryWidget, GetToolTipWidget));
	}
}

void UMKHInventoryEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!bIsInit)
	{
		ClearEntry();
	}
}

void UMKHInventoryEntryWidget::NativeDestruct()
{
	if (UInventoryDashboardWidget* OwningDashboard = Dashboard.Get())
	{
		OwningDashboard->ClearHoveredEntry(this);
	}
	UnbindFromItem();
	Super::NativeDestruct();
}

void UMKHInventoryEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	InitEntry(Cast<UInventoryItem>(ListItemObject));
	bIsEquipEntry = false;
}

// ============================================================
// Public Interface
// ============================================================

void UMKHInventoryEntryWidget::InitEntry(UInventoryItem* InItem)
{
	if (!IsValid(InItem))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHInventoryEntryWidget::InitEntry: invalid item on %s."), *GetName());
		return;
	}

	UnbindFromItem();
	InventoryItem = InItem;
	InventoryItem->OnItemUpdatedDelegate.AddUniqueDynamic(this, &ThisClass::HandleItemUpdated);
	RefreshEntry();
	bIsInit = true;
	if (UInventoryDashboardWidget* OwningDashboard = Dashboard.Get())
	{
		OwningDashboard->HandleEntryContentChanged(this);
	}
}

void UMKHInventoryEntryWidget::ClearEntry()
{
	UInventoryDashboardWidget* OwningDashboard = Dashboard.Get();
	if (IsValid(InventoryItem) && IsValid(OwningDashboard))
	{
		OwningDashboard->CloseActionMenu();
	}

	UnbindFromItem();
	bIsInit = false;
	InventoryItem = nullptr;
	ApplyEmptyLook();
	if (IsValid(OwningDashboard))
	{
		OwningDashboard->HandleEntryContentChanged(this);
	}
}

void UMKHInventoryEntryWidget::RefreshEntry()
{
	if (!IsValid(InventoryItem) || !IsValid(SlotIcon))
	{
		return;
	}

	UTexture2D* WeaponHudIcon = bUseWeaponHudIcon ? InventoryItem->GetWeaponHudIcon() : nullptr;
	SlotIcon->SetBrushFromTexture(IsValid(WeaponHudIcon) ? WeaponHudIcon : InventoryItem->GetIcon());
	SlotIcon->SetBrushTintColor(FSlateColor(FLinearColor::White));

	FRarityDefinition Rarity;
	UMKHDataSubsystem::FindRarityDefinitionForContext(this, InventoryItem->GetRarityTag(), Rarity);
	if (IsValid(SlotBg))
	{
		SlotBg->SetBrushTintColor(FSlateColor(FLinearColor::White));
		SlotBg->SetBrushFromMaterial(Rarity.RarityColors.MI_Banner);
	}
	if (IsValid(SlotFrame))
	{
		SlotFrame->SetBrushTintColor(FSlateColor(Rarity.RarityColors.MainColor));
	}

	SetQuantity(InventoryItem->GetQuantity());
}

void UMKHInventoryEntryWidget::SetDashboard(UInventoryDashboardWidget* InDashboard)
{
	Dashboard = InDashboard;
}

int64 UMKHInventoryEntryWidget::GetItemID() const
{
	return IsValid(InventoryItem) ? InventoryItem->GetItemID() : 0;
}

void UMKHInventoryEntryWidget::SetContentHidden(bool bHidden)
{
	if (IsValid(EntryContent))
	{
		EntryContent->SetVisibility(bHidden ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
	}
}

UWidget* UMKHInventoryEntryWidget::GetToolTipWidget() const
{
	UInventoryDashboardWidget* OwningDashboard = Dashboard.Get();
	if (!IsValid(InventoryItem) || !IsValid(OwningDashboard) || OwningDashboard->IsActionMenuOpen()
		|| !UMKHMenuNavigationLibrary::IsGamepadNavigationActive(this))
	{
		return nullptr;
	}
	return OwningDashboard->CreateItemToolTip(InventoryItem);
}

UWidget* UMKHInventoryEntryWidget::GetToolTipAnchor()
{
	return IsValid(SlotFrame) ? static_cast<UWidget*>(SlotFrame) : this;
}

// ============================================================
// Protected / Internal Logic (input)
// ============================================================

void UMKHInventoryEntryWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (UInventoryDashboardWidget* OwningDashboard = Dashboard.Get())
	{
		OwningDashboard->SetHoveredEntry(this);
	}
}

void UMKHInventoryEntryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (UInventoryDashboardWidget* OwningDashboard = Dashboard.Get())
	{
		OwningDashboard->ClearHoveredEntry(this);
	}
}

FReply UMKHInventoryEntryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryDashboardWidget* OwningDashboard = Dashboard.Get();
	if (IsValid(OwningDashboard))
	{
		OwningDashboard->CloseActionMenu();
	}
	if (!bIsInit || !IsValid(OwningDashboard))
	{
		return FReply::Unhandled();
	}

	const bool bLeftButton = InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
	if (InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		OwningDashboard->OpenActionMenu(this);
		return FReply::Handled();
	}
	if (bLeftButton && InMouseEvent.IsControlDown())
	{
		OwningDashboard->ConfirmDropItem(GetItemID(), -1);
		return FReply::Handled();
	}
	if (bLeftButton && InMouseEvent.IsShiftDown() && OwningDashboard->CanSplitItem(InventoryItem))
	{
		OwningDashboard->RequestSplitItem(InventoryItem);
		return FReply::Handled();
	}
	if (bLeftButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return FReply::Unhandled();
}

FReply UMKHInventoryEntryWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bIsInit || !InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Unhandled();
	}

	QuickEquip();
	return FReply::Handled();
}

FReply UMKHInventoryEntryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	UInventoryDashboardWidget* OwningDashboard = Dashboard.Get();
	if (bIsInit && IsValid(OwningDashboard) && UMKHMenuNavigationLibrary::IsGamepadAcceptKey(InKeyEvent))
	{
		OwningDashboard->OpenActionMenu(this);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMKHInventoryEntryWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (!IsValid(InventoryItem))
	{
		return;
	}
	if (UInventoryDashboardWidget* OwningDashboard = Dashboard.Get())
	{
		OwningDashboard->ClearHoveredEntry(this);
	}

	UMKHInventoryEntryDragOperation* Operation = NewObject<UMKHInventoryEntryDragOperation>(this);
	Operation->InitEntryDrag(this, InventoryItem);
	Operation->Pivot = EDragPivot::CenterCenter;
	if (UInventoryDashboardWidget* OwningDashboard = Dashboard.Get())
	{
		Operation->DefaultDragVisual = OwningDashboard->CreateItemDragVisual(InventoryItem);
	}
	OutOperation = Operation;
}

void UMKHInventoryEntryWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	const UInventoryDashboardWidget* OwningDashboard = Dashboard.Get();
	if (!IsValid(OwningDashboard))
	{
		SetDropHighlight(false);
		return;
	}

	// A weapon whose type already sits in the other weapon slot goes there: this slot does not light up.
	const UInventoryItem* Item = GetQuickSlottableItem(InOperation);
	const bool bTakesItem = IsValid(Item) && OwningDashboard->ResolveQuickSlotDropTarget(Item, SlotTag).MatchesTagExact(SlotTag);
	SetDropHighlight(bTakesItem || OwningDashboard->CanDropSplitOnQuickSlot(InOperation, SlotTag));
}

void UMKHInventoryEntryWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	SetDropHighlight(false);
}

bool UMKHInventoryEntryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryDashboardWidget* OwningDashboard = Dashboard.Get();
	if (!IsValid(OwningDashboard))
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	// The drop ends here: the equipment panels never get their OnDrop to clear their own highlight.
	if (bIsEquipEntry)
	{
		OwningDashboard->ClearEquipmentDropHighlights();
	}
	SetDropHighlight(false);

	if (OwningDashboard->TryDropSplitOnQuickSlot(InOperation, SlotTag))
	{
		return true;
	}

	const UInventoryItem* Item = GetQuickSlottableItem(InOperation);
	if (IsValid(Item))
	{
		OwningDashboard->RequestAddToQuickSlot(Item->GetItemID(), OwningDashboard->ResolveQuickSlotDropTarget(Item, SlotTag));
	}
	return IsValid(Item);
}

void UMKHInventoryEntryWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
	SetDropHighlight(false);

	if (UInventoryDashboardWidget* OwningDashboard = Dashboard.Get())
	{
		OwningDashboard->TryDropDraggedItemOutside(InventoryItem, InDragDropEvent);
	}
}

// ============================================================
// Private helpers
// ============================================================

void UMKHInventoryEntryWidget::HandleItemUpdated()
{
	RefreshEntry();
}

void UMKHInventoryEntryWidget::UnbindFromItem()
{
	if (IsValid(InventoryItem))
	{
		InventoryItem->OnItemUpdatedDelegate.RemoveAll(this);
	}
}

void UMKHInventoryEntryWidget::QuickEquip() const
{
	UInventoryDashboardWidget* OwningDashboard = Dashboard.Get();
	if (!IsValid(OwningDashboard) || !IsValid(InventoryItem))
	{
		return;
	}

	const int64 ItemID = InventoryItem->GetItemID();
	if (InventoryItem->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Armor))
	{
		if (bIsEquipEntry)
		{
			OwningDashboard->RequestUnequipItem(InventoryItem->GetSlotTag());
		}
		else
		{
			OwningDashboard->RequestUseItem(ItemID, 1);
		}
	}
	else if (bIsQuickSlot)
	{
		OwningDashboard->RequestRemoveFromQuickSlot(ItemID);
	}
	else
	{
		OwningDashboard->RequestAddToQuickSlot(ItemID, OwningDashboard->FindQuickSlotForItem(InventoryItem));
	}
}

UInventoryItem* UMKHInventoryEntryWidget::GetQuickSlottableItem(const UDragDropOperation* Operation) const
{
	if (!bIsQuickSlot)
	{
		return nullptr;
	}

	UInventoryItem* Item = UMKHInventoryEntryDragOperation::GetDraggedItem(Operation);
	const FGameplayTag& AcceptedCategory = bIsEquipEntry
		? MKHGameplayTags::Equip::Category_Weapon.GetTag()
		: MKHGameplayTags::Equip::Category_Consumable.GetTag();
	const bool bAccepted = IsValid(Item) && !Item->IsEquipped() && Item->GetItemTag().MatchesTag(AcceptedCategory);
	return bAccepted ? Item : nullptr;
}

void UMKHInventoryEntryWidget::ApplyEmptyLook()
{
	const FSlateColor Transparent(FLinearColor(1.f, 1.f, 1.f, 0.f));
	if (IsValid(SlotIcon))
	{
		SlotIcon->SetBrushFromTexture(nullptr);
		SlotIcon->SetBrushTintColor(Transparent);
	}
	if (IsValid(SlotBg))
	{
		SlotBg->SetBrushTintColor(Transparent);
	}
	if (IsValid(SlotFrame))
	{
		SlotFrame->SetBrushTintColor(FSlateColor(EmptyFrameTint));
	}
	SetQuantity(0);
}

void UMKHInventoryEntryWidget::SetQuantity(int32 Quantity)
{
	if (IsValid(QuantityText))
	{
		QuantityText->SetVisibility(Quantity > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		QuantityText->SetText(FText::AsNumber(Quantity));
	}
}

void UMKHInventoryEntryWidget::SetDropHighlight(bool bActive) const
{
	if (IsValid(DropHighlight))
	{
		DropHighlight->SetHighlightActive(bActive);
	}
}
