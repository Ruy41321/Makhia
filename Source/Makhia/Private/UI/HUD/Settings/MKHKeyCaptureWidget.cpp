// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHKeyCaptureWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Libraries/MKHInputSettingsLibrary.h"
#include "MKHLogChannels.h"
#include "TimerManager.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"
#include "UI/Navigation/MKHMenuNavigationSubsystem.h"

namespace MKHKeyCapture
{
	/** Whether a key is a stick direction or an axis; the sticks stay on movement and camera. */
	bool IsStickOrAxisKey(const FKey& Key)
	{
		static const FKey StickDirections[] = {
			EKeys::Gamepad_LeftStick_Up, EKeys::Gamepad_LeftStick_Down, EKeys::Gamepad_LeftStick_Left, EKeys::Gamepad_LeftStick_Right,
			EKeys::Gamepad_RightStick_Up, EKeys::Gamepad_RightStick_Down, EKeys::Gamepad_RightStick_Left, EKeys::Gamepad_RightStick_Right };

		for (const FKey& Direction : StickDirections)
		{
			if (Key == Direction)
			{
				return true;
			}
		}
		return Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D();
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHKeyCaptureWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(ReplaceButton))
	{
		ReplaceButton->OnClicked.AddDynamic(this, &UMKHKeyCaptureWidget::HandleReplaceClicked);
	}
	if (IsValid(CancelButton))
	{
		CancelButton->OnClicked.AddDynamic(this, &UMKHKeyCaptureWidget::HandleCancelClicked);
	}

	SetConflictUIVisible(false);

	// Without keyboard focus the widget never receives NativeOnKeyDown.
	SetIsFocusable(true);
	SetKeyboardFocus();
}

void UMKHKeyCaptureWidget::NativeDestruct()
{
	if (IsValid(ReplaceButton))
	{
		ReplaceButton->OnClicked.RemoveDynamic(this, &UMKHKeyCaptureWidget::HandleReplaceClicked);
	}
	if (IsValid(CancelButton))
	{
		CancelButton->OnClicked.RemoveDynamic(this, &UMKHKeyCaptureWidget::HandleCancelClicked);
	}

	if (const UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(HoldToCancelTimer);
	}

	Super::NativeDestruct();
}

FReply UMKHKeyCaptureWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bIsListening)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	const FKey PressedKey = InKeyEvent.GetKey();

	// The pad's Back cancels a keyboard capture, where no gamepad button can be bound anyway.
	if (PressedKey == EKeys::Escape
		|| (CaptureDevice == EMKHInputDevice::KeyboardMouse && UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent)))
	{
		CancelCapture();
		return FReply::Handled();
	}

	// A controller slot is never left empty (see UMKHKeyboardSettingsWidget::ApplyPendingRebind).
	if (PressedKey == EKeys::Delete && CaptureDevice == EMKHInputDevice::KeyboardMouse)
	{
		bIsListening = false;
		OnClearRequested.Broadcast();
		return FReply::Handled();
	}

	// Keys of the other device are swallowed so they cannot drive the menu behind the modal either.
	if (!IsCapturableKey(PressedKey))
	{
		return FReply::Handled();
	}

	if (CaptureDevice == EMKHInputDevice::Gamepad)
	{
		// The chord button is held as a modifier: the mapping keeps it, only the other button is stored.
		if (PressedKey == ChordKey)
		{
			return FReply::Handled();
		}
		if (!InKeyEvent.IsRepeat())
		{
			BeginGamepadPress(PressedKey);
		}
		return FReply::Handled();
	}

	return ProcessCapturedKey(PressedKey) ? FReply::Handled() : FReply::Unhandled();
}

FReply UMKHKeyCaptureWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bIsListening && ChordKey.IsValid() && InKeyEvent.GetKey() == ChordKey)
	{
		return FReply::Handled();
	}
	if (!bIsListening || !PressedGamepadKey.IsValid() || InKeyEvent.GetKey() != PressedGamepadKey)
	{
		return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
	}

	if (const UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(HoldToCancelTimer);
	}
	const FKey ReleasedKey = PressedGamepadKey;
	PressedGamepadKey = EKeys::Invalid;
	ProcessCapturedKey(ReleasedKey);
	return FReply::Handled();
}

FReply UMKHKeyCaptureWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bIsListening || !IsCapturableKey(InMouseEvent.GetEffectingButton()))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	return ProcessCapturedKey(InMouseEvent.GetEffectingButton()) ? FReply::Handled() : FReply::Unhandled();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHKeyCaptureWidget::BeginCapture(const FText& InActionDisplayName, EPlayerMappableKeySlot InSlot, EMKHInputDevice InDevice,
	const FKey& InChordKey)
{
	PendingKey = EKeys::Invalid;
	PressedGamepadKey = EKeys::Invalid;
	bIsListening = true;
	CaptureDevice = InDevice;
	ChordKey = InDevice == EMKHInputDevice::Gamepad ? InChordKey : EKeys::Invalid;

	if (IsValid(ActionNameText))
	{
		ActionNameText->SetText(InActionDisplayName);
	}

	if (IsValid(SlotNameText))
	{
		SlotNameText->SetText(BuildSlotPrompt(InSlot));
	}

	if (IsValid(ConflictText))
	{
		ConflictText->SetText(FText::GetEmpty());
	}

	SetConflictUIVisible(false);
	BP_OnCaptureStarted();

	SetIsFocusable(true);
	SetKeyboardFocus();
}

void UMKHKeyCaptureWidget::ShowConflict(const FText& InConflictMessage)
{
	bIsListening = false;

	if (IsValid(ConflictText))
	{
		ConflictText->SetText(InConflictMessage);
	}

	SetConflictUIVisible(true);
	BP_OnConflictShown();

	// No longer listening: the gamepad focus moves to Replace (A confirms, Cancel sits next to it).
	if (UMKHMenuNavigationSubsystem* MenuNavigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(MenuNavigation))
	{
		MenuNavigation->RequestFocusInside(ReplaceButton);
	}
}

void UMKHKeyCaptureWidget::ConfirmPendingKey()
{
	if (!PendingKey.IsValid())
	{
		CancelCapture();
		return;
	}

	OnConflictConfirmed.Broadcast(PendingKey);
}

void UMKHKeyCaptureWidget::CancelCapture()
{
	bIsListening = false;
	PendingKey = EKeys::Invalid;
	PressedGamepadKey = EKeys::Invalid;
	if (const UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(HoldToCancelTimer);
	}
	OnCaptureCancelled.Broadcast();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHKeyCaptureWidget::BeginGamepadPress(const FKey& Key)
{
	PressedGamepadKey = Key;

	const UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().SetTimer(HoldToCancelTimer, FTimerDelegate::CreateUObject(this, &UMKHKeyCaptureWidget::HandleHoldToCancel),
			HoldToCancelSeconds, false);
	}
}

void UMKHKeyCaptureWidget::HandleHoldToCancel()
{
	if (bIsListening && PressedGamepadKey.IsValid())
	{
		CancelCapture();
	}
}

FText UMKHKeyCaptureWidget::BuildSlotPrompt(EPlayerMappableKeySlot InSlot) const
{
	if (CaptureDevice != EMKHInputDevice::Gamepad)
	{
		return InSlot == EPlayerMappableKeySlot::First
			? NSLOCTEXT("MKHSettings", "PrimarySlot", "Primary slot")
			: NSLOCTEXT("MKHSettings", "SecondarySlot", "Secondary slot");
	}

	if (!ChordKey.IsValid())
	{
		return NSLOCTEXT("MKHSettings", "ControllerButtonSlotHoldToCancel", "Controller button (hold to cancel)");
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Chord"), UMKHInputSettingsLibrary::GetKeyDisplayName(ChordKey));
	return FText::Format(NSLOCTEXT("MKHSettings", "ControllerChordSlotHoldToCancel", "{Chord} + … (hold to cancel)"), Args);
}

void UMKHKeyCaptureWidget::HandleReplaceClicked()
{
	ConfirmPendingKey();
}

void UMKHKeyCaptureWidget::HandleCancelClicked()
{
	CancelCapture();
}

void UMKHKeyCaptureWidget::SetConflictUIVisible(bool bVisible) const
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (IsValid(ConflictText))
	{
		ConflictText->SetVisibility(NewVisibility);
	}
	if (IsValid(ReplaceButton))
	{
		ReplaceButton->SetVisibility(NewVisibility);
	}
}

bool UMKHKeyCaptureWidget::ProcessCapturedKey(const FKey& CapturedKey)
{
	if (!CapturedKey.IsValid())
	{
		return false;
	}

	// Held from here so a conflict prompt can confirm it later without re-capturing.
	PendingKey = CapturedKey;
	bIsListening = false;

	UE_LOG(LogMKHInput, Verbose, TEXT("KeyCapture: captured %s"), *CapturedKey.ToString());

	OnKeyCaptured.Broadcast(CapturedKey);
	return true;
}

bool UMKHKeyCaptureWidget::IsCapturableKey(const FKey& Key) const
{
	if (!UMKHInputSettingsLibrary::IsKeyForDevice(Key, CaptureDevice))
	{
		return false;
	}
	return CaptureDevice != EMKHInputDevice::Gamepad || !MKHKeyCapture::IsStickOrAxisKey(Key);
}
