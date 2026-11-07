// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/QuickSlots/MKHSkillQuickSlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Equipment/EquipmentTypes.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "Inventory/MKHSkillLayout.h"
#include "UI/HUD/Inventory/MKHDropHighlightWidget.h"
#include "UI/HUD/Inventory/MKHInventoryWeaponSkillsWidget.h"
#include "UI/HUD/Inventory/MKHSkillDragOperation.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

// ============================================================
// Lifecycle
// ============================================================

UMKHSkillQuickSlotWidget::UMKHSkillQuickSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseRarityBackground = false;
	EmptyIconTint = FLinearColor(0.1f, 0.1f, 0.1f, 0.9f);
}

// ============================================================
// Public Interface
// ============================================================

void UMKHSkillQuickSlotWidget::EnableReorder(UMKHInventoryWeaponSkillsWidget* InRow)
{
	ReorderRow = InRow;
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

FGameplayTag UMKHSkillQuickSlotWidget::GetSkillSlotTag() const
{
	return FMKHSkillLayout::GetSlotAt(SkillIndex);
}

UTexture2D* UMKHSkillQuickSlotWidget::GetSkillIcon() const
{
	return HasSlotContent() ? ResolveSlotIcon() : nullptr;
}

void UMKHSkillQuickSlotWidget::ShowPreviewIcon(UTexture2D* Icon, float Opacity)
{
	if (!IsValid(SlotIcon))
	{
		return;
	}

	SlotIcon->SetVisibility(IsValid(Icon) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	SlotIcon->SetRenderOpacity(Opacity);
	if (IsValid(Icon))
	{
		SlotIcon->SetBrushFromTexture(Icon);
		SlotIcon->SetBrushTintColor(FSlateColor(FLinearColor::White));
	}
}

void UMKHSkillQuickSlotWidget::ClearPreviewIcon()
{
	if (IsValid(SlotIcon))
	{
		SlotIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		SlotIcon->SetRenderOpacity(1.f);
	}
	RefreshSlot();
}

void UMKHSkillQuickSlotWidget::SetReorderLook(bool bHighlighted, bool bLifted)
{
	if (IsValid(ReorderHighlight))
	{
		ReorderHighlight->SetHighlightActive(bHighlighted);
	}
	SetRenderTranslation(FVector2D(0.f, bLifted ? LiftedOffset : 0.f));
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHSkillQuickSlotWidget::HasSlotContent() const
{
	FEquipmentAbilityDefinition Skill;
	return FindSkill(Skill);
}

UTexture2D* UMKHSkillQuickSlotWidget::ResolveSlotIcon() const
{
	FEquipmentAbilityDefinition Skill;
	return FindSkill(Skill) ? Skill.Icon.Get() : nullptr;
}

FGameplayTag UMKHSkillQuickSlotWidget::ResolveCooldownTag() const
{
	FEquipmentAbilityDefinition Skill;
	return FindSkill(Skill) ? Skill.CooldownTag : FGameplayTag();
}

FReply UMKHSkillQuickSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UMKHInventoryWeaponSkillsWidget* Row = ReorderRow.Get();
	if (!IsValid(Row))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	Row->HandleSlotPressed();
	return HasSlotContent() && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)
		? UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply
		: FReply::Handled();
}

void UMKHSkillQuickSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (UMKHInventoryWeaponSkillsWidget* Row = ReorderRow.Get())
	{
		OutOperation = Row->BeginSkillDrag(*this);
	}
}

void UMKHSkillQuickSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
	if (UMKHInventoryWeaponSkillsWidget* Row = ReorderRow.Get())
	{
		Row->HandleSkillDragEnter(*this, InOperation);
	}
}

void UMKHSkillQuickSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	if (UMKHInventoryWeaponSkillsWidget* Row = ReorderRow.Get())
	{
		Row->HandleSkillDragLeave(*this, InOperation);
	}
}

bool UMKHSkillQuickSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UMKHInventoryWeaponSkillsWidget* Row = ReorderRow.Get();
	return IsValid(Row) ? Row->HandleSkillDrop(*this, InOperation) : Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

FReply UMKHSkillQuickSlotWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (UMKHInventoryWeaponSkillsWidget* Row = ReorderRow.Get())
	{
		if (UMKHMenuNavigationLibrary::IsGamepadAcceptKey(InKeyEvent))
		{
			Row->HandlePadAccept(*this);
			return FReply::Handled();
		}
		if (UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent) && Row->HandlePadBack())
		{
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UMKHSkillQuickSlotWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	const FReply Reply = Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
	if (UMKHInventoryWeaponSkillsWidget* Row = ReorderRow.Get())
	{
		Row->HandleSlotFocusChanged();
	}
	return Reply;
}

void UMKHSkillQuickSlotWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	if (UMKHInventoryWeaponSkillsWidget* Row = ReorderRow.Get())
	{
		Row->HandleSlotFocusChanged();
	}
}

bool UMKHSkillQuickSlotWidget::FindSkill(FEquipmentAbilityDefinition& OutSkill) const
{
	if (!IsValid(InventoryItem))
	{
		return false;
	}

	const FEquipmentAbilityDefinition* Skill = FMKHSkillLayout::FindSkillInSlot(InventoryItem->GetEffectPackage(), GetSkillSlotTag());
	if (Skill == nullptr)
	{
		return false;
	}
	OutSkill = *Skill;
	return true;
}
