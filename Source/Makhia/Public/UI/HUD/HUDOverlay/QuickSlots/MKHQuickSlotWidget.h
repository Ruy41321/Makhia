// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Styling/SlateBrush.h"
#include "UI/MKHSystemWidget.h"
#include "MKHQuickSlotWidget.generated.h"

class UAbilitySystemComponent;
class UImage;
class UInputAction;
class UInventoryItem;
class UMKHKeyHintWidget;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/**
 * Base of every HUD quick slot (weapon, weapon skill, consumable): an item icon over a background, the keycap of
 * the action that triggers the slot and, when the content has one, a cooldown overlay.
 *
 * C++ owns the whole flow (item binding, filled/empty look, rarity background, keycap, cooldown tracking);
 * subclasses only say what the slot shows through the Resolve* hooks, and the Blueprint owns the layout.
 * Only SlotIcon is required in the widget tree; every other part is optional.
 * Full structure: see Documentation/HUDQuickSlots.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHQuickSlotWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows an item in the slot and follows its updates until the slot is reset.
	 *
	 * @param InItem  UI view of the item; null empties the slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void SetSlotItem(UInventoryItem* InItem);

	/** Empties the slot: drops the item, restores the empty look and stops any cooldown display. */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void ResetSlot();

	/** Re-renders the slot from the current item (icon, background, quantity, cooldown). */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void RefreshSlot();

	/**
	 * Changes the action that triggers the slot and forwards it to the keycap.
	 *
	 * @param InInputAction  Action whose key is shown under the slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void SetInputAction(UInputAction* InInputAction);

	/** True when the slot currently shows content (the flash plays white when filled, red when empty). */
	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	bool IsSlotFilled() const;

	/** Quick slot this widget displays. */
	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	FGameplayTag GetQuickSlotTag() const { return QuickSlotTag; }

	/**
	 * Changes the quick slot this widget displays.
	 *
	 * @param InQuickSlotTag  New quick slot tag (e.g. Input.Ability.QuickSlot.Weapon.Primary).
	 */
	void SetQuickSlotTag(const FGameplayTag& InQuickSlotTag) { QuickSlotTag = InQuickSlotTag; }

	/** Item shown by the slot, or null when it is empty. */
	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	UInventoryItem* GetSlotItem() const { return InventoryItem; }

	/** ItemID of the item shown by the slot, or 0 when it is empty. */
	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	int64 GetSlotItemID() const;

	/** Plays the activation flash (BP_PlayActivationFlash): white when filled, red when empty. */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void PlayActivationFlash();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Forwards InputAction to the keycap, also in the designer. */
	virtual void NativePreConstruct() override;

	/** Caches the designer brushes used by the empty look. */
	virtual void NativeOnInitialized() override;

	/** Shows the empty look until an item is assigned. */
	virtual void NativeConstruct() override;

	/** Unbinds from the item and the ability system. */
	virtual void NativeDestruct() override;

	// ============================================================
	// Protected / Internal Logic (subclass hooks)
	// ============================================================

	/** True when the current item gives the slot something to show. Default: a valid item. */
	virtual bool HasSlotContent() const;

	/** Icon of the current content. Only called when HasSlotContent() is true. */
	virtual UTexture2D* ResolveSlotIcon() const PURE_VIRTUAL(UMKHQuickSlotWidget::ResolveSlotIcon, return nullptr;);

	/** Cooldown tag of the current content; an invalid tag means "no cooldown". */
	virtual FGameplayTag ResolveCooldownTag() const;

	/** Quantity to print on the slot; values below 2 hide the counter. */
	virtual int32 ResolveDisplayedQuantity() const;

	/** Blueprint hook, called once the slot has been re-rendered. */
	UFUNCTION(BlueprintImplementableEvent, Category = "QuickSlot")
	void BP_OnSlotRefreshed(bool bFilled);

	/** Blueprint hook, called when a tracked cooldown ends (the slots flash on it). */
	UFUNCTION(BlueprintImplementableEvent, Category = "QuickSlot")
	void BP_OnCooldownFinished();

	/** Blueprint hook playing the slot's flash (W_FlashVFX); read IsSlotFilled() for the colour. */
	UFUNCTION(BlueprintImplementableEvent, Category = "QuickSlot")
	void BP_PlayActivationFlash();

	/** Item updated callback: re-renders the slot. */
	UFUNCTION()
	void HandleItemUpdated();

private:
	// --- Look ---

	/** Applies icon, background and quantity of the current content. */
	void ApplyFilledVisuals();

	/** Restores the designer brushes and applies the empty icon and tints. */
	void ApplyEmptyVisuals();

	/** Paints SlotBg with the rarity's square material, when bUseRarityBackground is set. */
	void ApplyRarityBackground() const;

	/** Shows or hides QuantityText from ResolveDisplayedQuantity(). */
	void UpdateQuantityText() const;

	// --- Item binding ---

	/** Subscribes to the item's update delegate. */
	void BindItem(UInventoryItem* InItem);

	/** Unsubscribes from the current item and forgets it. */
	void UnbindItem();

	// --- Cooldown ---

	/**
	 * Follows a cooldown tag on the owner's ability system; shows the overlay while the tag is present.
	 * Replaces any cooldown tracked before.
	 *
	 * @param CooldownTag  Tag granted by the cooldown effect; invalid stops tracking.
	 */
	void StartCooldownTracking(const FGameplayTag& CooldownTag);

	/** Stops following the tracked cooldown tag and hides the overlay without flashing. */
	void StopCooldownTracking();

	/** Tag count callback: starts the countdown on add, ends it (with the finished hook) on removal. */
	void HandleCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Shows the overlay and starts refreshing it every CooldownRefreshInterval. */
	void BeginCooldownDisplay();

	/**
	 * Hides the overlay and stops the refresh timer.
	 *
	 * @param bFinished  True when the cooldown actually ran out (fires BP_OnCooldownFinished).
	 */
	void EndCooldownDisplay(bool bFinished);

	/** Timer callback: updates the progress bar and the seconds left. */
	void UpdateCooldownDisplay();

	/** Ability system of the owning player, or null before the player state exists. */
	UAbilitySystemComponent* GetOwnerAbilitySystem() const;

	// ============================================================
	// Properties
	// ============================================================

protected:
	// --- Configuration ---

	/** Action that triggers the slot; its key is shown by KeyCap. Set per instance or via SetInputAction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickSlot")
	TObjectPtr<UInputAction> InputAction;

	/** Quick slot this widget displays (e.g. Input.Ability.QuickSlot.Consumable.First). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot")
	FGameplayTag QuickSlotTag;

	/** Texture shown while the slot is empty; null keeps the designer icon. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot|Appearance")
	TObjectPtr<UTexture2D> EmptyIconTexture;

	/** Icon tint while the slot is empty. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot|Appearance")
	FLinearColor EmptyIconTint = FLinearColor(0.f, 0.f, 0.f, 0.4f);

	/** True to paint SlotBg with the item rarity when filled (and EmptyBackgroundTint when empty). */
	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot|Appearance")
	bool bUseRarityBackground = true;

	/** Background tint while the slot is empty; used only with bUseRarityBackground. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot|Appearance", meta = (EditCondition = "bUseRarityBackground"))
	FLinearColor EmptyBackgroundTint = FLinearColor(0.f, 0.f, 0.f, 0.4f);

	/** Seconds between two refreshes of the cooldown overlay. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot|Cooldown", meta = (ClampMin = "0.02"))
	float CooldownRefreshInterval = 0.1f;

	// --- State ---

	/** Item currently shown, or null when the slot is empty. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "QuickSlot")
	TObjectPtr<UInventoryItem> InventoryItem;

	// --- Bound widgets ---

	/** Item icon. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> SlotIcon;

	/** Optional background, painted with the rarity when bUseRarityBackground is set. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> SlotBg;

	/** Optional keycap (W_KeyCap) showing the key of InputAction. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHKeyHintWidget> KeyCap;

	/** Optional stack counter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	/** Optional cooldown overlay, visible only while the cooldown runs. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CDOverlay;

	/** Optional cooldown progress (remaining / duration). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CDProgress;

	/** Optional seconds left, rounded up. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CDTime;

private:
	/** Designer brush of SlotIcon, restored by the empty look. */
	FSlateBrush DefaultIconBrush;

	/** Designer brush of SlotBg, restored by the empty look. */
	FSlateBrush DefaultBgBrush;

	/** True once the designer brushes have been cached. */
	bool bDefaultBrushesCached = false;

	/** Cooldown tag currently followed. */
	FGameplayTag TrackedCooldownTag;

	/** Ability system the cooldown tag event is registered on. */
	TWeakObjectPtr<UAbilitySystemComponent> TrackedAbilitySystem;

	/** Registration of the cooldown tag event. */
	FDelegateHandle CooldownTagEventHandle;

	/** Refresh timer of the cooldown overlay. */
	FTimerHandle CooldownRefreshTimer;
};
