// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/MKHIntroSkipHintWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Input/MKHActiveInputDeviceSubsystem.h"
#include "InputAction.h"
#include "Libraries/MKHInputSettingsLibrary.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHIntroSkipHintWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	RefreshPrompt();
}

void UMKHIntroSkipHintWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetHoldProgress(0.f);

	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = UMKHInputSettingsLibrary::GetActiveInputDeviceSubsystem(GetOwningPlayer()); IsValid(DeviceSubsystem))
	{
		DeviceSubsystem->OnActiveInputDeviceChanged.AddUniqueDynamic(this, &UMKHIntroSkipHintWidget::HandleActiveInputDeviceChanged);
	}
	RefreshPrompt();
}

void UMKHIntroSkipHintWidget::NativeDestruct()
{
	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = UMKHInputSettingsLibrary::GetActiveInputDeviceSubsystem(GetOwningPlayer()); IsValid(DeviceSubsystem))
	{
		DeviceSubsystem->OnActiveInputDeviceChanged.RemoveDynamic(this, &UMKHIntroSkipHintWidget::HandleActiveInputDeviceChanged);
	}

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHIntroSkipHintWidget::SetSkipAction(UInputAction* InSkipAction)
{
	SkipAction = InSkipAction;
	RefreshPrompt();
}

void UMKHIntroSkipHintWidget::SetHoldProgress(const float HoldFraction)
{
	const float Fraction = FMath::Clamp(HoldFraction, 0.f, 1.f);
	if (IsValid(HoldProgressBar))
	{
		HoldProgressBar->SetPercent(Fraction);
		HoldProgressBar->SetVisibility(Fraction > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	BP_OnHoldProgressChanged(Fraction);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHIntroSkipHintWidget::HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice)
{
	RefreshPrompt();
}

void UMKHIntroSkipHintWidget::RefreshPrompt() const
{
	if (!IsValid(Txt_SkipHint))
	{
		return;
	}

	// The designer has no player to ask: preview with the default key.
	FFormatNamedArguments Args;
	Args.Add(TEXT("Key"), IsDesignTime()
		? UMKHInputSettingsLibrary::GetKeyDisplayName(EKeys::SpaceBar)
		: UMKHInputSettingsLibrary::GetActionKeyDisplayNameForActiveDevice(GetOwningPlayer(), SkipAction));
	Txt_SkipHint->SetText(FText::Format(PromptFormat, Args));
}
