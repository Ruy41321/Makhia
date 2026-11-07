// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/QuickSlots/MKHQuickSlotWidget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Data/MKHDataSubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Equipment/Rarity/RarityDefinition.h"
#include "InputAction.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "Materials/MaterialInstance.h"
#include "MKHLogChannels.h"
#include "TimerManager.h"
#include "UI/HUD/HUDOverlay/MKHKeyHintWidget.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHQuickSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(KeyCap))
	{
		KeyCap->SetInputAction(InputAction);
	}
}

void UMKHQuickSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(SlotIcon))
	{
		DefaultIconBrush = SlotIcon->GetBrush();
	}
	if (IsValid(SlotBg))
	{
		DefaultBgBrush = SlotBg->GetBrush();
	}
	bDefaultBrushesCached = true;
}

void UMKHQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The slot may have received its item before being added to the screen.
	RefreshSlot();
}

void UMKHQuickSlotWidget::NativeDestruct()
{
	StopCooldownTracking();
	UnbindItem();

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHQuickSlotWidget::SetSlotItem(UInventoryItem* InItem)
{
	UnbindItem();
	BindItem(InItem);
	RefreshSlot();
}

void UMKHQuickSlotWidget::ResetSlot()
{
	UnbindItem();
	RefreshSlot();
}

void UMKHQuickSlotWidget::RefreshSlot()
{
	const bool bFilled = HasSlotContent();
	if (bFilled)
	{
		ApplyFilledVisuals();
	}
	else
	{
		ApplyEmptyVisuals();
	}

	BP_OnSlotRefreshed(bFilled);
}

void UMKHQuickSlotWidget::SetInputAction(UInputAction* InInputAction)
{
	InputAction = InInputAction;

	if (IsValid(KeyCap))
	{
		KeyCap->SetInputAction(InputAction);
	}
}

bool UMKHQuickSlotWidget::IsSlotFilled() const
{
	return HasSlotContent();
}

int64 UMKHQuickSlotWidget::GetSlotItemID() const
{
	return IsValid(InventoryItem) ? InventoryItem->GetItemID() : 0;
}

void UMKHQuickSlotWidget::PlayActivationFlash()
{
	BP_PlayActivationFlash();
}

// ============================================================
// Protected / Internal Logic (subclass hooks)
// ============================================================

bool UMKHQuickSlotWidget::HasSlotContent() const
{
	return IsValid(InventoryItem);
}

FGameplayTag UMKHQuickSlotWidget::ResolveCooldownTag() const
{
	return FGameplayTag();
}

int32 UMKHQuickSlotWidget::ResolveDisplayedQuantity() const
{
	return 0;
}

void UMKHQuickSlotWidget::HandleItemUpdated()
{
	RefreshSlot();
}

// ============================================================
// Protected / Internal Logic (look)
// ============================================================

void UMKHQuickSlotWidget::ApplyFilledVisuals()
{
	if (IsValid(SlotIcon))
	{
		SlotIcon->SetBrushFromTexture(ResolveSlotIcon());
		SlotIcon->SetBrushTintColor(FSlateColor(FLinearColor::White));
	}

	ApplyRarityBackground();
	UpdateQuantityText();
	StartCooldownTracking(ResolveCooldownTag());
}

void UMKHQuickSlotWidget::ApplyEmptyVisuals()
{
	StopCooldownTracking();

	if (IsValid(SlotIcon))
	{
		if (bDefaultBrushesCached)
		{
			SlotIcon->SetBrush(DefaultIconBrush);
		}
		if (IsValid(EmptyIconTexture))
		{
			SlotIcon->SetBrushFromTexture(EmptyIconTexture);
		}
		SlotIcon->SetBrushTintColor(FSlateColor(EmptyIconTint));
	}

	if (bUseRarityBackground && IsValid(SlotBg))
	{
		if (bDefaultBrushesCached)
		{
			SlotBg->SetBrush(DefaultBgBrush);
		}
		SlotBg->SetBrushTintColor(FSlateColor(EmptyBackgroundTint));
	}

	UpdateQuantityText();
}

void UMKHQuickSlotWidget::ApplyRarityBackground() const
{
	if (!bUseRarityBackground || !IsValid(SlotBg) || !IsValid(InventoryItem))
	{
		return;
	}

	FRarityDefinition Rarity;
	if (!UMKHDataSubsystem::FindRarityDefinitionForContext(this, InventoryItem->GetRarityTag(), Rarity))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: no rarity definition for %s, background left unchanged."),
			*GetName(), *InventoryItem->GetRarityTag().ToString());
		return;
	}

	if (IsValid(Rarity.RarityColors.MI_Square))
	{
		SlotBg->SetBrushFromMaterial(Rarity.RarityColors.MI_Square);
	}
	SlotBg->SetBrushTintColor(FSlateColor(FLinearColor::White));
}

void UMKHQuickSlotWidget::UpdateQuantityText() const
{
	if (!IsValid(QuantityText))
	{
		return;
	}

	const int32 Quantity = HasSlotContent() ? ResolveDisplayedQuantity() : 0;
	QuantityText->SetText(FText::AsNumber(Quantity));
	QuantityText->SetVisibility(Quantity > 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

// ============================================================
// Protected / Internal Logic (item binding)
// ============================================================

void UMKHQuickSlotWidget::BindItem(UInventoryItem* InItem)
{
	InventoryItem = InItem;

	if (IsValid(InventoryItem))
	{
		InventoryItem->OnItemUpdatedDelegate.AddUniqueDynamic(this, &UMKHQuickSlotWidget::HandleItemUpdated);
	}
}

void UMKHQuickSlotWidget::UnbindItem()
{
	if (IsValid(InventoryItem))
	{
		InventoryItem->OnItemUpdatedDelegate.RemoveDynamic(this, &UMKHQuickSlotWidget::HandleItemUpdated);
	}
	InventoryItem = nullptr;
}

// ============================================================
// Protected / Internal Logic (cooldown)
// ============================================================

void UMKHQuickSlotWidget::StartCooldownTracking(const FGameplayTag& CooldownTag)
{
	if (CooldownTag == TrackedCooldownTag && TrackedAbilitySystem.IsValid())
	{
		return;
	}

	StopCooldownTracking();

	if (!CooldownTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = GetOwnerAbilitySystem();
	if (!IsValid(AbilitySystem))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("%s: no ability system on the owning player, cooldown %s not shown."),
			*GetName(), *CooldownTag.ToString());
		return;
	}

	TrackedCooldownTag = CooldownTag;
	TrackedAbilitySystem = AbilitySystem;
	CooldownTagEventHandle = AbilitySystem->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UMKHQuickSlotWidget::HandleCooldownTagChanged);

	// The cooldown may already be running (e.g. the item was moved to this slot mid-cooldown).
	if (AbilitySystem->GetTagCount(CooldownTag) > 0)
	{
		BeginCooldownDisplay();
	}
}

void UMKHQuickSlotWidget::StopCooldownTracking()
{
	if (UAbilitySystemComponent* AbilitySystem = TrackedAbilitySystem.Get(); IsValid(AbilitySystem) && CooldownTagEventHandle.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(TrackedCooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(CooldownTagEventHandle);
	}

	CooldownTagEventHandle.Reset();
	TrackedAbilitySystem.Reset();
	TrackedCooldownTag = FGameplayTag();

	EndCooldownDisplay(false);
}

void UMKHQuickSlotWidget::HandleCooldownTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		BeginCooldownDisplay();
	}
	else
	{
		EndCooldownDisplay(true);
	}
}

void UMKHQuickSlotWidget::BeginCooldownDisplay()
{
	if (IsValid(CDOverlay))
	{
		CDOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	UpdateCooldownDisplay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CooldownRefreshTimer, this, &UMKHQuickSlotWidget::UpdateCooldownDisplay,
			CooldownRefreshInterval, true);
	}
}

void UMKHQuickSlotWidget::EndCooldownDisplay(bool bFinished)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownRefreshTimer);
	}

	if (IsValid(CDOverlay))
	{
		CDOverlay->SetVisibility(ESlateVisibility::Hidden);
	}

	if (bFinished)
	{
		BP_OnCooldownFinished();
	}
}

void UMKHQuickSlotWidget::UpdateCooldownDisplay()
{
	const UMKHAbilitySystemComponent* AbilitySystem = Cast<UMKHAbilitySystemComponent>(TrackedAbilitySystem.Get());
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	float TimeRemaining = 0.f;
	float Duration = 0.f;
	AbilitySystem->GetCooldownRemainingForTag(TrackedCooldownTag, TimeRemaining, Duration);

	if (IsValid(CDProgress))
	{
		CDProgress->SetPercent(Duration > 0.f ? TimeRemaining / Duration : 0.f);
	}
	if (IsValid(CDTime))
	{
		CDTime->SetText(FText::AsNumber(FMath::CeilToInt(TimeRemaining)));
	}
}

UAbilitySystemComponent* UMKHQuickSlotWidget::GetOwnerAbilitySystem() const
{
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwningPlayer());
}
