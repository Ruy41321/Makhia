// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHSettingsPageWidget.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHSettingsPageWidget::ActivatePage()
{
	if (bIsPageActive)
	{
		return;
	}

	bIsPageActive = true;
	NativeOnPageActivated();
	BP_OnPageActivated();
	NotifyDirtyStateChanged();
}

void UMKHSettingsPageWidget::DeactivatePage()
{
	if (!bIsPageActive)
	{
		return;
	}

	bIsPageActive = false;
	NativeOnPageDeactivated();
	BP_OnPageDeactivated();
}

void UMKHSettingsPageWidget::ApplyChanges()
{
	// Pages with no persistent model of their own have nothing to write.
}

void UMKHSettingsPageWidget::RevertChanges()
{
}

void UMKHSettingsPageWidget::ResetToDefaults()
{
}

bool UMKHSettingsPageWidget::HasPendingChanges() const
{
	return false;
}

bool UMKHSettingsPageWidget::UsesActionBar() const
{
	// Most settings pages stage their edits and need an explicit Apply; opting out is the exception.
	return true;
}

bool UMKHSettingsPageWidget::IsBlockingClose() const
{
	return false;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSettingsPageWidget::NativeOnPageActivated()
{
}

void UMKHSettingsPageWidget::NativeOnPageDeactivated()
{
}

void UMKHSettingsPageWidget::NotifyDirtyStateChanged() const
{
	OnPageDirtyStateChanged.Broadcast();
}
