// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHQuickSlotWidget.h"
#include "MKHSkillQuickSlotWidget.generated.h"

class UMKHDropHighlightWidget;
class UMKHInventoryWeaponSkillsWidget;
struct FEquipmentAbilityDefinition;

/**
 * Weapon skill slot (W_SkillSlotHUD): shows the skill of the weapon standing on the slot's input (SkillIndex-th skill
 * slot, FMKHSkillLayout), with its icon and cooldown. Empty when no skill of the weapon stands there. On the HUD it takes
 * no input; in the inventory its row turns on the player's reorder (EnableReorder) and receives its input events.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSkillQuickSlotWidget : public UMKHQuickSlotWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Skill slots keep their designer background and use the dark "no skill" tint. */
	UMKHSkillQuickSlotWidget(const FObjectInitializer& ObjectInitializer);

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Makes the slot take the mouse and the focus, sending its input to the row. Inventory only.
	 *
	 * @param InRow Row handling the reorder.
	 */
	void EnableReorder(UMKHInventoryWeaponSkillsWidget* InRow);

	/** Skill slot this widget stands for. */
	FGameplayTag GetSkillSlotTag() const;

	/** Icon of the skill on the slot, or nullptr when empty. */
	UTexture2D* GetSkillIcon() const;

	/**
	 * Shows another icon until ClearPreviewIcon: the skill being dragged, or the one a drop would send here.
	 *
	 * @param Icon    Icon to show; nullptr hides the icon.
	 * @param Opacity Opacity of the icon.
	 */
	void ShowPreviewIcon(UTexture2D* Icon, float Opacity);

	/** Shows the slot's own content again. */
	void ClearPreviewIcon();

	/**
	 * Sets the reorder look.
	 *
	 * @param bHighlighted True while the slot can take the skill, or has the pad focus.
	 * @param bLifted      True while the pad holds this slot's skill.
	 */
	void SetReorderLook(bool bHighlighted, bool bLifted);

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** True when the weapon has a skill at SkillIndex. */
	virtual bool HasSlotContent() const override;

	/** The skill's icon. */
	virtual UTexture2D* ResolveSlotIcon() const override;

	/** The skill's cooldown tag. */
	virtual FGameplayTag ResolveCooldownTag() const override;

	// --- Reorder input, forwarded to the row (inventory only) ---

	/** Starts detecting a drag on a filled slot. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Creates the skill drag. */
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	/** Shows the outcome of a drop here. */
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Removes the outcome shown by NativeOnDragEnter. */
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Moves the dragged skill here. */
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Pad A takes or places a skill; pad B puts back a held skill. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Updates the row's pad look. */
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

	/** Updates the row's pad look. */
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

private:
	/**
	 * Finds the skill standing on the slot's input in the weapon's effect package.
	 *
	 * @param OutSkill  Copy of the skill definition when found.
	 * @return True when the weapon has such a skill.
	 */
	bool FindSkill(FEquipmentAbilityDefinition& OutSkill) const;

	// ============================================================
	// Properties
	// ============================================================

public:
	/** Position of the skill slot shown (0 = first, FMKHSkillLayout::GetSlotAt). Set by the row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot|Skill", meta = (ClampMin = "0"))
	int32 SkillIndex = 0;

	/** Ability input tag triggering this slot; W_WeaponSkillRow matches it to flash the right slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickSlot|Skill", meta = (Categories = "Input.Ability"))
	FGameplayTag InputTag;

protected:
	/** Optional highlight shown by the reorder (W_FadedVFX). */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UMKHDropHighlightWidget> ReorderHighlight;

private:
	/** Row handling the reorder; unset on the HUD. */
	TWeakObjectPtr<UMKHInventoryWeaponSkillsWidget> ReorderRow;

	/** Upward shift of the slot while the pad holds its skill, in slate units. */
	static constexpr float LiftedOffset = -6.f;
};
