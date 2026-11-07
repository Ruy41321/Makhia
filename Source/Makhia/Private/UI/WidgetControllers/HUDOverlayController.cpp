// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetControllers/HUDOverlayController.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Engine/World.h"
#include "Interfaces/InventoryInterface.h"
#include "Interfaces/EquipmentInterface.h"
#include "Interfaces/QuickSlotInterface.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Inventory/InventoryItem/InventoryItem.h"
#include "Player/PlayerController/MKHPlayerController.h"
#include "QuickSlot/QuickSlotManagerComponent.h"
#include "TimerManager.h"
#include "UI/HUD/HUDOverlay/HUDOverlayWidget.h"

// ============================================================
// Lifecycle
// ============================================================

void UHUDOverlayController::BindCallbacksToDependencies()
{
	BindActivationCallbacks();
	BindEquipmentCallbacks();
	BindInventoryCallbacks();
	BindQuickSlotManagerCallbacks();
}

void UHUDOverlayController::BroadcastInitialValues()
{
	if (!EnsureOwningInventory())
	{
		return;
	}

	for (const FRPGInventoryEntry& Entry : OwningInventoryComp->GetInventoryEntries())
	{
		if (Entry.IsValid() && Entry.bIsQuickSlotted)
		{
			UInventoryItem* Item = UInventoryItem::CreateFromInventoryEntry(this, Entry);
			HUDItemQuickSlottedDelegate.Broadcast(Item);
		}
	}

	// The new overlay starts with every row active: send the current state even when it did not change.
	ActiveWeaponSlotTag = ComputeActiveWeaponSlotTag();
	HUDActiveWeaponSlotChangedDelegate.Broadcast(ActiveWeaponSlotTag);
}

void UHUDOverlayController::BindDelegatesToWidget_Implementation()
{
	if (!IsValid(OwningHUDOverlayWidget))
	{
		return;
	}

	HUDItemQuickSlottedDelegate.AddDynamic(OwningHUDOverlayWidget, &UHUDOverlayWidget::OnItemQuickSlotted);
	HUDItemChangedDelegate.AddDynamic(OwningHUDOverlayWidget, &UHUDOverlayWidget::OnItemChanged);
	HUDItemRemovedDelegate.AddDynamic(OwningHUDOverlayWidget, &UHUDOverlayWidget::OnItemRemoved);
	HUDActiveWeaponSlotChangedDelegate.AddDynamic(OwningHUDOverlayWidget, &UHUDOverlayWidget::OnActiveWeaponSlotChanged);
}

void UHUDOverlayController::UnbindAllEventsFromDelegates()
{
	HUDItemQuickSlottedDelegate.Clear();
	HUDItemChangedDelegate.Clear();
	HUDItemRemovedDelegate.Clear();
	HUDActiveWeaponSlotChangedDelegate.Clear();
}

// ============================================================
// Public Interface
// ============================================================

void UHUDOverlayController::SetOwningInventoryComponent()
{
	OwningInventoryComp = IInventoryInterface::Execute_GetInventoryComponent(OwningActor);
}

void UHUDOverlayController::SetOwningEquipmentComponent()
{
	OwningEquipmentComp = IEquipmentInterface::Execute_GetEquipmentManagerComponent(OwningActor);
}

void UHUDOverlayController::SetOwningQuickSlotManagerComp()
{
	OwningQuickSlotManagerComp = IQuickSlotInterface::Execute_GetQuickSlotManagerComponent(OwningActor);
}

void UHUDOverlayController::SetHUDOverlayWidget(UHUDOverlayWidget* InHUDOverlayWidget)
{
	OwningHUDOverlayWidget = InHUDOverlayWidget;

	if (IsValid(OwningHUDOverlayWidget) && IsValid(OwningInventoryComp))
	{
		BindDelegatesToWidget();
	}
}

// ============================================================
// Protected / Internal Logic (bindings)
// ============================================================

void UHUDOverlayController::BindActivationCallbacks()
{
	if (AMKHPlayerController* PC = Cast<AMKHPlayerController>(OwningActor))
	{
		PC->SkillActivatedDelegate.BindLambda(
			[this](const FGameplayTag& SkillTag)
			{
				if (SkillTag.MatchesTag(MKHGameplayTags::Input::Skill) && IsValid(OwningHUDOverlayWidget))
				{
					OwningHUDOverlayWidget->OnQuickSlotActivated(SkillTag);
				}
			});
	}

	if (EnsureOwningQuickSlotManagerComp())
	{
		OwningQuickSlotManagerComp->QuickSlotActivatedDelegate.BindLambda(
			[this](const FGameplayTag& QuickSlotTag)
			{
				if (IsValid(OwningHUDOverlayWidget))
				{
					OwningHUDOverlayWidget->OnQuickSlotActivated(QuickSlotTag);
				}
			});
	}
}

void UHUDOverlayController::BindEquipmentCallbacks()
{
	if (!EnsureOwningEquipment())
	{
		return;
	}

	OwningEquipmentComp->EquipmentList.EquipmentEntryDelegate.AddUObject(this, &UHUDOverlayController::HandleEquipmentEntryChanged);
	OwningEquipmentComp->EquipmentList.UnEquippedEntryDelegate.AddUObject(this, &UHUDOverlayController::HandleEquipmentEntryChanged);
}

void UHUDOverlayController::BindInventoryCallbacks()
{
	if (!EnsureOwningInventory())
	{
		return;
	}

	OwningInventoryComp->InventoryList.QuickSlotItemRelocatedDelegate.AddLambda(
		[this](const FRPGInventoryEntry& InventoryEntry)
		{
			UInventoryItem* Item = UInventoryItem::CreateFromInventoryEntry(this, InventoryEntry);
			HUDItemQuickSlottedDelegate.Broadcast(Item);
			RequestActiveWeaponSlotRefresh();
		});

	OwningInventoryComp->InventoryList.QuickSlotItemChangeDelegate.AddLambda(
		[this](const FRPGInventoryEntry& InventoryEntry)
		{
			UInventoryItem* Item = UInventoryItem::CreateFromInventoryEntry(this, InventoryEntry);
			HUDItemChangedDelegate.Broadcast(Item);
		});

	OwningInventoryComp->InventoryList.QuickSlotItemRemovedDelegate.AddLambda(
		[this](const int64 RemovedItemID)
		{
			HUDItemRemovedDelegate.Broadcast(RemovedItemID);
			RequestActiveWeaponSlotRefresh();
		});
}

void UHUDOverlayController::BindQuickSlotManagerCallbacks()
{
	if (!EnsureOwningQuickSlotManagerComp())
	{
		return;
	}

	OwningQuickSlotManagerComp->OnQuickEquipWeaponSlotChanged.AddWeakLambda(this,
		[this](const FGameplayTag&)
		{
			RequestActiveWeaponSlotRefresh();
		});
	OwningQuickSlotManagerComp->OnQuickSlotsChanged.AddUObject(this, &UHUDOverlayController::RequestActiveWeaponSlotRefresh);
}

void UHUDOverlayController::HandleEquipmentEntryChanged(const FRPGEquipmentEntry& Entry)
{
	if (Entry.SlotTag.MatchesTagExact(MKHGameplayTags::Equip::WeaponSlot))
	{
		RequestActiveWeaponSlotRefresh();
	}
}

// ============================================================
// Protected / Internal Logic (active weapon slot)
// ============================================================

void UHUDOverlayController::RequestActiveWeaponSlotRefresh()
{
	if (bActiveWeaponSlotRefreshPending)
	{
		return;
	}

	UWorld* World = IsValid(OwningActor) ? OwningActor->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		RefreshActiveWeaponSlot();
		return;
	}

	bActiveWeaponSlotRefreshPending = true;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UHUDOverlayController::RefreshActiveWeaponSlot));
}

void UHUDOverlayController::RefreshActiveWeaponSlot()
{
	bActiveWeaponSlotRefreshPending = false;

	const FGameplayTag NewActiveWeaponSlotTag = ComputeActiveWeaponSlotTag();
	if (NewActiveWeaponSlotTag == ActiveWeaponSlotTag)
	{
		return;
	}

	ActiveWeaponSlotTag = NewActiveWeaponSlotTag;
	HUDActiveWeaponSlotChangedDelegate.Broadcast(ActiveWeaponSlotTag);
}

FGameplayTag UHUDOverlayController::ComputeActiveWeaponSlotTag()
{
	if (!EnsureOwningQuickSlotManagerComp())
	{
		return FGameplayTag();
	}

	const FGameplayTag EquippedSlotTag = OwningQuickSlotManagerComp->FindQuickSlotTagByItemID(GetEquippedWeaponItemID());
	if (EquippedSlotTag.MatchesTag(MKHGameplayTags::Input::WeaponQuickSlotCategory))
	{
		return EquippedSlotTag;
	}

	const FGameplayTag QuickEquipSlotTag = OwningQuickSlotManagerComp->GetQuickEquipWeaponSlotTag();
	if (QuickEquipSlotTag.IsValid() && OwningQuickSlotManagerComp->GetQuickSlotItemID(QuickEquipSlotTag) != 0)
	{
		return QuickEquipSlotTag;
	}

	// The quick-equip slot may lag behind a replicated quick slot change: fall back to the filled weapon slot.
	const FGameplayTag WeaponSlotTags[] = { MKHGameplayTags::Equip::WeaponQuickSlot1, MKHGameplayTags::Equip::WeaponQuickSlot2 };
	for (const FGameplayTag& WeaponSlotTag : WeaponSlotTags)
	{
		if (OwningQuickSlotManagerComp->GetQuickSlotItemID(WeaponSlotTag) != 0)
		{
			return WeaponSlotTag;
		}
	}
	return FGameplayTag();
}

int64 UHUDOverlayController::GetEquippedWeaponItemID()
{
	if (!EnsureOwningEquipment())
	{
		return 0;
	}

	const FRPGEquipmentEntry* Entry = OwningEquipmentComp->EquipmentList.FindEntryBySlot(MKHGameplayTags::Equip::WeaponSlot);
	return Entry ? Entry->Item.ItemID : 0;
}

bool UHUDOverlayController::EnsureOwningInventory()
{
	if (!IsValid(OwningInventoryComp))
	{
		SetOwningInventoryComponent();
	}
	return IsValid(OwningInventoryComp);
}

bool UHUDOverlayController::EnsureOwningEquipment()
{
	if (!IsValid(OwningEquipmentComp))
	{
		SetOwningEquipmentComponent();
	}
	return IsValid(OwningEquipmentComp);
}

bool UHUDOverlayController::EnsureOwningQuickSlotManagerComp()
{
	if (!IsValid(OwningQuickSlotManagerComp))
	{
		SetOwningQuickSlotManagerComp();
	}
	return IsValid(OwningQuickSlotManagerComp);
}
