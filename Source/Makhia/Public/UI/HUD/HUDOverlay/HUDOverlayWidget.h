// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "GameplayTagContainer.h"
#include "HUDOverlayWidget.generated.h"

class UInventoryItem;
class UMKHConsumablesHUDWidget;
class UMKHInventoryHintWidget;
class UMKHQuickSlotContainerWidget;
class UMKHWeaponHUDWidget;

/**
 * Gameplay overlay (W_HUDOverlay). Receives the quick slot events of UHUDOverlayController and routes them to the
 * weapon block (weapons, by item tag) or to the consumables block (everything else).
 * See Documentation/HUDQuickSlots.md.
 */
UCLASS()
class MAKHIA_API UHUDOverlayWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * An item entered, moved between or left the quick slots: it leaves its old slot and shows in its new one.
	 *
	 * @param NewItem  UI view of the item.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|QuickSlots")
	void OnItemQuickSlotted(UInventoryItem* NewItem);

	/**
	 * A quick slotted item changed (quantity, rolls): the slot showing it refreshes.
	 *
	 * @param NewItem  Updated UI view of the item.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|QuickSlots")
	void OnItemChanged(UInventoryItem* NewItem);

	/**
	 * A quick slotted item was removed: its slot empties.
	 *
	 * @param RemovedItemID  Item removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|QuickSlots")
	void OnItemRemoved(const int64 RemovedItemID);

	/**
	 * A quick slot, skill or the inventory was activated: flashes what it triggers.
	 *
	 * @param QuickSlotTag  Activated tag (Input.Inventory, a quick slot or a skill input tag).
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|QuickSlots")
	void OnQuickSlotActivated(const FGameplayTag& QuickSlotTag);

	/**
	 * The active weapon quick slot changed: the other filled weapon row is dimmed.
	 *
	 * @param ActiveWeaponSlotTag  Active weapon quick slot; invalid when no weapon is quick slotted.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|QuickSlots")
	void OnActiveWeaponSlotChanged(const FGameplayTag& ActiveWeaponSlotTag);

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Weapon block for weapons, consumables block otherwise; null when the matching block is missing. */
	UMKHQuickSlotContainerWidget* GetContainerForItem(const UInventoryItem* Item) const;

	/** Shows the scoreboard look picked in the settings and collapses the other one. */
	void ApplyScoreboardStyle();

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** Weapon block (W_WeaponHUD). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHWeaponHUDWidget> WeaponHUD;

	/** Consumables block (W_ConsumablesHUD). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHConsumablesHUDWidget> ConsumablesHUD;

	/** Inventory hint (W_InventoryHint), flashed when the inventory opens. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHInventoryHintWidget> InventoryHint;

	/** Pediment scoreboard (W_MatchInfo). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> ScoreboardPediment;

	/** Open text scoreboard (W_MatchInfo_Epigraph). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> ScoreboardEpigraph;

private:
	/** Subscription to UMKHGameUserSettings::OnScoreboardStyleChanged. */
	FDelegateHandle ScoreboardStyleHandle;
};
