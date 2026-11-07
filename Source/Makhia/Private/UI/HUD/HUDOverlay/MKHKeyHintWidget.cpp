// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/MKHKeyHintWidget.h"

#include "Components/TextBlock.h"
#include "Input/MKHActiveInputDeviceSubsystem.h"
#include "InputAction.h"
#include "Libraries/MKHInputSettingsLibrary.h"
#include "MKHLogChannels.h"
#include "UserSettings/EnhancedInputUserSettings.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHKeyHintWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(KeyBackground))
	{
		KeyBackground->SetVisibility(bShowBackground ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMKHKeyHintWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindInputSettings();
	RefreshKeyText();
}

void UMKHKeyHintWidget::NativeDestruct()
{
	UnbindInputSettings();

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHKeyHintWidget::RefreshKeyText()
{
	// The designer has no player to ask: keep the placeholder key authored in the widget.
	if (!IsValid(KeyText) || IsDesignTime())
	{
		return;
	}

	const bool bUsingGamepad = UMKHInputSettingsLibrary::GetActiveInputDevice(GetOwningPlayer()) == EMKHInputDevice::Gamepad;
	if (bGamepadOnly)
	{
		SetVisibility(bUsingGamepad ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bUsingGamepad && FixedGamepadKey.IsValid())
	{
		KeyText->SetText(UMKHInputSettingsLibrary::GetKeyDisplayName(FixedGamepadKey));
		return;
	}

	const UInputAction* ShownAction = bUsingGamepad && IsValid(GamepadInputAction) ? GamepadInputAction.Get() : InputAction.Get();
	if (!IsValid(ShownAction))
	{
		UE_LOG(LogMKHInput, Verbose, TEXT("%s: no InputAction set, the keycap stays empty."), *GetName());
	}

	KeyText->SetText(UMKHInputSettingsLibrary::GetActionKeyDisplayNameForActiveDevice(GetOwningPlayer(), ShownAction, bShowChord));
}

void UMKHKeyHintWidget::SetInputAction(UInputAction* InInputAction)
{
	InputAction = InInputAction;
	RefreshKeyText();
}

void UMKHKeyHintWidget::SetFixedGamepadKey(const FKey& InKey)
{
	FixedGamepadKey = InKey;
	RefreshKeyText();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHKeyHintWidget::HandleInputSettingsApplied()
{
	RefreshKeyText();
}

void UMKHKeyHintWidget::HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice)
{
	RefreshKeyText();
}

void UMKHKeyHintWidget::BindInputSettings()
{
	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = UMKHInputSettingsLibrary::GetActiveInputDeviceSubsystem(GetOwningPlayer()); IsValid(DeviceSubsystem))
	{
		BoundDeviceSubsystem = DeviceSubsystem;
		DeviceSubsystem->OnActiveInputDeviceChanged.AddUniqueDynamic(this, &UMKHKeyHintWidget::HandleActiveInputDeviceChanged);
	}

	UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	if (!IsValid(UserSettings))
	{
		return;
	}

	BoundInputSettings = UserSettings;
	UserSettings->OnSettingsApplied.AddUniqueDynamic(this, &UMKHKeyHintWidget::HandleInputSettingsApplied);
}

void UMKHKeyHintWidget::UnbindInputSettings()
{
	if (UEnhancedInputUserSettings* UserSettings = BoundInputSettings.Get())
	{
		UserSettings->OnSettingsApplied.RemoveDynamic(this, &UMKHKeyHintWidget::HandleInputSettingsApplied);
	}
	BoundInputSettings.Reset();

	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = BoundDeviceSubsystem.Get())
	{
		DeviceSubsystem->OnActiveInputDeviceChanged.RemoveDynamic(this, &UMKHKeyHintWidget::HandleActiveInputDeviceChanged);
	}
	BoundDeviceSubsystem.Reset();
}
