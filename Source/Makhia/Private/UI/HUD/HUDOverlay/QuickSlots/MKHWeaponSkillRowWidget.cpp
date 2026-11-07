// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/QuickSlots/MKHWeaponSkillRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "MKHLogChannels.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHSkillQuickSlotWidget.h"
#include "UI/HUD/HUDOverlay/QuickSlots/MKHWeaponQuickSlotWidget.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHWeaponSkillRowWidget::NativePreConstruct()
{
	CacheRowSlots();

	Super::NativePreConstruct();

	PushRowSettingsToWeaponSlot();
}

void UMKHWeaponSkillRowWidget::NativeOnInitialized()
{
	CacheRowSlots();

	Super::NativeOnInitialized();

	SetRenderTransformPivot(InactiveScalePivot);
	ApplyInactiveBlend(bRowActive ? 0.f : 1.f);
}

void UMKHWeaponSkillRowWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float TargetBlend = bRowActive ? 0.f : 1.f;
	if (CurrentBlend == TargetBlend)
	{
		return;
	}

	const float Step = TransitionDuration > 0.f ? InDeltaTime / TransitionDuration : 1.f;
	ApplyInactiveBlend(FMath::Abs(TargetBlend - CurrentBlend) <= Step
		? TargetBlend
		: CurrentBlend + FMath::Sign(TargetBlend - CurrentBlend) * Step);
}

// ============================================================
// Public Interface
// ============================================================

void UMKHWeaponSkillRowWidget::PlayActivationFlash(const FGameplayTag& InQuickSlotTag)
{
	if (IsValid(WeaponSlot) && InQuickSlotTag.MatchesTag(WeaponSlot->GetQuickSlotTag()))
	{
		FlashWeaponSlot();
		return;
	}

	if (bRowActive)
	{
		FlashSkillSlot(InQuickSlotTag);
	}
}

void UMKHWeaponSkillRowWidget::SetRowActive(const bool bInActive, const bool bAnimate)
{
	bRowActive = bInActive;

	if (!bAnimate)
	{
		ApplyInactiveBlend(bRowActive ? 0.f : 1.f);
	}
}

bool UMKHWeaponSkillRowWidget::HasWeapon() const
{
	return IsValid(WeaponSlot) && WeaponSlot->IsSlotFilled();
}

UMKHWeaponQuickSlotWidget* UMKHWeaponSkillRowWidget::GetWeaponSlot()
{
	// The parent W_WeaponHUD may gather before this row has initialized.
	CacheRowSlots();
	return WeaponSlot;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHWeaponSkillRowWidget::GatherQuickSlots(TArray<UMKHQuickSlotWidget*>& OutSlots) const
{
	if (IsValid(WeaponSlot))
	{
		OutSlots.Add(WeaponSlot);
	}
}

void UMKHWeaponSkillRowWidget::AssignSlotItem(UMKHQuickSlotWidget& QuickSlot, UInventoryItem* Item)
{
	Super::AssignSlotItem(QuickSlot, Item);

	for (UMKHSkillQuickSlotWidget* SkillSlot : SkillSlots)
	{
		if (IsValid(SkillSlot))
		{
			SkillSlot->SetSlotItem(Item);
		}
	}
}

void UMKHWeaponSkillRowWidget::ClearSlotItem(UMKHQuickSlotWidget& QuickSlot)
{
	Super::ClearSlotItem(QuickSlot);

	for (UMKHSkillQuickSlotWidget* SkillSlot : SkillSlots)
	{
		if (IsValid(SkillSlot))
		{
			SkillSlot->ResetSlot();
		}
	}
}

void UMKHWeaponSkillRowWidget::CacheRowSlots()
{
	if (IsValid(WeaponSlot) || !IsValid(WidgetTree))
	{
		return;
	}

	SkillSlots.Reset();
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (UMKHWeaponQuickSlotWidget* Weapon = Cast<UMKHWeaponQuickSlotWidget>(Widget); IsValid(Weapon) && !IsValid(WeaponSlot))
		{
			WeaponSlot = Weapon;
		}
		else if (UMKHSkillQuickSlotWidget* Skill = Cast<UMKHSkillQuickSlotWidget>(Widget))
		{
			Skill->SkillIndex = SkillSlots.Num();
			SkillSlots.Add(Skill);
		}
	});

	if (!IsValid(WeaponSlot))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: no weapon quick slot in the row, weapons will not be shown."), *GetName());
		return;
	}

	// Before the first construct too: starting loadout preferences quick slot weapons before the HUD is on screen.
	PushRowSettingsToWeaponSlot();
}

void UMKHWeaponSkillRowWidget::PushRowSettingsToWeaponSlot() const
{
	if (IsValid(WeaponSlot))
	{
		WeaponSlot->SetQuickSlotTag(QuickSlotTag);
		WeaponSlot->SetInputAction(WeaponInputAction);
	}
}

void UMKHWeaponSkillRowWidget::FlashWeaponSlot() const
{
	// Pressing the key of the weapon already in hand does nothing, so it does not flash either.
	const UInventoryItem* Weapon = WeaponSlot->GetSlotItem();
	if (!IsValid(Weapon) || !Weapon->IsEquipped())
	{
		WeaponSlot->PlayActivationFlash();
	}
}

void UMKHWeaponSkillRowWidget::FlashSkillSlot(const FGameplayTag& InputTag) const
{
	for (UMKHSkillQuickSlotWidget* SkillSlot : SkillSlots)
	{
		if (IsValid(SkillSlot) && SkillSlot->InputTag.MatchesTagExact(InputTag))
		{
			SkillSlot->PlayActivationFlash();
			return;
		}
	}
}

void UMKHWeaponSkillRowWidget::ApplyInactiveBlend(const float Blend)
{
	CurrentBlend = Blend;

	const float Eased = FMath::SmoothStep(0.f, 1.f, Blend);
	SetColorAndOpacity(FMath::Lerp(FLinearColor::White, InactiveTint, Eased));
	SetRenderScale(FVector2D(FMath::Lerp(1.f, InactiveScale, Eased)));
}
