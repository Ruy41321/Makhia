// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/Inventory/InventoryDashboardWidget.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Audio/MKHSoundEffectSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "MKHLogChannels.h"
#include "Slate/SObjectWidget.h"
#include "Slate/UMGDragDropOp.h"
#include "TimerManager.h"
#include "UI/HUD/Inventory/MKHInventoryActionMenuWidget.h"
#include "UI/HUD/Inventory/MKHInventoryBagWidget.h"
#include "UI/HUD/Inventory/MKHInventoryEntryWidget.h"
#include "UI/HUD/Inventory/MKHInventoryEquipmentWidget.h"
#include "UI/HUD/Inventory/MKHInventoryQuickSlotAreaWidget.h"
#include "UI/HUD/Inventory/MKHInventorySortMenuWidget.h"
#include "UI/HUD/Inventory/MKHItemDragVisualWidget.h"
#include "UI/HUD/Inventory/MKHQuantityPopupWidget.h"
#include "UI/HUD/Inventory/MKHSplitStackDragOperation.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"
#include "UI/Navigation/MKHMenuNavigationSubsystem.h"
#include "UI/WidgetControllers/InventoryDashboardController.h"

#define LOCTEXT_NAMESPACE "MKHInventoryDashboard"

// ============================================================
// Lifecycle
// ============================================================

void UInventoryDashboardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	for (UMKHInventoryPanelWidget* Panel : TArray<UMKHInventoryPanelWidget*>{ Bag, Equipment, WeaponLoadout, QuickSlotArea })
	{
		if (IsValid(Panel))
		{
			Panel->SetDashboard(this);
		}
	}

	if (IsValid(SortButton))
	{
		SortButton->OnClicked.AddDynamic(this, &ThisClass::HandleSortButtonClicked);
	}
}

// ============================================================
// View events
// ============================================================

void UInventoryDashboardWidget::OnInventoryItemChanged(UInventoryItem* NewItem)
{
	if (IsValid(Bag))
	{
		Bag->SyncItem(NewItem);
	}
}

void UInventoryDashboardWidget::OnInventoryItemRemoved(const int64 RemovedItemID)
{
	if (IsValid(Bag))
	{
		Bag->RemoveItemByID(RemovedItemID);
	}
}

void UInventoryDashboardWidget::OnItemEquipped(UInventoryItem* EquipItem)
{
	// Weapons show through their quick slot (OnQuickSlotItemRelocated), never through the equipment slot.
	if (IsValid(EquipItem) && IsValid(Equipment) && EquipItem->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Armor))
	{
		Equipment->ShowItem(EquipItem);
	}
}

void UInventoryDashboardWidget::OnItemUnequipped(const FGameplayTag& SlotTag)
{
	if (IsValid(Equipment) && SlotTag.MatchesTag(MKHGameplayTags::Equip::ArmorSlot))
	{
		Equipment->ClearItem(SlotTag, 0);
	}
}

void UInventoryDashboardWidget::OnQuickSlotItemRelocated(UInventoryItem* QuickSlottedItem)
{
	if (!IsValid(QuickSlottedItem))
	{
		return;
	}

	if (IsValid(Bag))
	{
		Bag->HandleQuickSlotRelocated(QuickSlottedItem);
	}
	if (QuickSlottedItem->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Weapon))
	{
		if (IsValid(WeaponLoadout))
		{
			WeaponLoadout->HandleQuickSlotRelocated(QuickSlottedItem);
		}
	}
	else if (IsValid(QuickSlotArea))
	{
		QuickSlotArea->HandleQuickSlotRelocated(QuickSlottedItem);
	}
}

void UInventoryDashboardWidget::OnQuickSlotItemChanged(UInventoryItem* QuickSlottedItem)
{
	if (!IsValid(QuickSlottedItem))
	{
		return;
	}

	if (QuickSlottedItem->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Weapon))
	{
		if (IsValid(WeaponLoadout))
		{
			WeaponLoadout->ShowItem(QuickSlottedItem);
		}
	}
	else if (IsValid(QuickSlotArea))
	{
		QuickSlotArea->SyncItem(QuickSlottedItem);
	}
}

void UInventoryDashboardWidget::OnQuickSlotItemRemoved(const int64 RemovedItemID)
{
	if (IsValid(QuickSlotArea))
	{
		QuickSlotArea->RemoveItemByID(RemovedItemID);
	}
}

// ============================================================
// Open / close
// ============================================================

void UInventoryDashboardWidget::OpenInventoryDashboard()
{
	SetVisibility(ESlateVisibility::Visible);
	bIsOpen = true;

	// First opening only: afterwards the bag keeps the player's criterion.
	if (IsValid(Bag) && Bag->GetSortMode() == EMKHBagSortMode::None)
	{
		Bag->SetSortMode(EMKHBagSortMode::Category);
	}

	OnOpenInventoryDashboardDelegate.Broadcast();
}

void UInventoryDashboardWidget::CloseInventoryDashboard()
{
	CloseTransientUI();
	SetVisibility(ESlateVisibility::Collapsed);
	bIsOpen = false;
	OnCloseInventoryDashboardDelegate.Broadcast();
}

void UInventoryDashboardWidget::DestroyInventoryDashboard()
{
	CloseTransientUI();
	bIsOpen = false;
	RemoveFromParent();
	OnDestructionInventoryDashboardDelegate.Broadcast();
}

void UInventoryDashboardWidget::UnbindAllEventsFromDelegates()
{
	OnOpenInventoryDashboardDelegate.Clear();
	OnCloseInventoryDashboardDelegate.Clear();
	OnDestructionInventoryDashboardDelegate.Clear();
}

void UInventoryDashboardWidget::CloseTransientUI()
{
	CloseActionMenu();
	CloseSortMenu();
	CloseQuantityPopup();
	CancelSplitDrag();
	HoveredEntry.Reset();
	RefreshHoverToolTip();
}

// ============================================================
// Requests
// ============================================================

void UInventoryDashboardWidget::RequestUseItem(int64 ItemID, int32 Quantity)
{
	UInventoryDashboardController* Controller = GetDashboardController();
	if (!IsValid(Controller))
	{
		return;
	}

	// Read before the use: a used item may leave the bag.
	const bool bIsEquipment = Controller->GetItemTag(ItemID).MatchesTag(MKHGameplayTags::Equip::Category_Equipment);
	Controller->UseItem(ItemID, Quantity);
	if (bIsEquipment)
	{
		PlayUISound(MKHGameplayTags::SFX::UIEquip);
	}
}

void UInventoryDashboardWidget::RequestUnequipItem(const FGameplayTag& EquipmentSlotTag)
{
	if (UInventoryDashboardController* Controller = GetDashboardController())
	{
		Controller->UnequipItem(EquipmentSlotTag);
		PlayUISound(MKHGameplayTags::SFX::UIUnequip);
	}
}

void UInventoryDashboardWidget::PlayUISound(const FGameplayTag& SoundTag) const
{
	if (UMKHSoundEffectSubsystem* SoundEffects = UMKHSoundEffectSubsystem::Get(this))
	{
		SoundEffects->PlayUISound(SoundTag, this);
	}
}

void UInventoryDashboardWidget::RequestSwapWeaponSkills(int64 ItemID, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot)
{
	if (UInventoryDashboardController* Controller = GetDashboardController())
	{
		Controller->SwapWeaponSkills(ItemID, FromSlot, ToSlot);
	}
}

void UInventoryDashboardWidget::RequestAddToQuickSlot(int64 ItemID, const FGameplayTag& QuickSlotTag)
{
	UInventoryDashboardController* Controller = GetDashboardController();
	if (!IsValid(Controller) || !QuickSlotTag.IsValid())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget::RequestAddToQuickSlot: no controller or no quick slot for item %lld."), ItemID);
		return;
	}
	Controller->AddToQuickSlot(ItemID, QuickSlotTag);
}

void UInventoryDashboardWidget::RequestRemoveFromQuickSlot(int64 ItemID)
{
	if (UInventoryDashboardController* Controller = GetDashboardController())
	{
		Controller->RemoveFromQuickSlot(ItemID);
	}
}

FGameplayTag UInventoryDashboardWidget::FindQuickSlotForItem(const UInventoryItem* Item) const
{
	UInventoryDashboardController* Controller = GetDashboardController();
	return (IsValid(Item) && IsValid(Controller)) ? Controller->FindQuickSlotForItem(*Item) : FGameplayTag();
}

FGameplayTag UInventoryDashboardWidget::ResolveQuickSlotDropTarget(const UInventoryItem* Item, const FGameplayTag& QuickSlotTag) const
{
	UInventoryDashboardController* Controller = GetDashboardController();
	if (!IsValid(Item) || !IsValid(Controller) || !Item->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Weapon))
	{
		return QuickSlotTag;
	}

	const FGameplayTag SameTypeSlot = Controller->FindWeaponQuickSlotOfSameType(*Item);
	return SameTypeSlot.IsValid() ? SameTypeSlot : QuickSlotTag;
}

void UInventoryDashboardWidget::ConfirmDropItem(int64 ItemID, int32 Quantity)
{
	if (UInventoryDashboardController* Controller = GetDashboardController())
	{
		Controller->DropItem(ItemID, Quantity);
	}
}

// ============================================================
// Action menu and item visuals
// ============================================================

void UInventoryDashboardWidget::OpenActionMenu(UMKHInventoryEntryWidget* Entry)
{
	CloseActionMenu();
	if (!IsValid(Entry) || !Entry->IsEntryInitialized())
	{
		return;
	}
	if (!ActionMenuClass)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget: ActionMenuClass not set on %s."), *GetClass()->GetName());
		return;
	}

	ActionMenu = CreateWidget<UMKHInventoryActionMenuWidget>(GetOwningPlayer(), ActionMenuClass);
	if (!IsValid(ActionMenu))
	{
		return;
	}
	ActionMenu->InitActionMenu(Entry);
	ActionMenu->AddToViewport(ActionMenuZOrder);
	RefreshHoverToolTip();
}

void UInventoryDashboardWidget::CloseActionMenu()
{
	UWidgetBlueprintLibrary::CancelDragDrop();
	if (!IsValid(ActionMenu))
	{
		return;
	}

	ActionMenu->RemoveFromParent();
	ActionMenu = nullptr;
	RefreshHoverToolTip();
}

void UInventoryDashboardWidget::OpenSortMenu()
{
	CloseActionMenu();
	CloseSortMenu();
	if (!SortMenuClass || !IsValid(SortButton))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget: SortMenuClass or SortButton not set on %s."), *GetClass()->GetName());
		return;
	}

	SortMenu = CreateWidget<UMKHInventorySortMenuWidget>(GetOwningPlayer(), SortMenuClass);
	if (IsValid(SortMenu))
	{
		SortMenu->InitSortMenu(this, SortButton);
		SortMenu->AddToViewport(ActionMenuZOrder);
	}
}

void UInventoryDashboardWidget::CloseSortMenu()
{
	if (IsValid(SortMenu))
	{
		SortMenu->RemoveFromParent();
	}
	SortMenu = nullptr;
}

void UInventoryDashboardWidget::RequestBagSort(EMKHBagSortMode SortMode)
{
	if (IsValid(Bag))
	{
		Bag->SetSortMode(SortMode);
	}
	CloseSortMenu();
}

EMKHBagSortMode UInventoryDashboardWidget::GetBagSortMode() const
{
	return IsValid(Bag) ? Bag->GetSortMode() : EMKHBagSortMode::None;
}

void UInventoryDashboardWidget::HandleSortButtonClicked()
{
	OpenSortMenu();
}

void UInventoryDashboardWidget::SetHoveredEntry(UMKHInventoryEntryWidget* Entry)
{
	HoveredEntry = Entry;
	RefreshHoverToolTip();
}

void UInventoryDashboardWidget::ClearHoveredEntry(const UMKHInventoryEntryWidget* Entry)
{
	if (HoveredEntry.Get() == Entry)
	{
		HoveredEntry.Reset();
		RefreshHoverToolTip();
	}
}

void UInventoryDashboardWidget::HandleEntryContentChanged(const UMKHInventoryEntryWidget* Entry)
{
	if (HoveredEntry.Get() == Entry)
	{
		RefreshHoverToolTip();
	}
}

void UInventoryDashboardWidget::RefreshHoverToolTip()
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UMKHMenuNavigationSubsystem* NavigationSubsystem = IsValid(LocalPlayer) ? LocalPlayer->GetSubsystem<UMKHMenuNavigationSubsystem>() : nullptr;
	if (!IsValid(NavigationSubsystem))
	{
		return;
	}

	UMKHInventoryEntryWidget* Entry = HoveredEntry.Get();
	const bool bShow = IsValid(Entry) && Entry->IsEntryInitialized() && bIsOpen && !IsActionMenuOpen()
		&& !UWidgetBlueprintLibrary::IsDragDropping();
	if (bShow)
	{
		NavigationSubsystem->ShowAnchoredToolTip(CreateItemToolTip(Entry->GetInventoryItem()), Entry->GetToolTipAnchor());
	}
	else
	{
		NavigationSubsystem->HideAnchoredToolTip();
	}
}

UWidget* UInventoryDashboardWidget::CreateItemDragVisual(UInventoryItem* Item)
{
	if (!DragVisualClass)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget: DragVisualClass not set on %s."), *GetClass()->GetName());
		return nullptr;
	}

	UMKHItemDragVisualWidget* DragVisual = CreateWidget<UMKHItemDragVisualWidget>(GetOwningPlayer(), DragVisualClass);
	if (IsValid(DragVisual))
	{
		DragVisual->SetItem(Item);
	}
	return DragVisual;
}

UWidget* UInventoryDashboardWidget::CreateItemToolTip(UInventoryItem* Item)
{
	return IsValid(Item) ? MakeItemToolTip(Item) : nullptr;
}

// ============================================================
// Drop, split, quantity popup
// ============================================================

void UInventoryDashboardWidget::RequestDropItem(UInventoryItem* Item)
{
	if (!IsValid(Item))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget::RequestDropItem: invalid item."));
		return;
	}

	CloseQuantityPopup();

	const bool bIsStack = Item->GetQuantity() > 1
		&& !Item->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Equipment);
	if (bIsStack && OpenQuantityPopup(*Item, EMKHQuantityPopupAction::Drop))
	{
		return;
	}

	ConfirmDropItem(Item->GetItemID(), -1);
}

bool UInventoryDashboardWidget::TryDropDraggedItemOutside(UInventoryItem* Item, const FPointerEvent& PointerEvent)
{
	if (!IsValid(Item) || !IsReleasedOutsideFrame(PointerEvent))
	{
		return false;
	}

	CloseQuantityPopup();
	ConfirmDropItem(Item->GetItemID(), -1);
	return true;
}

bool UInventoryDashboardWidget::CanSplitItem(const UInventoryItem* Item) const
{
	return IsValid(Item) && Item->GetQuantity() > 1
		&& Item->GetItemTag().MatchesTag(MKHGameplayTags::Equip::Category_Consumable);
}

void UInventoryDashboardWidget::RequestSplitItem(UInventoryItem* Item)
{
	if (!CanSplitItem(Item))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget::RequestSplitItem: '%s' cannot be split."),
			IsValid(Item) ? *Item->GetItemTag().ToString() : TEXT("None"));
		return;
	}

	CloseQuantityPopup();
	PendingSplitSource = Item;
	OpenQuantityPopup(*Item, EMKHQuantityPopupAction::Split);
}

bool UInventoryDashboardWidget::CanDropSplitOnQuickSlot(const UDragDropOperation* Operation, const FGameplayTag& QuickSlotTag) const
{
	return IsValid(Cast<UMKHSplitStackDragOperation>(Operation))
		&& QuickSlotTag.MatchesTag(MKHGameplayTags::Input::ConsumableQuickSlotCategory);
}

bool UInventoryDashboardWidget::TryDropSplitOnQuickSlot(UDragDropOperation* Operation, const FGameplayTag& QuickSlotTag)
{
	if (!CanDropSplitOnQuickSlot(Operation, QuickSlotTag))
	{
		return false;
	}

	UInventoryDashboardController* Controller = GetDashboardController();
	if (!IsValid(Controller))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget::TryDropSplitOnQuickSlot: no dashboard controller."));
		return false;
	}

	const UMKHSplitStackDragOperation* Split = CastChecked<UMKHSplitStackDragOperation>(Operation);
	Controller->SplitStackToQuickSlot(Split->GetItemID(), Split->GetQuantity(), QuickSlotTag);
	return true;
}

void UInventoryDashboardWidget::CloseQuantityPopup()
{
	if (IsValid(QuantityPopup))
	{
		QuantityPopup->RemoveFromParent();
	}
	QuantityPopup = nullptr;
}

bool UInventoryDashboardWidget::OpenQuantityPopup(const UInventoryItem& Item, EMKHQuantityPopupAction Action)
{
	if (!QuantityPopupClass)
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget: QuantityPopupClass not set on %s."), *GetClass()->GetName());
		return false;
	}

	QuantityPopup = CreateWidget<UMKHQuantityPopupWidget>(GetOwningPlayer(), QuantityPopupClass);
	if (!IsValid(QuantityPopup))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget: could not create the quantity popup."));
		return false;
	}

	PendingQuantityAction = Action;
	const FText ConfirmLabel = Action == EMKHQuantityPopupAction::Split
		? NSLOCTEXT("MKHInventoryDashboard", "SplitConfirm", "Split")
		: NSLOCTEXT("MKHInventoryDashboard", "DropConfirm", "Drop");

	QuantityPopup->OnQuantityConfirmed.AddDynamic(this, &ThisClass::HandleQuantityConfirmed);
	QuantityPopup->AddToViewport(QuantityPopupZOrder);
	QuantityPopup->InitQuantityPopup(&Item, ConfirmLabel);
	return true;
}

FReply UInventoryDashboardWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bIsOpen && UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent))
	{
		CloseInventoryDashboard();
		return FReply::Handled();
	}
	if (bIsOpen && InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Top)
	{
		OpenSortMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UInventoryDashboardWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		CloseInventoryDashboard();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UInventoryDashboardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	CloseActionMenu();
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventoryDashboardWidget::HandleQuantityConfirmed(int64 ItemID, int32 Quantity)
{
	if (PendingQuantityAction == EMKHQuantityPopupAction::Drop)
	{
		ConfirmDropItem(ItemID, Quantity);
		return;
	}

	UInventoryDashboardController* Controller = GetDashboardController();
	if (UMKHMenuNavigationLibrary::IsGamepadNavigationActive(this) && IsValid(Controller))
	{
		Controller->SplitStackToQuickSlot(ItemID, Quantity, Controller->FindConsumableQuickSlotForSplit());
		return;
	}

	// Started next tick: the confirm click is still being processed and a drag cannot begin inside it.
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Quantity]()
	{
		BeginSplitDrag(PendingSplitSource.Get(), Quantity);
	}));
}

void UInventoryDashboardWidget::BeginSplitDrag(const UInventoryItem* SourceItem, int32 Quantity)
{
	if (!IsValid(SourceItem))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget::BeginSplitDrag: the split stack is gone."));
		return;
	}

	FRPGInventoryEntry PreviewEntry;
	PreviewEntry.Item = SourceItem->GetItemEntry();
	PreviewEntry.Item.Quantity = Quantity;
	UInventoryItem* PreviewItem = UInventoryItem::CreateFromInventoryEntry(this, PreviewEntry);

	UMKHSplitStackDragOperation* Operation = NewObject<UMKHSplitStackDragOperation>(this);
	Operation->InitSplit(SourceItem->GetItemID(), Quantity);
	Operation->Payload = PreviewItem;
	Operation->Tag = FString::Printf(TEXT("%s x%d"), *SourceItem->GetItemName().ToString(), Quantity);
	Operation->DefaultDragVisual = CreateItemDragVisual(PreviewItem);
	Operation->Pivot = EDragPivot::CenterCenter;

	if (!StartDragDropAtCursor(Operation))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget::BeginSplitDrag: could not start the drag."));
	}
}

bool UInventoryDashboardWidget::StartDragDropAtCursor(UDragDropOperation* Operation)
{
	const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
	if (!IsValid(Operation) || !FSlateApplication::IsInitialized() || !CachedWidget.IsValid()
		|| CachedWidget->GetType() != FName(TEXT("SObjectWidget")))
	{
		return false;
	}

	FSlateApplication& Slate = FSlateApplication::Get();
	if (Slate.IsDragDropping())
	{
		Slate.CancelDragDrop();
	}

	const FVector2D CursorPosition = Slate.GetCursorPos();
	const FWidgetPath PathUnderCursor = Slate.LocateWindowUnderMouse(CursorPosition, Slate.GetInteractiveTopLevelWindows());
	if (!PathUnderCursor.IsValid())
	{
		return false;
	}

	// Same reply a DragDetected handler returns; the next mouse release drops the operation where the cursor is.
	const int32 UserIndex = Slate.GetUserIndexForMouse();
	const FPointerEvent PointerEvent(UserIndex, FSlateApplication::CursorPointerIndex, CursorPosition, CursorPosition,
		Slate.GetPressedMouseButtons(), EKeys::Invalid, 0.f, Slate.GetModifierKeys());
	const TSharedRef<FUMGDragDropOp> DragDropOp = FUMGDragDropOp::New(Operation, FSlateApplication::CursorPointerIndex,
		CursorPosition, CursorPosition, UWidgetLayoutLibrary::GetViewportScale(this), StaticCastSharedPtr<SObjectWidget>(CachedWidget));
	Slate.ProcessReply(PathUnderCursor, FReply::Handled().BeginDragDrop(DragDropOp), &PathUnderCursor, &PointerEvent, UserIndex);
	return Slate.IsDragDropping();
}

void UInventoryDashboardWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

	const UMKHSplitStackDragOperation* Split = Cast<UMKHSplitStackDragOperation>(InOperation);
	if (IsValid(Split) && IsReleasedOutsideFrame(InDragDropEvent))
	{
		ConfirmDropItem(Split->GetItemID(), Split->GetQuantity());
	}
}

bool UInventoryDashboardWidget::IsReleasedOutsideFrame(const FPointerEvent& PointerEvent) const
{
	if (PointerEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return false;
	}

	if (!IsValid(InventoryFrame))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UInventoryDashboardWidget: no InventoryFrame widget, dragging out of the inventory does not drop."));
		return false;
	}
	return !InventoryFrame->GetCachedGeometry().IsUnderLocation(PointerEvent.GetScreenSpacePosition());
}

void UInventoryDashboardWidget::CancelSplitDrag() const
{
	if (IsValid(Cast<UMKHSplitStackDragOperation>(UWidgetBlueprintLibrary::GetDragDroppingContent())))
	{
		UWidgetBlueprintLibrary::CancelDragDrop();
	}
}

FGameplayTag UInventoryDashboardWidget::GetGamepadQuickSlotChoice(const UInventoryItem* Item, int32 ChoiceIndex) const
{
	UInventoryDashboardController* Controller = GetDashboardController();
	if (!IsValid(Item) || !IsValid(Controller) || !UMKHMenuNavigationLibrary::IsGamepadNavigationActive(this))
	{
		return FGameplayTag();
	}

	const TArray<FGameplayTag> Choices = Controller->FindQuickSlotChoices(*Item);
	return Choices.IsValidIndex(ChoiceIndex) ? Choices[ChoiceIndex] : FGameplayTag();
}

bool UInventoryDashboardWidget::ShowsGenericQuickSlotButton(const UInventoryItem* Item) const
{
	return IsValid(Item) && (Item->IsQuickSlotted() || !UMKHMenuNavigationLibrary::IsGamepadNavigationActive(this));
}

FText UInventoryDashboardWidget::GetQuickSlotChoiceLabel(const FGameplayTag& QuickSlotTag)
{
	using namespace MKHGameplayTags::Equip;

	int32 SlotNumber = 1;
	if (QuickSlotTag == WeaponQuickSlot2 || QuickSlotTag == ConsumableQuickSlot2)
	{
		SlotNumber = 2;
	}
	else if (QuickSlotTag == ConsumableQuickSlot3)
	{
		SlotNumber = 3;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("SlotNumber"), FText::AsNumber(SlotNumber));
	return FText::Format(LOCTEXT("QuickSlotChoice", "QuickSlot {SlotNumber}"), Arguments);
}

UInventoryDashboardController* UInventoryDashboardWidget::GetDashboardController() const
{
	return Cast<UInventoryDashboardController>(GetWidgetController());
}

void UInventoryDashboardWidget::ClearEquipmentDropHighlights() const
{
	for (const UMKHInventoryEquipmentWidget* Panel : { Equipment.Get(), WeaponLoadout.Get() })
	{
		if (IsValid(Panel))
		{
			Panel->SetDropHighlight(false);
		}
	}
}

#undef LOCTEXT_NAMESPACE
