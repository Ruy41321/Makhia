// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inventory/MKHBagSorting.h"
#include "UI/MKHSystemWidget.h"
#include "InventoryDashboardWidget.generated.h"

class UButton;
class UDragDropOperation;
class UInventoryDashboardController;
class UInventoryItem;
class UMKHInventoryActionMenuWidget;
class UMKHInventoryBagWidget;
class UMKHInventoryEntryWidget;
class UMKHInventoryEquipmentWidget;
class UMKHInventoryQuickSlotAreaWidget;
class UMKHInventorySortMenuWidget;
class UMKHItemDragVisualWidget;
class UMKHQuantityPopupWidget;
class UWidget;
DECLARE_MULTICAST_DELEGATE(FOnOpenInventoryDashboardSignature);
DECLARE_MULTICAST_DELEGATE(FOnCloseInventoryDashboardSignature);
DECLARE_MULTICAST_DELEGATE(FOnDestructionInventoryDashboardSignature);

/** Action applied with the quantity chosen in the quantity popup. */
UENUM()
enum class EMKHQuantityPopupAction : uint8
{
	/** Drops the chosen quantity on the ground. */
	Drop,
	/** Starts dragging the chosen quantity as a split stack. */
	Split
};

/**
 * The inventory dashboard (W_InventoryDashboard): bag, equipment panel (with the stats area) and consumable quick
 * slots. C++ owns the whole flow: it routes the inventory changes fed by UInventoryDashboardController to its
 * panels, and every player request of its entries, panels and action menu to the controller, which reaches the
 * server-authoritative components. Also owns the action menu, the quantity popup (drop / split) and the split drag.
 * The Blueprint owns the layout and builds the item tooltip (MakeItemToolTip).
 * Full model: Documentation/InventoryDashboard.md.
 */
UCLASS()
class MAKHIA_API UInventoryDashboardWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	// --- View events (bound by UInventoryDashboardController) ---

	/** A bag entry was added or changed. */
	UFUNCTION()
	void OnInventoryItemChanged(UInventoryItem* NewItem);

	/** A bag entry was removed. */
	UFUNCTION()
	void OnInventoryItemRemoved(const int64 RemovedItemID);

	/** An item was equipped (or an equipped entry changed); only armor shows in the equipment panel this way. */
	UFUNCTION()
	void OnItemEquipped(UInventoryItem* EquipItem);

	/** The equipment slot was emptied. */
	UFUNCTION()
	void OnItemUnequipped(const FGameplayTag& SlotTag);

	/** An item entered, left or moved between quick slots (check IsQuickSlotted on the item). */
	UFUNCTION()
	void OnQuickSlotItemRelocated(UInventoryItem* QuickSlottedItem);

	/** A quick-slotted entry changed (e.g. its quantity). */
	UFUNCTION()
	void OnQuickSlotItemChanged(UInventoryItem* QuickSlottedItem);

	/** A quick-slotted entry was removed. */
	UFUNCTION()
	void OnQuickSlotItemRemoved(const int64 RemovedItemID);

	// --- Open / close ---

	/** Shows the dashboard and broadcasts OnOpenInventoryDashboardDelegate. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void OpenInventoryDashboard();

	/** Hides the dashboard (action menu, quantity popup and split drag close) and broadcasts OnCloseInventoryDashboardDelegate. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseInventoryDashboard();

	/** Removes the dashboard for good and broadcasts OnDestructionInventoryDashboardDelegate. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DestroyInventoryDashboard();

	/** Clears every native open / close / destruction listener. */
	void UnbindAllEventsFromDelegates();

	// --- Requests (entries, panels, action menu) ---

	/**
	 * Uses an item: equips an armor or a weapon (plays SFX.UI.Equip), drinks a consumable.
	 *
	 * @param ItemID   Item to use.
	 * @param Quantity Quantity to use.
	 */
	void RequestUseItem(int64 ItemID, int32 Quantity);

	/**
	 * Unequips the item of an equipment slot (it returns to the bag) and plays SFX.UI.Unequip.
	 *
	 * @param EquipmentSlotTag Equipment slot to empty.
	 */
	void RequestUnequipItem(const FGameplayTag& EquipmentSlotTag);

	/**
	 * Moves a weapon's skill to another skill slot; a skill already there takes the slot left free.
	 *
	 * @param ItemID   Weapon.
	 * @param FromSlot Skill slot holding the skill to move.
	 * @param ToSlot   Destination skill slot.
	 */
	void RequestSwapWeaponSkills(int64 ItemID, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot);

	/**
	 * Assigns an item to a quick slot.
	 *
	 * @param ItemID       Item to assign.
	 * @param QuickSlotTag Target quick slot.
	 */
	void RequestAddToQuickSlot(int64 ItemID, const FGameplayTag& QuickSlotTag);

	/**
	 * Takes an item out of its quick slot.
	 *
	 * @param ItemID Item to remove.
	 */
	void RequestRemoveFromQuickSlot(int64 ItemID);

	/**
	 * Quick slot a "quick slot" action (double click, menu button, drop on the equipment panel) assigns an item to
	 * (UInventoryDashboardController::FindQuickSlotForItem).
	 *
	 * @param Item The item.
	 * @return The quick slot tag, or an empty tag for items that cannot be quick-slotted.
	 */
	FGameplayTag FindQuickSlotForItem(const UInventoryItem* Item) const;

	/**
	 * Quick slot an item dropped on a quick slot entry really goes to: a weapon whose type is already quick-slotted in
	 * another slot replaces that weapon (one weapon per type); anything else goes to the slot it was dropped on.
	 *
	 * @param Item         The dropped item.
	 * @param QuickSlotTag Quick slot of the entry under the cursor.
	 * @return The quick slot to assign.
	 */
	FGameplayTag ResolveQuickSlotDropTarget(const UInventoryItem* Item, const FGameplayTag& QuickSlotTag) const;

	// --- Action menu ---

	/**
	 * Opens the action menu on an entry (closing any other one) and hides the entry's tooltip meanwhile.
	 *
	 * @param Entry The entry; ignored when it shows no item.
	 */
	void OpenActionMenu(UMKHInventoryEntryWidget* Entry);

	/** Cancels the drag in progress and closes the action menu if it is open. */
	void CloseActionMenu();

	/** True while the action menu is open on any entry: no item tooltip shows meanwhile. */
	bool IsActionMenuOpen() const { return ActionMenu != nullptr; }

	// --- Bag sorting ---

	/** Opens the menu of the sort criteria next to SortButton, closing any action menu. */
	void OpenSortMenu();

	/** Closes the sort menu if it is open. */
	void CloseSortMenu();

	/**
	 * Sorts the bag and closes the sort menu.
	 *
	 * @param SortMode The chosen criterion.
	 */
	void RequestBagSort(EMKHBagSortMode SortMode);

	/** Gets the criterion the bag is sorted by. */
	EMKHBagSortMode GetBagSortMode() const;

	// --- Hover tooltip (mouse) ---

	/**
	 * The mouse entered an entry: shows its item tooltip next to it.
	 *
	 * @param Entry The hovered entry.
	 */
	void SetHoveredEntry(UMKHInventoryEntryWidget* Entry);

	/**
	 * The mouse left an entry, or the entry is going away or being dragged: hides its tooltip.
	 *
	 * @param Entry The entry; ignored when it is not the hovered one.
	 */
	void ClearHoveredEntry(const UMKHInventoryEntryWidget* Entry);

	/**
	 * An entry started showing another item or none: the tooltip follows when the entry is the hovered one.
	 *
	 * @param Entry The entry.
	 */
	void HandleEntryContentChanged(const UMKHInventoryEntryWidget* Entry);

	// --- Item visuals ---

	/**
	 * Creates the widget shown under the cursor while dragging an item or a split (DragVisualClass).
	 *
	 * @param Item The dragged item.
	 * @return The drag visual, or nullptr when DragVisualClass is not set.
	 */
	UWidget* CreateItemDragVisual(UInventoryItem* Item);

	/**
	 * Builds the tooltip of an item (MakeItemToolTip, implemented by the Blueprint).
	 *
	 * @param Item The hovered or focused item.
	 * @return The tooltip widget.
	 */
	UWidget* CreateItemToolTip(UInventoryItem* Item);

	/** Removes the drop highlight of the armor and weapon panels. */
	void ClearEquipmentDropHighlights() const;

	// --- Drop ---

	/**
	 * Starts dropping an item from the inventory UI. A consumable stack of more than one item opens the quantity
	 * popup; anything else (or a missing popup class) is dropped whole right away through ConfirmDropItem.
	 *
	 * @param Item The item to drop.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drop")
	void RequestDropItem(UInventoryItem* Item);

	/**
	 * Drops an item on the ground through the owning inventory.
	 *
	 * @param ItemID   Item to drop.
	 * @param Quantity Quantity to drop; -1 drops the whole stack.
	 */
	void ConfirmDropItem(int64 ItemID, int32 Quantity);

	/**
	 * Mouse drag released where no inventory widget took it (called from the entry's OnDragCancelled): outside
	 * InventoryFrame the whole item (whole stack, no quantity popup, like Ctrl+click) is dropped; inside, or for a drag
	 * cancelled by code (Esc, the dashboard closing), nothing happens and the item stays where it was.
	 *
	 * @param Item         The dragged item.
	 * @param PointerEvent The release event (an empty event means the drag was cancelled by code).
	 * @return True when the item is being dropped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drop")
	bool TryDropDraggedItemOutside(UInventoryItem* Item, const FPointerEvent& PointerEvent);

	// --- Split ---

	/** Returns whether an item can be split: a consumable stack of more than one item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Split")
	bool CanSplitItem(const UInventoryItem* Item) const;

	/**
	 * Opens the quantity popup (1..stack) for a split; confirming puts the chosen quantity in a drag operation
	 * attached to the cursor (UMKHSplitStackDragOperation), ready to be dropped on a consumable quick slot.
	 *
	 * @param Item The stack to split; ignored when CanSplitItem is false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Split")
	void RequestSplitItem(UInventoryItem* Item);

	/**
	 * Returns whether a drag operation is a split that a quick slot can receive (consumable quick slots only).
	 *
	 * @param Operation    The operation being dragged.
	 * @param QuickSlotTag Tag of the slot under the cursor.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Split")
	bool CanDropSplitOnQuickSlot(const UDragDropOperation* Operation, const FGameplayTag& QuickSlotTag) const;

	/**
	 * Handles a split dropped on a quick slot: asks the inventory (through the controller) to move the split
	 * quantity into a new stack assigned to that slot.
	 *
	 * @param Operation    The dropped operation.
	 * @param QuickSlotTag Tag of the receiving slot.
	 * @return True when the operation was a split accepted by the slot (the drop is handled).
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Split")
	bool TryDropSplitOnQuickSlot(UDragDropOperation* Operation, const FGameplayTag& QuickSlotTag);

	// --- Quick slots (gamepad action menu) ---

	/**
	 * Gamepad action menu: the quick slot its ChoiceIndex-th per-slot button assigns the item to
	 * (UInventoryDashboardController::FindQuickSlotChoices). With the mouse the menu keeps the generic button.
	 *
	 * @param Item        The item the action menu was opened on.
	 * @param ChoiceIndex Button index, from 0.
	 * @return The quick slot tag, or an empty tag when that button stays hidden.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	FGameplayTag GetGamepadQuickSlotChoice(const UInventoryItem* Item, int32 ChoiceIndex) const;

	/**
	 * Returns whether the action menu shows the generic quick slot button (automatic slot / remove): always with
	 * the mouse, only to remove a quick-slotted item with the gamepad (the per-slot buttons replace "add").
	 *
	 * @param Item The item the action menu was opened on.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	bool ShowsGenericQuickSlotButton(const UInventoryItem* Item) const;

	/**
	 * Label of a per-slot quick slot button, e.g. "QuickSlot 2" (the slot number within its weapon or consumable row).
	 *
	 * @param QuickSlotTag Quick slot the button assigns.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	static FText GetQuickSlotChoiceLabel(const FGameplayTag& QuickSlotTag);

	// --- Quantity popup ---

	/** Closes the quantity popup if it is open. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quantity")
	void CloseQuantityPopup();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Gives the dashboard to its panels (and through them to their entries). */
	virtual void NativeOnInitialized() override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Tab closes the dashboard before anything else sees it. */
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Gamepad Back (B) closes the dashboard, like Esc (the menus and the quantity popup close themselves); Y opens the sort menu. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** A click on the dashboard background closes the action menu. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** A split drag (started by the dashboard) released outside InventoryFrame drops only the split quantity. */
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/**
	 * Implemented by the Blueprint: creates the item tooltip (W_ToolTip_EquipmentEntry) for an entry.
	 *
	 * @param Item The item to describe.
	 * @return The tooltip widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|ToolTip")
	UWidget* MakeItemToolTip(UInventoryItem* Item);

private:
	/** The sort button was clicked. */
	UFUNCTION()
	void HandleSortButtonClicked();

	/** Shows the tooltip of the hovered entry, or hides it (no entry, action menu open, drag in progress, dashboard closed). */
	void RefreshHoverToolTip();

	/** Closes the transient UI (action menu, quantity popup, split drag) before the dashboard hides or goes away. */
	void CloseTransientUI();

	/**
	 * Creates the quantity popup for a stack and adds it to the viewport.
	 *
	 * @param Item   The stack the action applies to.
	 * @param Action What the confirmed quantity is used for.
	 * @return False when QuantityPopupClass is not set or the popup cannot be created.
	 */
	bool OpenQuantityPopup(const UInventoryItem& Item, EMKHQuantityPopupAction Action);

	/**
	 * Popup confirmation handler: drops, or starts the split drag on the next tick (outside the click handling).
	 * With the gamepad there is no cursor to drag with: the split goes straight to a consumable quick slot.
	 */
	UFUNCTION()
	void HandleQuantityConfirmed(int64 ItemID, int32 Quantity);

	/**
	 * Attaches a split of the stack to the cursor as a UMG drag operation.
	 *
	 * @param SourceItem The stack being split.
	 * @param Quantity   Quantity carried by the drag.
	 */
	void BeginSplitDrag(const UInventoryItem* SourceItem, int32 Quantity);

	/**
	 * Starts a Slate drag & drop of a UMG operation from the current cursor position, without a mouse press:
	 * the next mouse release drops it on the widget under the cursor.
	 *
	 * @param Operation The operation to drag.
	 * @return True when the drag started.
	 */
	bool StartDragDropAtCursor(UDragDropOperation* Operation);

	/** Cancels the drag in progress if it is a split. */
	void CancelSplitDrag() const;

	/**
	 * Returns whether a drag ended with a real left button release outside InventoryFrame (CancelDragDrop sends an
	 * empty event, which never counts).
	 *
	 * @param PointerEvent The release event.
	 */
	bool IsReleasedOutsideFrame(const FPointerEvent& PointerEvent) const;

	/** Gets the dashboard controller set by the HUD, or nullptr. */
	UInventoryDashboardController* GetDashboardController() const;

	/**
	 * Plays an interface sound effect (DT_SoundEffects row).
	 *
	 * @param SoundTag Sound effect.
	 */
	void PlayUISound(const FGameplayTag& SoundTag) const;

public:
	// ============================================================
	// Properties
	// ============================================================

	/** True while the dashboard is shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsOpen = true;

	/** Broadcast when the dashboard opens. */
	FOnOpenInventoryDashboardSignature OnOpenInventoryDashboardDelegate;

	/** Broadcast when the dashboard is hidden. */
	FOnCloseInventoryDashboardSignature OnCloseInventoryDashboardDelegate;

	/** Broadcast when the dashboard is destroyed. */
	FOnDestructionInventoryDashboardSignature OnDestructionInventoryDashboardDelegate;

protected:
	/** The inventory window: a drag released outside it drops the item (TryDropDraggedItemOutside). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Drop", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> InventoryFrame;

	/** The bag. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidget))
	TObjectPtr<UMKHInventoryBagWidget> Bag;

	/** Icon on the bag's title row: opens the sort menu. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Sort", meta = (BindWidgetOptional))
	TObjectPtr<UButton> SortButton;

	/** Menu of the sort criteria (e.g. W_BagSortMenu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Sort")
	TSubclassOf<UMKHInventorySortMenuWidget> SortMenuClass;

	/** Armor panel (portrait, armor slots, stats). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidget))
	TObjectPtr<UMKHInventoryEquipmentWidget> Equipment;

	/** Weapon panel (one row per weapon quick slot, with its skills). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidget))
	TObjectPtr<UMKHInventoryEquipmentWidget> WeaponLoadout;

	/** Consumable quick slots. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidget))
	TObjectPtr<UMKHInventoryQuickSlotAreaWidget> QuickSlotArea;

	/** Action menu opened on an entry (e.g. W_ActionMenu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|ActionMenu")
	TSubclassOf<UMKHInventoryActionMenuWidget> ActionMenuClass;

	/** Viewport Z order of the action menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|ActionMenu")
	int32 ActionMenuZOrder = 0;

	/** Widget under the cursor while an item or a split is dragged (e.g. W_EntryDragVisual). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|DragDrop")
	TSubclassOf<UMKHItemDragVisualWidget> DragVisualClass;

	/** Popup used to choose how many items of a stack to drop or split (e.g. W_QuantityPopup). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Quantity")
	TSubclassOf<UMKHQuantityPopupWidget> QuantityPopupClass;

	/** Viewport Z order of the quantity popup; must be above the dashboard and the action menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Quantity")
	int32 QuantityPopupZOrder = 50;

private:
	/** The action menu currently open, if any. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHInventoryActionMenuWidget> ActionMenu;

	/** The sort menu currently open, if any. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHInventorySortMenuWidget> SortMenu;

	/** The quantity popup currently open, if any. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHQuantityPopupWidget> QuantityPopup;

	/** Entry under the mouse. */
	TWeakObjectPtr<UMKHInventoryEntryWidget> HoveredEntry;

	/** What the open popup's quantity is used for. */
	EMKHQuantityPopupAction PendingQuantityAction = EMKHQuantityPopupAction::Drop;

	/** Stack the open split popup refers to (the preview of the drag is built from it). */
	TWeakObjectPtr<const UInventoryItem> PendingSplitSource;
};
