// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/WidgetControllers/WidgetController.h"
#include "InventoryDashboardController.generated.h"

class UInventoryItem;
class UInventoryDashboardWidget;
class UInventoryComponent;
class UEquipmentManagerComponent;
class UQuickSlotManagerComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDashboardBagItemChangedSignature, UInventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDashboardBagItemRemovedSignature, const int64, RemovedItemID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDashboardEquipmentChangeSignature, UInventoryItem*, Item /*Equipped Item*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDashboardEquipmentRemovedSignature, const FGameplayTag&, SlotTag /*UnEquipped Slot*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDashboardQuickSlotRelocatedSignature, UInventoryItem*, Item /*Quick Slotted Item*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDashboardQuickSlotChangedSignature, UInventoryItem*, Item /*Quick Slotted Item*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDashboardQuickSlotRemovedSignature, const int64, RemovedItemID);

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class MAKHIA_API UInventoryDashboardController : public UWidgetController
{
	GENERATED_BODY()
	
public:

	UPROPERTY(BlueprintAssignable)
	FDashboardBagItemChangedSignature DashboardBagItemChangedDelegate;

	UPROPERTY(BlueprintAssignable)
	FDashboardBagItemRemovedSignature DashboardBagItemRemovedDelegate;
	
	UPROPERTY(BlueprintAssignable)
	FDashboardEquipmentChangeSignature DashboardEquipmentChangeDelegate;
	
	UPROPERTY(BlueprintAssignable)
	FDashboardEquipmentRemovedSignature DashboardEquipmentRemovedDelegate;
	
	UPROPERTY(BlueprintAssignable)
	FDashboardQuickSlotRelocatedSignature QuickSlotItemRelocatedDelegate;
	
	UPROPERTY(BlueprintAssignable)
	FDashboardQuickSlotChangedSignature QuickSlotItemChangedDelegate;
	
	UPROPERTY(BlueprintAssignable)
	FDashboardQuickSlotRemovedSignature QuickSlotItemRemovedDelegate;
	
	void SetOwningInventory();

	void SetOwningEquipmentManagerComp();
	
	void SetOwningQuickSlotManagerComp();
	
	void SetInventoryWidget(UInventoryDashboardWidget* InInventoryWidget);

	virtual void BindCallbacksToDependencies() override;

	virtual void BroadcastInitialValues() override;
	
	virtual void BindDelegatesToWidget_Implementation() override;
	
	virtual void UnbindAllEventsFromDelegates() override;

	/**
	 * Forwards a split confirmed in the UI to the owning inventory: moves Quantity items of the stack into a new
	 * stack assigned to the quick slot (the whole stack when Quantity covers it).
	 *
	 * @param ItemID       Source stack.
	 * @param Quantity     Quantity to move.
	 * @param QuickSlotTag Consumable quick slot receiving the new stack.
	 */
	void SplitStackToQuickSlot(int64 ItemID, int32 Quantity, const FGameplayTag& QuickSlotTag);

	/**
	 * Picks the consumable quick slot a split goes to when there is no drag (gamepad): the first empty one,
	 * else the first slot (its stack goes back to the bag).
	 *
	 * @return Tag of the chosen consumable quick slot.
	 */
	FGameplayTag FindConsumableQuickSlotForSplit();

	/**
	 * Lists the quick slots the gamepad action menu offers for an item, one button each: the three consumable slots,
	 * or the two weapon slots (only the slot of an already quick-slotted weapon of the same type, as with the
	 * mouse). The slot the item already occupies is left out.
	 *
	 * @param Item The item the action menu was opened on.
	 * @return The quick slot tags in button order; empty for items that cannot be quick-slotted.
	 */
	TArray<FGameplayTag> FindQuickSlotChoices(const UInventoryItem& Item);

	/**
	 * Picks the quick slot a one-click "quick slot" action assigns an item to: for a consumable the first empty
	 * consumable slot, else the first one; for a weapon the slot of a quick-slotted weapon of the same type, else the
	 * first weapon slot when empty, else the second.
	 *
	 * @param Item The item to quick-slot.
	 * @return The quick slot tag, or an empty tag for items that cannot be quick-slotted.
	 */
	FGameplayTag FindQuickSlotForItem(const UInventoryItem& Item);

	// --- Requests from the dashboard (forwarded to the owning components) ---

	/**
	 * Uses an item of the owning inventory (equips an armor, drinks a consumable).
	 *
	 * @param ItemID   Item to use.
	 * @param Quantity Quantity to use.
	 */
	void UseItem(int64 ItemID, int32 Quantity);

	/**
	 * Item tag of an item of the bag.
	 *
	 * @param ItemID Item to look up.
	 * @return Its item tag, or an empty tag when it is not in the bag.
	 */
	FGameplayTag GetItemTag(int64 ItemID);

	/**
	 * Unequips the item of an equipment slot at the player's request: an armor type taken off is remembered
	 * (UMKHQuickSlotPreferenceComponent).
	 *
	 * @param EquipmentSlotTag Equipment slot to empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Requests")
	void UnequipItem(const FGameplayTag& EquipmentSlotTag);

	/**
	 * Moves a weapon's skill to another skill slot at the player's request (a skill already there takes the slot left
	 * free); the choice is remembered for each skill moved (UMKHQuickSlotPreferenceComponent).
	 *
	 * @param ItemID   Weapon.
	 * @param FromSlot Skill slot holding the skill to move.
	 * @param ToSlot   Destination skill slot.
	 */
	void SwapWeaponSkills(int64 ItemID, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot);

	/**
	 * Assigns an item to a quick slot on the owning machine.
	 *
	 * @param ItemID       Item to assign.
	 * @param QuickSlotTag Target quick slot.
	 */
	void AddToQuickSlot(int64 ItemID, const FGameplayTag& QuickSlotTag);

	/**
	 * Takes an item out of its quick slot at the player's request: the slot stays empty in the next matches
	 * (UMKHQuickSlotPreferenceComponent).
	 *
	 * @param ItemID Item to remove.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Requests")
	void RemoveFromQuickSlot(int64 ItemID);

	/**
	 * Drops an item on the ground.
	 *
	 * @param ItemID   Item to drop.
	 * @param Quantity Quantity to drop; -1 drops the whole stack.
	 */
	void DropItem(int64 ItemID, int32 Quantity);

	/**
	 * Finds the weapon quick slot holding another weapon of the same type (equipment slot) as Item.
	 *
	 * @param Item The weapon about to be quick-slotted.
	 * @return The occupied weapon quick slot, or an empty tag when there is none.
	 */
	FGameplayTag FindWeaponQuickSlotOfSameType(const UInventoryItem& Item);

private:
	
	bool EnsureOwningInventory();
	
	bool EnsureOwningEquipmentManagerComp();
	
	bool EnsureOwningQuickSlotManagerComp();

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UInventoryComponent> OwningInventoryComp;
	
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UEquipmentManagerComponent> OwningEquipmentManagerComp;
	
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UQuickSlotManagerComponent> OwningQuickSlotManagerComp;
	
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UInventoryDashboardWidget> OwningInventoryDashboardWidget;	
};
