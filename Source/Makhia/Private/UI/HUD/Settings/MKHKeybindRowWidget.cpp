// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHKeybindRowWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Libraries/MKHInputSettingsLibrary.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHKeybindRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(PrimaryKeyButton))
	{
		PrimaryKeyButton->OnClicked.AddDynamic(this, &UMKHKeybindRowWidget::HandlePrimaryClicked);
	}
	if (IsValid(SecondaryKeyButton))
	{
		SecondaryKeyButton->OnClicked.AddDynamic(this, &UMKHKeybindRowWidget::HandleSecondaryClicked);
	}
	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.AddDynamic(this, &UMKHKeybindRowWidget::HandleResetClicked);
	}
}

void UMKHKeybindRowWidget::NativeDestruct()
{
	if (IsValid(PrimaryKeyButton))
	{
		PrimaryKeyButton->OnClicked.RemoveDynamic(this, &UMKHKeybindRowWidget::HandlePrimaryClicked);
	}
	if (IsValid(SecondaryKeyButton))
	{
		SecondaryKeyButton->OnClicked.RemoveDynamic(this, &UMKHKeybindRowWidget::HandleSecondaryClicked);
	}
	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.RemoveDynamic(this, &UMKHKeybindRowWidget::HandleResetClicked);
	}

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHKeybindRowWidget::SetupRow(FName InMappingName, const FText& InDisplayName, EMKHInputDevice InDevice)
{
	MappingName = InMappingName;
	Device = InDevice;

	// One button per action on a controller: the Secondary column only exists for keyboard and mouse.
	if (IsValid(SecondaryKeyButton))
	{
		SecondaryKeyButton->SetVisibility(Device == EMKHInputDevice::Gamepad ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
	}

	if (IsValid(ActionNameText))
	{
		ActionNameText->SetText(InDisplayName);
	}

	RefreshFromProfile();
}

void UMKHKeybindRowWidget::RefreshFromProfile()
{
	RefreshSlot(EPlayerMappableKeySlot::First, PrimaryKeyText);
	RefreshSlot(EPlayerMappableKeySlot::Second, SecondaryKeyText);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHKeybindRowWidget::RefreshSlot(EPlayerMappableKeySlot KeySlot, UTextBlock* SlotText)
{
	const UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	if (!IsValid(UserSettings))
	{
		return;
	}

	const FPlayerKeyMapping* Mapping = UMKHInputSettingsLibrary::FindMappingForSlot(UserSettings, MappingName, KeySlot, Device);
	const bool bHasMapping = Mapping != nullptr && Mapping->GetCurrentKey().IsValid();

	const bool bIsCustomized = bHasMapping && Mapping->IsCustomized();

	if (IsValid(SlotText))
	{
		SlotText->SetText(bHasMapping
			? UMKHInputSettingsLibrary::GetMappingKeyDisplayName(GetOwningPlayer(), *Mapping, Device)
			: UnboundSlotText);

		// Brass marks a rebound key, so the list shows at a glance what the player changed.
		SlotText->SetColorAndOpacity(!bHasMapping ? UnboundKeyColor
			: (bIsCustomized ? CustomizedKeyColor : DefaultKeyColor));
	}

	BP_OnSlotStateChanged(KeySlot, bIsCustomized);
}

void UMKHKeybindRowWidget::HandlePrimaryClicked()
{
	OnSlotClicked.Broadcast(MappingName, EPlayerMappableKeySlot::First);
}

void UMKHKeybindRowWidget::HandleSecondaryClicked()
{
	OnSlotClicked.Broadcast(MappingName, EPlayerMappableKeySlot::Second);
}

void UMKHKeybindRowWidget::HandleResetClicked()
{
	OnResetClicked.Broadcast(MappingName);
}
