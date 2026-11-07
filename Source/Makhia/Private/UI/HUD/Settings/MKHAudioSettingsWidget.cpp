// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHAudioSettingsWidget.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "MKHLogChannels.h"

bool UMKHAudioSettingsWidget::UsesActionBar() const
{
	return false;
}

void UMKHAudioSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeSliderValues();
	RefreshAllReadouts();
	BindSliderEvents();
	BindResetButtonEvents();
}

void UMKHAudioSettingsWidget::NativeDestruct()
{
	// Safety net: persist any change made without releasing the slider (e.g. menu closed mid-drag).
	HandleVolumeCaptureEnd();
	UnbindSliderEvents();
	UnbindResetButtonEvents();

	Super::NativeDestruct();
}

void UMKHAudioSettingsWidget::HandleMasterVolumeChanged(float NewValue)
{
	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SetMasterVolume(NewValue);
	}

	RefreshChannelReadout(NewValue, DefaultChannelVolume, MasterValueText, MasterResetButton);
	RefreshTrackFill(MasterTrackRow, NewValue);
}

void UMKHAudioSettingsWidget::HandleMusicVolumeChanged(float NewValue)
{
	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SetMusicVolume(NewValue);
	}

	RefreshChannelReadout(NewValue, DefaultChannelVolume, MusicValueText, MusicResetButton);
	RefreshTrackFill(MusicTrackRow, NewValue);
}

void UMKHAudioSettingsWidget::HandleSFXVolumeChanged(float NewValue)
{
	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SetSFXVolume(NewValue);
	}

	RefreshChannelReadout(NewValue, DefaultChannelVolume, SFXValueText, SFXResetButton);
	RefreshTrackFill(SFXTrackRow, NewValue);
}

void UMKHAudioSettingsWidget::HandleCrowdVolumeChanged(float NewValue)
{
	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SetCrowdVolume(NewValue);
	}

	RefreshChannelReadout(NewValue, DefaultChannelVolume, CrowdValueText, CrowdResetButton);
	RefreshTrackFill(CrowdTrackRow, NewValue);
}

void UMKHAudioSettingsWidget::HandleVolumeCaptureEnd()
{
	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SaveSettings();
	}
}

void UMKHAudioSettingsWidget::HandleMasterResetClicked()
{
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->SetValue(DefaultChannelVolume);
	}

	HandleMasterVolumeChanged(DefaultChannelVolume);
	HandleVolumeCaptureEnd();
}

void UMKHAudioSettingsWidget::HandleMusicResetClicked()
{
	if (IsValid(MusicVolumeSlider))
	{
		MusicVolumeSlider->SetValue(DefaultChannelVolume);
	}

	HandleMusicVolumeChanged(DefaultChannelVolume);
	HandleVolumeCaptureEnd();
}

void UMKHAudioSettingsWidget::HandleSFXResetClicked()
{
	if (IsValid(SFXVolumeSlider))
	{
		SFXVolumeSlider->SetValue(DefaultChannelVolume);
	}

	HandleSFXVolumeChanged(DefaultChannelVolume);
	HandleVolumeCaptureEnd();
}

void UMKHAudioSettingsWidget::HandleCrowdResetClicked()
{
	if (IsValid(CrowdVolumeSlider))
	{
		CrowdVolumeSlider->SetValue(DefaultChannelVolume);
	}

	HandleCrowdVolumeChanged(DefaultChannelVolume);
	HandleVolumeCaptureEnd();
}

void UMKHAudioSettingsWidget::RefreshChannelReadout(float Volume, float DefaultVolume,
	UTextBlock* ValueText, UButton* ResetButton) const
{
	if (IsValid(ValueText))
	{
		// AsPercent follows the culture (French writes "75 %").
		static const FNumberFormattingOptions WholePercent = FNumberFormattingOptions().SetMaximumFractionalDigits(0);
		ValueText->SetText(FText::AsPercent(Volume, &WholePercent));

		// A silent channel reads as dimmed, so "muted" is visible without an extra icon.
		ValueText->SetColorAndOpacity(FMath::IsNearlyZero(Volume) ? SilentValueColor : AudibleValueColor);
	}

	if (IsValid(ResetButton))
	{
		ResetButton->SetVisibility(FMath::IsNearlyEqual(Volume, DefaultVolume)
			? ESlateVisibility::Hidden
			: ESlateVisibility::Visible);
	}
}

void UMKHAudioSettingsWidget::RefreshTrackFill(UHorizontalBox* TrackRow, float Volume) const
{
	if (!IsValid(TrackRow) || TrackRow->GetChildrenCount() < 2)
	{
		return;
	}

	// A zero-weight slot collapses to nothing, so both halves keep a sliver of size.
	const float Filled = FMath::Clamp(Volume, 0.0001f, 0.9999f);

	FSlateChildSize FillSize(ESlateSizeRule::Fill);
	FillSize.Value = Filled;

	FSlateChildSize RestSize(ESlateSizeRule::Fill);
	RestSize.Value = 1.0f - Filled;

	if (UHorizontalBoxSlot* FillSlot = Cast<UHorizontalBoxSlot>(TrackRow->GetChildAt(0)->Slot); FillSlot != nullptr)
	{
		FillSlot->SetSize(FillSize);
	}
	if (UHorizontalBoxSlot* RestSlot = Cast<UHorizontalBoxSlot>(TrackRow->GetChildAt(1)->Slot); RestSlot != nullptr)
	{
		RestSlot->SetSize(RestSize);
	}
}

void UMKHAudioSettingsWidget::RefreshAllReadouts() const
{
	const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(UserSettings))
	{
		return;
	}

	RefreshChannelReadout(UserSettings->GetMasterVolume(), DefaultChannelVolume, MasterValueText, MasterResetButton);
	RefreshChannelReadout(UserSettings->GetMusicVolume(),  DefaultChannelVolume, MusicValueText,  MusicResetButton);
	RefreshChannelReadout(UserSettings->GetSFXVolume(),    DefaultChannelVolume, SFXValueText,    SFXResetButton);
	RefreshChannelReadout(UserSettings->GetCrowdVolume(),  DefaultChannelVolume, CrowdValueText,  CrowdResetButton);

	RefreshTrackFill(MasterTrackRow, UserSettings->GetMasterVolume());
	RefreshTrackFill(MusicTrackRow,  UserSettings->GetMusicVolume());
	RefreshTrackFill(SFXTrackRow,    UserSettings->GetSFXVolume());
	RefreshTrackFill(CrowdTrackRow,  UserSettings->GetCrowdVolume());
}

void UMKHAudioSettingsWidget::InitializeSliderValues() const
{
	const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(UserSettings))
	{
		UE_LOG(LogMKHAudio, Warning, TEXT("UMKHAudioSettingsWidget::InitializeSliderValues: UMKHGameUserSettings unavailable."));
		return;
	}

	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->SetValue(UserSettings->GetMasterVolume());
	}
	if (IsValid(MusicVolumeSlider))
	{
		MusicVolumeSlider->SetValue(UserSettings->GetMusicVolume());
	}
	if (IsValid(SFXVolumeSlider))
	{
		SFXVolumeSlider->SetValue(UserSettings->GetSFXVolume());
	}
	if (IsValid(CrowdVolumeSlider))
	{
		CrowdVolumeSlider->SetValue(UserSettings->GetCrowdVolume());
	}
}

void UMKHAudioSettingsWidget::BindSliderEvents()
{
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleMasterVolumeChanged);
		MasterVolumeSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
		MasterVolumeSlider->OnControllerCaptureEnd.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
	}
	if (IsValid(MusicVolumeSlider))
	{
		MusicVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleMusicVolumeChanged);
		MusicVolumeSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
		MusicVolumeSlider->OnControllerCaptureEnd.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
	}
	if (IsValid(SFXVolumeSlider))
	{
		SFXVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleSFXVolumeChanged);
		SFXVolumeSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
		SFXVolumeSlider->OnControllerCaptureEnd.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
	}
	if (IsValid(CrowdVolumeSlider))
	{
		CrowdVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleCrowdVolumeChanged);
		CrowdVolumeSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
		CrowdVolumeSlider->OnControllerCaptureEnd.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
	}
}

void UMKHAudioSettingsWidget::BindResetButtonEvents()
{
	if (IsValid(MasterResetButton))
	{
		MasterResetButton->OnClicked.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleMasterResetClicked);
	}
	if (IsValid(MusicResetButton))
	{
		MusicResetButton->OnClicked.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleMusicResetClicked);
	}
	if (IsValid(SFXResetButton))
	{
		SFXResetButton->OnClicked.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleSFXResetClicked);
	}
	if (IsValid(CrowdResetButton))
	{
		CrowdResetButton->OnClicked.AddUniqueDynamic(this, &UMKHAudioSettingsWidget::HandleCrowdResetClicked);
	}
}

void UMKHAudioSettingsWidget::UnbindSliderEvents()
{
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleMasterVolumeChanged);
		MasterVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
		MasterVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
	}
	if (IsValid(MusicVolumeSlider))
	{
		MusicVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleMusicVolumeChanged);
		MusicVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
		MusicVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
	}
	if (IsValid(SFXVolumeSlider))
	{
		SFXVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleSFXVolumeChanged);
		SFXVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
		SFXVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
	}
	if (IsValid(CrowdVolumeSlider))
	{
		CrowdVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleCrowdVolumeChanged);
		CrowdVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
		CrowdVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleVolumeCaptureEnd);
	}
}

void UMKHAudioSettingsWidget::UnbindResetButtonEvents()
{
	if (IsValid(MasterResetButton))
	{
		MasterResetButton->OnClicked.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleMasterResetClicked);
	}
	if (IsValid(MusicResetButton))
	{
		MusicResetButton->OnClicked.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleMusicResetClicked);
	}
	if (IsValid(SFXResetButton))
	{
		SFXResetButton->OnClicked.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleSFXResetClicked);
	}
	if (IsValid(CrowdResetButton))
	{
		CrowdResetButton->OnClicked.RemoveDynamic(this, &UMKHAudioSettingsWidget::HandleCrowdResetClicked);
	}
}
