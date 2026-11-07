// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/WidgetControllers/WidgetController.h"
#include "HUDOverlayController.generated.h"

class UEquipmentManagerComponent;
class UQuickSlotManagerComponent;
class UInventoryComponent;
class UHUDOverlayWidget;
class UInventoryItem;
struct FRPGEquipmentEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHUDItemQuickSlottedSignature, UInventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHUDItemChangedSignature, UInventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHUDItemRemovedSignature, const int64, RemovedItemID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHUDActiveWeaponSlotChangedSignature, const FGameplayTag&, ActiveWeaponSlotTag);

/**
 * Model side of the gameplay overlay: turns inventory, equipment and quick slot events into HUD events, and owns
 * the active weapon quick slot (the equipped weapon's slot, else the quick-equip slot).
 * See Documentation/HUDQuickSlots.md.
 */
UCLASS(Blueprintable, BlueprintType)
class MAKHIA_API UHUDOverlayController : public UWidgetController
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Subscribes to the player's inventory, equipment and quick slot components. */
	virtual void BindCallbacksToDependencies() override;

	/** Sends the quick slotted items and the active weapon slot to a newly created overlay. */
	virtual void BroadcastInitialValues() override;

	/** Binds the HUD delegates to the overlay widget. */
	virtual void BindDelegatesToWidget_Implementation() override;

	/** Clears the HUD delegates. */
	virtual void UnbindAllEventsFromDelegates() override;

	// ============================================================
	// Public Interface
	// ============================================================

	/** Caches the inventory component of the owning actor. */
	void SetOwningInventoryComponent();

	/** Caches the equipment component of the owning actor. */
	void SetOwningEquipmentComponent();

	/** Caches the quick slot manager of the owning actor. */
	void SetOwningQuickSlotManagerComp();

	/**
	 * Sets the overlay this controller drives and binds its delegates.
	 *
	 * @param InHUDOverlayWidget  Overlay; null when it is destroyed.
	 */
	void SetHUDOverlayWidget(UHUDOverlayWidget* InHUDOverlayWidget);

	/** Active weapon quick slot, invalid when no weapon is quick slotted. */
	UFUNCTION(BlueprintPure, Category = "HUD|QuickSlots")
	FGameplayTag GetActiveWeaponSlotTag() const { return ActiveWeaponSlotTag; }

	/** Broadcast when a quick slot changes its item. */
	UPROPERTY(BlueprintAssignable)
	FHUDItemQuickSlottedSignature HUDItemQuickSlottedDelegate;

	/** Broadcast when a quick slotted item changes. */
	UPROPERTY(BlueprintAssignable)
	FHUDItemChangedSignature HUDItemChangedDelegate;

	/** Broadcast when a quick slotted item is removed. */
	UPROPERTY(BlueprintAssignable)
	FHUDItemRemovedSignature HUDItemRemovedDelegate;

	/** Broadcast when the active weapon quick slot changes. */
	UPROPERTY(BlueprintAssignable)
	FHUDActiveWeaponSlotChangedSignature HUDActiveWeaponSlotChangedDelegate;

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Forwards skill and quick slot activations to the overlay (flash). */
	void BindActivationCallbacks();

	/** Recomputes the active weapon slot on equip / unequip of the weapon slot. */
	void BindEquipmentCallbacks();

	/** Forwards quick slot item events and recomputes the active weapon slot on them. */
	void BindInventoryCallbacks();

	/** Recomputes the active weapon slot when the quick-equip slot or the quick slot bindings change. */
	void BindQuickSlotManagerCallbacks();

	/** Equipment callback: recomputes the active weapon slot when the weapon slot changed. */
	void HandleEquipmentEntryChanged(const FRPGEquipmentEntry& Entry);

	/**
	 * Schedules RefreshActiveWeaponSlot for the next tick, once per tick. Equipment (PlayerState) and quick slots
	 * (PlayerController) replicate in any order and unequip is broadcast before the entry leaves the list, so the
	 * computation waits until every event of the frame has been applied.
	 */
	void RequestActiveWeaponSlotRefresh();

	/** Recomputes the active weapon slot and broadcasts it when it changed. */
	void RefreshActiveWeaponSlot();

	/**
	 * The single rule of the active weapon slot: the slot of the equipped weapon; else the quick-equip slot; else
	 * the first filled weapon slot.
	 *
	 * @return The active weapon quick slot, invalid when no weapon is quick slotted.
	 */
	FGameplayTag ComputeActiveWeaponSlotTag();

	/** ItemID of the weapon in the equipment weapon slot, or 0. */
	int64 GetEquippedWeaponItemID();

	/** Caches the inventory component when needed; true when valid. */
	bool EnsureOwningInventory();

	/** Caches the equipment component when needed; true when valid. */
	bool EnsureOwningEquipment();

	/** Caches the quick slot manager when needed; true when valid. */
	bool EnsureOwningQuickSlotManagerComp();

	// ============================================================
	// Properties
	// ============================================================

	/** Quick slot manager of the owning player controller. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UQuickSlotManagerComponent> OwningQuickSlotManagerComp;

	/** Inventory of the owning player. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UInventoryComponent> OwningInventoryComp;

	/** Equipment of the owning player. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UEquipmentManagerComponent> OwningEquipmentComp;

	/** Overlay driven by this controller. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UHUDOverlayWidget> OwningHUDOverlayWidget;

	/** Last active weapon quick slot broadcast. */
	FGameplayTag ActiveWeaponSlotTag;

	/** True while a refresh is scheduled for the next tick. */
	bool bActiveWeaponSlotRefreshPending = false;
};
