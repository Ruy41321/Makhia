// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventoryWeaponSkillsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHSkillQuickSlotWidget.h"
#include "UI/HUD/Inventory/InventoryDashboardWidget.h"
#include "UI/HUD/Inventory/MKHGamepadHintWidget.h"
#include "UI/HUD/Inventory/MKHSkillDragOperation.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

#define LOCTEXT_NAMESPACE "MKHInventoryWeaponSkills"

// ============================================================
// Lifecycle
// ============================================================

void UMKHInventoryWeaponSkillsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SkillSlots.Reset();
	if (IsValid(WidgetTree))
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (UMKHSkillQuickSlotWidget* Skill = Cast<UMKHSkillQuickSlotWidget>(Widget))
			{
				Skill->SkillIndex = SkillSlots.Num();
				SkillSlots.Add(Skill);
				Skill->EnableReorder(this);
			}
		});
	}
	OpenHitTestPathToSlots();
	RefreshPadHints(false);
	if (IsValid(CancelHint))
	{
		CancelHint->SetHintText(LOCTEXT("CancelSkillHint", "Cancel"));
	}
}

// ============================================================
// Public Interface
// ============================================================

void UMKHInventoryWeaponSkillsWidget::SetWeapon(UInventoryItem* Weapon)
{
	for (UMKHSkillQuickSlotWidget* Skill : SkillSlots)
	{
		if (IsValid(Skill))
		{
			Skill->SetSlotItem(Weapon);
		}
	}
}

void UMKHInventoryWeaponSkillsWidget::ClearWeapon()
{
	for (UMKHSkillQuickSlotWidget* Skill : SkillSlots)
	{
		if (IsValid(Skill))
		{
			Skill->ResetSlot();
		}
	}
}

int64 UMKHInventoryWeaponSkillsWidget::GetWeaponItemID() const
{
	return SkillSlots.Num() > 0 && IsValid(SkillSlots[0]) ? SkillSlots[0]->GetSlotItemID() : 0;
}

void UMKHInventoryWeaponSkillsWidget::SetDashboard(UInventoryDashboardWidget* InDashboard)
{
	Dashboard = InDashboard;
}

// ============================================================
// Public Interface (reorder)
// ============================================================

void UMKHInventoryWeaponSkillsWidget::HandleSlotPressed() const
{
	if (UInventoryDashboardWidget* OwningDashboard = Dashboard.Get())
	{
		OwningDashboard->CloseActionMenu();
	}
}

UMKHSkillDragOperation* UMKHInventoryWeaponSkillsWidget::BeginSkillDrag(UMKHSkillQuickSlotWidget& SourceSlot)
{
	UTexture2D* Icon = SourceSlot.GetSkillIcon();
	if (!IsValid(Icon))
	{
		return nullptr;
	}

	UImage* DragVisual = NewObject<UImage>(this);
	DragVisual->SetBrushFromTexture(Icon);
	DragVisual->SetDesiredSizeOverride(FVector2D(SourceSlot.GetCachedGeometry().GetLocalSize().X));

	UMKHSkillDragOperation* Operation = NewObject<UMKHSkillDragOperation>(this);
	Operation->InitSkillDrag(this, &SourceSlot);
	Operation->DefaultDragVisual = DragVisual;
	Operation->Pivot = EDragPivot::CenterCenter;

	for (UMKHSkillQuickSlotWidget* Skill : SkillSlots)
	{
		if (IsValid(Skill))
		{
			Skill->SetReorderLook(Skill != &SourceSlot, false);
		}
	}
	SourceSlot.ShowPreviewIcon(nullptr, 1.f);
	return Operation;
}

void UMKHInventoryWeaponSkillsWidget::EndSkillDrag()
{
	for (UMKHSkillQuickSlotWidget* Skill : SkillSlots)
	{
		if (IsValid(Skill))
		{
			Skill->SetReorderLook(false, false);
			Skill->ClearPreviewIcon();
		}
	}
}

void UMKHInventoryWeaponSkillsWidget::HandleSkillDragEnter(UMKHSkillQuickSlotWidget& TargetSlot, const UDragDropOperation* Operation)
{
	UMKHSkillQuickSlotWidget* Source = GetOwnDragSource(Operation);
	if (!IsValid(Source) || Source == &TargetSlot)
	{
		return;
	}

	// The two icons as a drop would leave them; ClearPreviewIcon does not run here so the source keeps its hidden look.
	UTexture2D* Displaced = TargetSlot.GetSkillIcon();
	TargetSlot.ShowPreviewIcon(Source->GetSkillIcon(), 1.f);
	Source->ShowPreviewIcon(Displaced, PreviewDisplacedOpacity);
}

void UMKHInventoryWeaponSkillsWidget::HandleSkillDragLeave(UMKHSkillQuickSlotWidget& TargetSlot, const UDragDropOperation* Operation)
{
	UMKHSkillQuickSlotWidget* Source = GetOwnDragSource(Operation);
	if (!IsValid(Source) || Source == &TargetSlot)
	{
		return;
	}

	TargetSlot.ClearPreviewIcon();
	Source->ShowPreviewIcon(nullptr, 1.f);
}

bool UMKHInventoryWeaponSkillsWidget::HandleSkillDrop(const UMKHSkillQuickSlotWidget& TargetSlot, const UDragDropOperation* Operation)
{
	const UMKHSkillQuickSlotWidget* Source = GetOwnDragSource(Operation);
	if (!IsValid(Source))
	{
		return false;
	}

	RequestSwap(*Source, TargetSlot);
	return true;
}

void UMKHInventoryWeaponSkillsWidget::HandlePadAccept(UMKHSkillQuickSlotWidget& FocusedSlot)
{
	if (const UMKHSkillQuickSlotWidget* Held = HeldSlot.Get())
	{
		if (Held != &FocusedSlot)
		{
			RequestSwap(*Held, FocusedSlot);
		}
		HeldSlot.Reset();
	}
	else if (FocusedSlot.IsSlotFilled())
	{
		HeldSlot = &FocusedSlot;
	}
	RefreshPadLook();
}

bool UMKHInventoryWeaponSkillsWidget::HandlePadBack()
{
	if (!HeldSlot.IsValid())
	{
		return false;
	}

	HeldSlot.Reset();
	RefreshPadLook();
	return true;
}

void UMKHInventoryWeaponSkillsWidget::HandleSlotFocusChanged()
{
	RefreshPadLook();
	if (HeldSlot.IsValid())
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { EndHoldIfFocusLeft(); }));
		}
	}
}

// ============================================================
// Private helpers
// ============================================================

UMKHSkillQuickSlotWidget* UMKHInventoryWeaponSkillsWidget::GetOwnDragSource(const UDragDropOperation* Operation) const
{
	const UMKHSkillDragOperation* SkillDrag = Cast<UMKHSkillDragOperation>(Operation);
	return IsValid(SkillDrag) && SkillDrag->GetRow() == this ? SkillDrag->GetSourceSlot() : nullptr;
}

void UMKHInventoryWeaponSkillsWidget::OpenHitTestPathToSlots()
{
	const auto OpenChain = [](UWidget* First)
	{
		for (UWidget* Widget = First; IsValid(Widget); Widget = Widget->GetParent())
		{
			if (Widget->GetVisibility() == ESlateVisibility::HitTestInvisible)
			{
				Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}
	};

	for (UMKHSkillQuickSlotWidget* Skill : SkillSlots)
	{
		if (IsValid(Skill))
		{
			OpenChain(Skill->GetParent());
		}
	}
	OpenChain(this);
}

void UMKHInventoryWeaponSkillsWidget::RequestSwap(const UMKHSkillQuickSlotWidget& From, const UMKHSkillQuickSlotWidget& To) const
{
	if (UInventoryDashboardWidget* OwningDashboard = Dashboard.Get(); IsValid(OwningDashboard) && &From != &To)
	{
		OwningDashboard->RequestSwapWeaponSkills(GetWeaponItemID(), From.GetSkillSlotTag(), To.GetSkillSlotTag());
	}
}

void UMKHInventoryWeaponSkillsWidget::RefreshPadLook() const
{
	const bool bPadActive = UMKHMenuNavigationLibrary::IsGamepadNavigationActive(this);
	APlayerController* Player = GetOwningPlayer();
	bool bFocusedSlotFilled = false;
	for (UMKHSkillQuickSlotWidget* Skill : SkillSlots)
	{
		if (IsValid(Skill))
		{
			const bool bHeld = HeldSlot.Get() == Skill;
			const bool bFocused = bPadActive && Skill->HasUserFocus(Player);
			Skill->SetReorderLook(bHeld || bFocused, bHeld);
			bFocusedSlotFilled |= bFocused && Skill->IsSlotFilled();
		}
	}
	RefreshPadHints(bFocusedSlotFilled);
}

void UMKHInventoryWeaponSkillsWidget::RefreshPadHints(bool bFocusedSlotFilled) const
{
	const bool bHolding = HeldSlot.IsValid();
	if (IsValid(AcceptHint))
	{
		AcceptHint->SetHintText(bHolding ? LOCTEXT("PlaceSkillHint", "Place here") : LOCTEXT("TakeSkillHint", "Take skill"));
		AcceptHint->SetHintEnabled(bHolding || bFocusedSlotFilled);
	}
	if (IsValid(CancelHint))
	{
		CancelHint->SetHintEnabled(bHolding);
	}
}

void UMKHInventoryWeaponSkillsWidget::EndHoldIfFocusLeft()
{
	APlayerController* Player = GetOwningPlayer();
	const bool bFocusInRow = SkillSlots.ContainsByPredicate([Player](const UMKHSkillQuickSlotWidget* Skill)
	{
		return IsValid(Skill) && Skill->HasUserFocus(Player);
	});
	if (!bFocusInRow)
	{
		HeldSlot.Reset();
		RefreshPadLook();
	}
}

#undef LOCTEXT_NAMESPACE
