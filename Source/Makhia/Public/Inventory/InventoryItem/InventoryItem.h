// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/EquipmentTypes.h"
#include "Inventory/ItemTypes.h"
#include "Inventory/MKHItemEntry.h"
#include "UObject/Object.h"
#include "InventoryItem.generated.h"

struct FRPGInventoryEntry;
struct FRPGEquipmentEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FItemUpdatedSignature);

/**
 * UI-only view of a single item, either from the inventory or from an equipment slot.
 * Wraps the item identity (FMKHItemEntry) plus its UI state, and caches the presentation data
 * derived from the item definition (resolved through UMKHDataSubsystem).
 * Never used on the gameplay path: equip/unequip/use work on FMKHItemEntry directly.
 */
UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UInventoryItem : public UObject
{
	GENERATED_BODY()

public:

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	// --- Initialisation ---

	/** Creates a UI item from an inventory entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item", meta = (WorldContext = "WorldContextObject"))
	static UInventoryItem* CreateFromInventoryEntry(UObject* WorldContextObject, const FRPGInventoryEntry& InEntry);

	/** Creates a UI item from an equipment entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item", meta = (WorldContext = "WorldContextObject"))
	static UInventoryItem* CreateFromEquipmentEntry(UObject* WorldContextObject, const FRPGEquipmentEntry& InEntry);

	/**
	 * Creates a UI item from a bare item, with no inventory or equipment state (e.g. an item lying on the ground).
	 *
	 * @param WorldContextObject Outer of the new object; also used to reach UMKHDataSubsystem.
	 * @param InItem             The item to display.
	 * @return The new UI item, or nullptr without a valid world context.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item", meta = (WorldContext = "WorldContextObject"))
	static UInventoryItem* CreateFromItemEntry(UObject* WorldContextObject, const FMKHItemEntry& InItem);

	/** Initializes this object from an inventory entry payload. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item")
	void InitFromInventoryEntry(const FRPGInventoryEntry& InEntry);

	/**
	 * Initializes this object from a bare item: not equipped, not quick-slotted, no slot.
	 *
	 * @param InItem The item to display.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item")
	void InitFromItemEntry(const FMKHItemEntry& InItem);

	/** Initializes this object from an equipment entry payload. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item")
	void InitFromEquipmentEntry(const FRPGEquipmentEntry& InEquipEntry);

	/** Copies all data from another UInventoryItem and broadcasts OnItemUpdatedDelegate. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item")
	void CopyFrom(const UInventoryItem* Other);

	// --- Item identity ---

	/** Returns the wrapped item identity (tag, ID, quantity, rarity, rolled effects). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FMKHItemEntry GetItemEntry() const { return Item; }

	/** Returns the item gameplay tag. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FGameplayTag GetItemTag() const { return Item.ItemTag; }

	/** Returns the item rarity tag. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FGameplayTag GetRarityTag() const { return Item.RarityTag; }

	/** Returns the current stack quantity. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	int32 GetQuantity() const { return Item.Quantity; }

	/** Returns the rolled effect package. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FEquipmentEffectPackage GetEffectPackage() const { return Item.EffectPackage; }

	/** Returns the unique item ID. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	int64 GetItemID() const { return Item.ItemID; }

	// --- UI state ---

	/** Returns whether the item is currently equipped. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool IsEquipped() const { return bIsEquipped; }

	/** Returns whether the item is currently assigned to a quick slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool IsQuickSlotted() const { return bIsQuickSlotted; }

	/** Returns the assigned quick slot tag, if any. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FGameplayTag GetQuickSlotTag() const { return QuickSlotTag; }

	// --- Item definition derived data (cached on init) ---

	/** Returns the localized display name from the item definition. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FText GetItemName() const { return ItemName; }

	/** Returns the equipment slot tag from the equipment definition (empty for non-equipment items). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FGameplayTag GetSlotTag() const { return SlotTag; }

	/** Returns the consumable cooldown duration in seconds (0 when not a consumable). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	float GetConsumableCooldownTime() const { return ConsumableCooldownTime; }

	/** Returns the cooldown gameplay tag applied when this consumable is used. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FGameplayTag GetConsumableCooldownTag() const { return ConsumableCooldownTag; }

	/** Returns the cached icon from the resolved item definition. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	UTexture2D* GetIcon() const { return Icon; }

	/** Returns the cached weapon HUD icon from the resolved item definition. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	UTexture2D* GetWeaponHudIcon() const { return WeaponHudIcon; }

	/** Returns the cached localized category name (e.g. "Hatchet"); empty for items without a category. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FText GetCategoryName() const { return CategoryName; }

	/** Returns the cached weapon base damage resolved from equipment definition. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	float GetWeaponDamage() const { return BaseWeaponDamage; }

	/** Returns the cached item description text. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FText GetDescription() const { return Description; }

private:

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Copies all fields from another inventory item without broadcasting delegates. */
	void CopyCoreDataFrom(const UInventoryItem& Other);

	/**
	 * Looks up the item definition of the wrapped item through UMKHDataSubsystem.
	 *
	 * @param OutItemDefinition Copy of the definition when found.
	 * @return True if a definition exists for the item tag.
	 */
	bool TryResolveItemDefinition(FMasterItemDefinition& OutItemDefinition) const;

	/** Updates every derived field (name, icons, slot, stats, cooldown, ...) from ItemDefinition. */
	void CacheDerivedDataFromItemDefinition();

	/** Clears derived cached presentation fields when definition resolution fails. */
	void ResetDerivedData();

	/** Resolves ItemDefinition and updates all derived cached fields. */
	void ResolveItemDefinition();

public:

	// ============================================================
	// Properties
	// ============================================================

	/** Broadcasts whenever this object is refreshed from another payload/item. */
	UPROPERTY(BlueprintAssignable)
	FItemUpdatedSignature OnItemUpdatedDelegate;

private:

	// --- Item identity & UI state ---

	/** Wrapped item identity, copied from the source inventory or equipment entry. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	FMKHItemEntry Item;

	/** True when this item is assigned to a quick slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	bool bIsQuickSlotted = false;

	/** Quick slot tag currently associated with this item. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	FGameplayTag QuickSlotTag = FGameplayTag();

	/** True when the item is equipped (equipment entry, or weapon in use from the inventory). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	bool bIsEquipped = false;

	// --- Derived from the item definition ---

	/** Item definition resolved from the item tag. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	FMasterItemDefinition ItemDefinition;

	/** Cached from ItemDefinition on init. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	FText ItemName = FText();

	/** Equipment slot from the equipment definition; falls back to the equipment entry slot when unresolved. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	FGameplayTag SlotTag = FGameplayTag();

	/** Cached from ItemDefinition on init. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Cached from ItemDefinition on init. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> WeaponHudIcon = nullptr;

	/** Cached from ItemDefinition on init. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	float BaseWeaponDamage = 0.f;

	/** Cached from ItemDefinition on init. Read directly by W_ToolTip_EquipmentEntry. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	float BlockingStability = 0.f;

	/** Cached from ItemDefinition on init. Read directly by W_ToolTip_EquipmentEntry. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	float BaseArmorGiven = 0.f;

	/** Cached from ItemDefinition on init. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	FText Description = FText();

	/** Localized category name, from UMKHDataSubsystem::GetItemCategoryName on init. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	FText CategoryName = FText();

	/** Cooldown duration (seconds) of the consumable, from ItemDefinition. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	float ConsumableCooldownTime = 0.f;

	/** Cooldown gameplay tag of the consumable, from ItemDefinition. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (AllowPrivateAccess = "true"))
	FGameplayTag ConsumableCooldownTag = FGameplayTag();
};
