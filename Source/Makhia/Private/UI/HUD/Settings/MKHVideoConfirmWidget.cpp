// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHVideoConfirmWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHVideoConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(KeepButton))
	{
		KeepButton->OnClicked.AddDynamic(this, &UMKHVideoConfirmWidget::HandleKeepClicked);
	}
	if (IsValid(RevertButton))
	{
		RevertButton->OnClicked.AddDynamic(this, &UMKHVideoConfirmWidget::HandleRevertClicked);
	}
}

void UMKHVideoConfirmWidget::NativeDestruct()
{
	StopCountdown();

	if (IsValid(KeepButton))
	{
		KeepButton->OnClicked.RemoveDynamic(this, &UMKHVideoConfirmWidget::HandleKeepClicked);
	}
	if (IsValid(RevertButton))
	{
		RevertButton->OnClicked.RemoveDynamic(this, &UMKHVideoConfirmWidget::HandleRevertClicked);
	}

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHVideoConfirmWidget::BeginCountdown()
{
	SecondsLeft = CountdownSeconds;
	RefreshCountdownText();

	if (const UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().SetTimer(CountdownTimer, FTimerDelegate::CreateUObject(
			this, &UMKHVideoConfirmWidget::HandleCountdownTick), 1.0f, true);
	}
}

void UMKHVideoConfirmWidget::ConfirmMode()
{
	StopCountdown();
	OnModeConfirmed.Broadcast();
}

void UMKHVideoConfirmWidget::RevertMode()
{
	StopCountdown();
	OnModeReverted.Broadcast();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHVideoConfirmWidget::HandleKeepClicked()
{
	ConfirmMode();
}

void UMKHVideoConfirmWidget::HandleRevertClicked()
{
	RevertMode();
}

void UMKHVideoConfirmWidget::HandleCountdownTick()
{
	--SecondsLeft;

	if (SecondsLeft <= 0)
	{
		// Silence means the player cannot see the panel: undo.
		RevertMode();
		return;
	}

	RefreshCountdownText();
}

void UMKHVideoConfirmWidget::RefreshCountdownText() const
{
	if (IsValid(CountdownText))
	{
		CountdownText->SetText(FText::Format(CountdownFormat, FText::AsNumber(SecondsLeft)));
	}
}

void UMKHVideoConfirmWidget::StopCountdown()
{
	if (const UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(CountdownTimer);
	}
}
