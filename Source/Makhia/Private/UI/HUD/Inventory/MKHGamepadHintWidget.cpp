// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHGamepadHintWidget.h"

#include "Components/TextBlock.h"
#include "Input/MKHActiveInputDeviceSubsystem.h"
#include "Libraries/MKHInputSettingsLibrary.h"
#include "UI/HUD/HUDOverlay/MKHKeyHintWidget.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHGamepadHintWidget::SetHintText(const FText& InHintText)
{
	HintText = InHintText;
	if (IsValid(HintLabel))
	{
		HintLabel->SetText(HintText);
	}
}

void UMKHGamepadHintWidget::SetHintEnabled(bool bEnabled)
{
	bHintEnabled = bEnabled;
	ApplyDevice(UMKHInputSettingsLibrary::GetActiveInputDevice(GetOwningPlayer()));
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHGamepadHintWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(HintLabel))
	{
		HintLabel->SetText(HintText);
	}
	if (IsValid(KeyCap))
	{
		KeyCap->SetFixedGamepadKey(GamepadKey);
	}
}

void UMKHGamepadHintWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = UMKHInputSettingsLibrary::GetActiveInputDeviceSubsystem(GetOwningPlayer()); IsValid(DeviceSubsystem))
	{
		BoundDeviceSubsystem = DeviceSubsystem;
		DeviceSubsystem->OnActiveInputDeviceChanged.AddUniqueDynamic(this, &ThisClass::HandleActiveInputDeviceChanged);
	}
	ApplyDevice(UMKHInputSettingsLibrary::GetActiveInputDevice(GetOwningPlayer()));
}

void UMKHGamepadHintWidget::NativeDestruct()
{
	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = BoundDeviceSubsystem.Get())
	{
		DeviceSubsystem->OnActiveInputDeviceChanged.RemoveDynamic(this, &ThisClass::HandleActiveInputDeviceChanged);
	}
	BoundDeviceSubsystem.Reset();

	Super::NativeDestruct();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHGamepadHintWidget::HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice)
{
	ApplyDevice(NewDevice);
}

void UMKHGamepadHintWidget::ApplyDevice(EMKHInputDevice Device)
{
	SetVisibility(bHintEnabled && Device == EMKHInputDevice::Gamepad ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
