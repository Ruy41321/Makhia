// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHQuickSlotContainerWidget.h"
#include "MKHWeaponSkillRowWidget.generated.h"

class UInputAction;
class UMKHSkillQuickSlotWidget;
class UMKHWeaponQuickSlotWidget;

/**
 * One weapon row of the HUD (W_WeaponSkillRow): a weapon quick slot plus the three skill slots of the weapon in it.
 *
 * The row pushes its QuickSlotTag and WeaponInputAction to the weapon slot, numbers the skill slots (SkillIndex 0,
 * 1, 2 in tree order) and shows the same item in all four. W_WeaponHUD marks it active or inactive: the inactive
 * row is dimmed and scaled down through colour and render transform only, so the layout never moves, and only
 * the active row flashes its skills. See "Active weapon" in Documentation/HUDQuickSlots.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHWeaponSkillRowWidget : public UMKHQuickSlotContainerWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Flashes the weapon slot when the tag is the row's weapon tag and its weapon is not already equipped, or the
	 * skill slot whose InputTag is exactly the tag when the row is active.
	 *
	 * @param InQuickSlotTag  Activated weapon quick slot or skill input tag.
	 */
	virtual void PlayActivationFlash(const FGameplayTag& InQuickSlotTag) override;

	/**
	 * Marks the row active (normal look) or inactive (dimmed, scaled down).
	 *
	 * @param bInActive  New state.
	 * @param bAnimate   True to blend over TransitionDuration, false to snap.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot|WeaponRow")
	void SetRowActive(bool bInActive, bool bAnimate = true);

	/** True when the row is active (normal look). */
	UFUNCTION(BlueprintPure, Category = "QuickSlot|WeaponRow")
	bool IsRowActive() const { return bRowActive; }

	/** True when the weapon slot shows a weapon. */
	UFUNCTION(BlueprintPure, Category = "QuickSlot|WeaponRow")
	bool HasWeapon() const;

	/** Weapon quick slot tag of the row (Input.Ability.QuickSlot.Weapon.Primary / Secondary). */
	UFUNCTION(BlueprintPure, Category = "QuickSlot|WeaponRow")
	FGameplayTag GetQuickSlotTag() const { return QuickSlotTag; }

	/** The row's weapon slot, or null when the tree has none. Caches the row slots on first use. */
	UMKHWeaponQuickSlotWidget* GetWeaponSlot();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Pushes QuickSlotTag and WeaponInputAction to the weapon slot again (designer edits). */
	virtual void NativePreConstruct() override;

	/** Numbers the skill slots and applies the current look. */
	virtual void NativeOnInitialized() override;

	/** Advances the active/inactive blend while it runs. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Gathers only the weapon slot: items are routed by the weapon's quick slot tag. */
	virtual void GatherQuickSlots(TArray<UMKHQuickSlotWidget*>& OutSlots) const override;

	/** Shows the weapon in the weapon slot and in the three skill slots. */
	virtual void AssignSlotItem(UMKHQuickSlotWidget& QuickSlot, UInventoryItem* Item) override;

	/** Empties the weapon slot and the three skill slots. */
	virtual void ClearSlotItem(UMKHQuickSlotWidget& QuickSlot) override;

private:
	/** Finds the weapon and skill slots of the tree, sets SkillIndex 0, 1, 2 and pushes the row settings to the weapon slot. */
	void CacheRowSlots();

	/** Gives the weapon slot the row's QuickSlotTag and WeaponInputAction, so the weapon block can route by tag. */
	void PushRowSettingsToWeaponSlot() const;

	/** Flashes the weapon slot unless its weapon is already equipped (pressing the equipped weapon's key). */
	void FlashWeaponSlot() const;

	/** Flashes the skill slot whose InputTag is exactly the tag. */
	void FlashSkillSlot(const FGameplayTag& InputTag) const;

	/** Applies tint and scale for a blend position (0 = active, 1 = inactive). */
	void ApplyInactiveBlend(float Blend);

	// ============================================================
	// Properties
	// ============================================================

protected:
	// --- Configuration ---

	/** Weapon quick slot shown by the row; pushed to the weapon slot. Set on the instance in W_WeaponHUD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickSlot|WeaponRow", meta = (Categories = "Input.Ability.QuickSlot.Weapon"))
	FGameplayTag QuickSlotTag;

	/** Action whose key is shown by the weapon slot. Set on the instance in W_WeaponHUD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickSlot|WeaponRow")
	TObjectPtr<UInputAction> WeaponInputAction;

	/** Linear colour multiplied on the whole row (slots, icons, keycaps) while inactive; 0.3 reads as ~55% brightness. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot|WeaponRow|Inactive")
	FLinearColor InactiveTint = FLinearColor(0.3f, 0.3f, 0.3f, 1.f);

	/** Render scale of the row while inactive (layout unchanged). */
	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot|WeaponRow|Inactive", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float InactiveScale = 0.85f;

	/** Normalised pivot of the inactive scale; (1, 0.5) keeps the weapon side, toward the screen edge, in place. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot|WeaponRow|Inactive")
	FVector2D InactiveScalePivot = FVector2D(1.f, 0.5f);

	/** Seconds of the blend between active and inactive. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot|WeaponRow|Inactive", meta = (ClampMin = "0.0"))
	float TransitionDuration = 0.15f;

private:
	// --- State ---

	/** Weapon slot of the tree. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHWeaponQuickSlotWidget> WeaponSlot;

	/** Skill slots of the tree, in tree order (SkillIndex 0, 1, 2). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMKHSkillQuickSlotWidget>> SkillSlots;

	/** Current state; rows start active. */
	bool bRowActive = true;

	/** Current blend position (0 = active, 1 = inactive). */
	float CurrentBlend = 0.f;
};
