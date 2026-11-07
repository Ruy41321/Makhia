// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHGeneralSettingsWidget.h"

#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "Libraries/MKHLocalizationLibrary.h"
#include "MKHLogChannels.h"
#include "UI/HUD/Settings/MKHKeybindCategoryWidget.h"
#include "UI/HUD/Settings/MKHOptionRowWidget.h"
#include "UI/HUD/Settings/MKHSliderRowWidget.h"

#define LOCTEXT_NAMESPACE "MKHSettings"

namespace MKHGeneral
{
	static const TArray<EMKHGeneralOption> LanguageSection  = { EMKHGeneralOption::Language };

	static const TArray<EMKHGeneralOption> GameplaySection = {
		EMKHGeneralOption::HoldToEquipPickup, EMKHGeneralOption::AutoQuickSlotLoadout, EMKHGeneralOption::ScoreboardStyle };

	static const TArray<EMKHGeneralOption> DisplaySection = { EMKHGeneralOption::Brightness };

	static const TArray<EMKHGeneralOption> CameraSection = {
		EMKHGeneralOption::MouseSensitivity, EMKHGeneralOption::InvertYAxis,
		EMKHGeneralOption::CameraShake };

	static const TArray<EMKHGeneralOption> ControllerSection = {
		EMKHGeneralOption::ControllerLookSensitivity, EMKHGeneralOption::ControllerInvertYAxis,
		EMKHGeneralOption::ControllerVibration };

	/** Shipped value of both look sensitivity sliders. */
	constexpr float DefaultSensitivity = 1.0f;

	static const TArray<EMKHGeneralOption> InterfaceSection = {
		EMKHGeneralOption::ShowNetworkStats, EMKHGeneralOption::ShowFPS };
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHGeneralSettingsWidget::NativeOnPageActivated()
{
	Super::NativeOnPageActivated();

	RebuildOptionList();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHGeneralSettingsWidget::ApplyChanges()
{
	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SaveSettings();
	}
}

void UMKHGeneralSettingsWidget::RevertChanges()
{
	RebuildOptionList();
}

void UMKHGeneralSettingsWidget::ResetToDefaults()
{
	UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(UserSettings))
	{
		return;
	}

	UserSettings->SetBrightness(UMKHGameUserSettings::DefaultBrightness);
	UserSettings->SetMouseSensitivity(1.0f);
	UserSettings->SetInvertYAxis(false);
	UserSettings->SetCameraShakeEnabled(true);
	UserSettings->SetHoldToEquipPickupEnabled(true);
	UserSettings->SetAutoQuickSlotLoadoutEnabled(true);
	UserSettings->SetScoreboardStyle(EMKHScoreboardStyle::Pediment);
	UserSettings->SetGamepadLookSensitivity(MKHGeneral::DefaultSensitivity);
	UserSettings->SetInvertGamepadYAxis(false);
	UserSettings->SetControllerVibrationEnabled(true, this);
	UserSettings->SetNetStatsMode(EMKHNetStatsMode::Off);
	UserSettings->SetShowFPS(false, this);
	UserSettings->SaveSettings();
	UMKHLocalizationLibrary::SetLanguage(UMKHLocalizationLibrary::GetDefaultLanguage());

	RebuildOptionList();
}

bool UMKHGeneralSettingsWidget::UsesActionBar() const
{
	return false;
}

void UMKHGeneralSettingsWidget::RebuildOptionList()
{
	if (!IsValid(OptionListContainer))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("GeneralSettings: OptionListContainer is not bound."));
		return;
	}

	for (const TObjectPtr<UMKHOptionRowWidget>& Row : OptionRows)
	{
		if (IsValid(Row))
		{
			Row->OnOptionStepped.RemoveAll(this);
			Row->OnOptionReset.RemoveAll(this);
		}
	}
	for (const TObjectPtr<UMKHSliderRowWidget>& Row : SliderRows)
	{
		if (IsValid(Row))
		{
			Row->OnValueChanged.RemoveAll(this);
			Row->OnValueCommitted.RemoveAll(this);
			Row->OnValueReset.RemoveAll(this);
		}
	}
	OptionRows.Reset();
	SliderRows.Reset();
	OptionListContainer->ClearChildren();

	BuildSection(LOCTEXT("GeneralLanguage", "Language"),   MKHGeneral::LanguageSection);
	BuildSection(LOCTEXT("GeneralGameplay", "Gameplay"),   MKHGeneral::GameplaySection);
	BuildSection(LOCTEXT("GeneralDisplay", "Display"),     MKHGeneral::DisplaySection);
	BuildSection(LOCTEXT("GeneralCamera", "Camera"),       MKHGeneral::CameraSection);
	BuildSection(LOCTEXT("GeneralController", "Controller"), MKHGeneral::ControllerSection);
	BuildSection(LOCTEXT("GeneralInterface", "Interface"), MKHGeneral::InterfaceSection);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHGeneralSettingsWidget::BuildSection(const FText& SectionLabel, const TArray<EMKHGeneralOption>& Options)
{
	if (IsValid(SectionHeaderClass))
	{
		if (UMKHKeybindCategoryWidget* Header = CreateWidget<UMKHKeybindCategoryWidget>(GetOwningPlayer(), SectionHeaderClass);
			IsValid(Header))
		{
			Header->SetCategoryName(SectionLabel, Options.Num());
			Header->SetColumnHeadersVisible(false);
			if (UVerticalBoxSlot* HeaderSlot = Cast<UVerticalBoxSlot>(OptionListContainer->AddChild(Header)); HeaderSlot != nullptr)
			{
				HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
	}

	for (const EMKHGeneralOption Option : Options)
	{
		// Brightness and the two look sensitivities are the only continuous values; everything else is a list.
		if (IsSliderOption(Option))
		{
			BuildSliderRow(Option);
			continue;
		}

		if (!IsValid(OptionRowClass))
		{
			UE_LOG(LogMKHInput, Warning, TEXT("GeneralSettings: OptionRowClass is not set."));
			continue;
		}

		UMKHOptionRowWidget* Row = CreateWidget<UMKHOptionRowWidget>(GetOwningPlayer(), OptionRowClass);
		if (!IsValid(Row))
		{
			continue;
		}

		Row->OnOptionStepped.AddUObject(this, &UMKHGeneralSettingsWidget::HandleOptionStepped);
		Row->OnOptionReset.AddUObject(this, &UMKHGeneralSettingsWidget::HandleOptionReset);

		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(OptionListContainer->AddChild(Row)); RowSlot != nullptr)
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		Row->SetupRow(ToOptionId(Option), GetOptionDisplayName(Option), GetOptionDescription(Option));

		const TArray<FText> Values = GetOptionValues(Option);
		const int32 Index = FMath::Clamp(GetOptionIndex(Option), 0, FMath::Max(0, Values.Num() - 1));
		Row->SetValue(Values.IsValidIndex(Index) ? Values[Index] : FText::GetEmpty(),
			Index, Values.Num(), false, false);

		OptionRows.Add(Row);
	}
}

void UMKHGeneralSettingsWidget::BuildSliderRow(EMKHGeneralOption Option)
{
	if (!IsValid(SliderRowClass))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("GeneralSettings: SliderRowClass is not set."));
		return;
	}

	UMKHSliderRowWidget* Row = CreateWidget<UMKHSliderRowWidget>(GetOwningPlayer(), SliderRowClass);
	if (!IsValid(Row))
	{
		return;
	}

	Row->OnValueChanged.AddUObject(this, &UMKHGeneralSettingsWidget::HandleSliderChanged);
	Row->OnValueCommitted.AddUObject(this, &UMKHGeneralSettingsWidget::HandleSliderCommitted);
	Row->OnValueReset.AddUObject(this, &UMKHGeneralSettingsWidget::HandleSliderReset);

	if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(OptionListContainer->AddChild(Row)); RowSlot != nullptr)
	{
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	float MinValue = 0.0f;
	float MaxValue = 1.0f;
	GetSliderRange(Option, MinValue, MaxValue);
	Row->SetupRow(ToOptionId(Option), GetOptionDisplayName(Option), GetOptionDescription(Option), MinValue, MaxValue);

	const float Value = GetSliderValue(Option);
	Row->SetValue(Value, !FMath::IsNearlyEqual(Value, GetDefaultSliderValue(Option)));

	SliderRows.Add(Row);
}

bool UMKHGeneralSettingsWidget::IsSliderOption(EMKHGeneralOption Option)
{
	return Option == EMKHGeneralOption::Brightness
		|| Option == EMKHGeneralOption::MouseSensitivity
		|| Option == EMKHGeneralOption::ControllerLookSensitivity;
}

float UMKHGeneralSettingsWidget::GetDefaultSliderValue(EMKHGeneralOption Option)
{
	return Option == EMKHGeneralOption::Brightness ? UMKHGameUserSettings::DefaultBrightness : MKHGeneral::DefaultSensitivity;
}

void UMKHGeneralSettingsWidget::GetSliderRange(EMKHGeneralOption Option, float& OutMin, float& OutMax) const
{
	if (Option == EMKHGeneralOption::Brightness)
	{
		OutMin = UMKHGameUserSettings::MinBrightness;
		OutMax = UMKHGameUserSettings::MaxBrightness;
		return;
	}

	// Mouse and right stick share one range: both multiply an already tuned base speed.
	OutMin = MinMouseSensitivity;
	OutMax = MaxMouseSensitivity;
}

float UMKHGeneralSettingsWidget::GetSliderValue(EMKHGeneralOption Option) const
{
	const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(UserSettings))
	{
		return GetDefaultSliderValue(Option);
	}

	switch (Option)
	{
	case EMKHGeneralOption::Brightness:                return UserSettings->GetBrightness();
	case EMKHGeneralOption::ControllerLookSensitivity: return UserSettings->GetGamepadLookSensitivity();
	default:                                           return UserSettings->GetMouseSensitivity();
	}
}

void UMKHGeneralSettingsWidget::SetSliderValue(EMKHGeneralOption Option, float Value) const
{
	UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(UserSettings))
	{
		return;
	}

	switch (Option)
	{
	case EMKHGeneralOption::Brightness:                UserSettings->SetBrightness(Value); break;
	case EMKHGeneralOption::ControllerLookSensitivity: UserSettings->SetGamepadLookSensitivity(Value); break;
	default:                                           UserSettings->SetMouseSensitivity(Value); break;
	}
}

void UMKHGeneralSettingsWidget::HandleOptionStepped(FName OptionId, int32 Delta)
{
	EMKHGeneralOption Option;
	if (!FromOptionId(OptionId, Option))
	{
		return;
	}

	const int32 NumValues = GetOptionValues(Option).Num();
	SetOptionIndex(Option, FMath::Clamp(GetOptionIndex(Option) + Delta, 0, NumValues - 1));

	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SaveSettings();
	}

	RebuildOptionList();
}

void UMKHGeneralSettingsWidget::HandleOptionReset(FName OptionId)
{
	EMKHGeneralOption Option;
	if (!FromOptionId(OptionId, Option))
	{
		return;
	}

	SetOptionIndex(Option, GetDefaultOptionIndex(Option));

	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SaveSettings();
	}

	RebuildOptionList();
}

void UMKHGeneralSettingsWidget::HandleSliderChanged(FName OptionId, float Value)
{
	// Live so the player can see or feel the new value while dragging; saved on release.
	EMKHGeneralOption Option;
	if (FromOptionId(OptionId, Option))
	{
		SetSliderValue(Option, Value);
	}
}

void UMKHGeneralSettingsWidget::HandleSliderCommitted(FName OptionId)
{
	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SaveSettings();
	}

	RebuildOptionList();
}

void UMKHGeneralSettingsWidget::HandleSliderReset(FName OptionId)
{
	EMKHGeneralOption Option;
	if (FromOptionId(OptionId, Option))
	{
		SetSliderValue(Option, GetDefaultSliderValue(Option));
	}

	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->SaveSettings();
	}

	RebuildOptionList();
}

// ============================================================
// Option metadata
// ============================================================

int32 UMKHGeneralSettingsWidget::GetOptionIndex(EMKHGeneralOption Option) const
{
	if (Option == EMKHGeneralOption::Language)
	{
		return FMath::Max(0, UMKHLocalizationLibrary::GetAvailableLanguages().IndexOfByKey(UMKHLocalizationLibrary::GetCurrentLanguage()));
	}

	const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(UserSettings))
	{
		return 0;
	}

	switch (Option)
	{
	case EMKHGeneralOption::InvertYAxis:      return UserSettings->IsYAxisInverted() ? 1 : 0;
	case EMKHGeneralOption::CameraShake:      return UserSettings->IsCameraShakeEnabled() ? 1 : 0;
	case EMKHGeneralOption::ControllerInvertYAxis: return UserSettings->IsGamepadYAxisInverted() ? 1 : 0;
	case EMKHGeneralOption::ControllerVibration:   return UserSettings->IsControllerVibrationEnabled() ? 1 : 0;
	case EMKHGeneralOption::ShowNetworkStats: return static_cast<int32>(UserSettings->GetNetStatsMode());
	case EMKHGeneralOption::ShowFPS:          return UserSettings->IsShowingFPS() ? 1 : 0;
	case EMKHGeneralOption::HoldToEquipPickup:    return UserSettings->IsHoldToEquipPickupEnabled() ? 1 : 0;
	case EMKHGeneralOption::AutoQuickSlotLoadout: return UserSettings->IsAutoQuickSlotLoadoutEnabled() ? 1 : 0;
	case EMKHGeneralOption::ScoreboardStyle:      return static_cast<int32>(UserSettings->GetScoreboardStyle());
	default:                                  return 0;
	}
}

int32 UMKHGeneralSettingsWidget::GetDefaultOptionIndex(EMKHGeneralOption Option) const
{
	switch (Option)
	{
	case EMKHGeneralOption::Language:
		return FMath::Max(0, UMKHLocalizationLibrary::GetAvailableLanguages().IndexOfByKey(UMKHLocalizationLibrary::GetDefaultLanguage()));
	case EMKHGeneralOption::CameraShake:
	case EMKHGeneralOption::ControllerVibration:
	case EMKHGeneralOption::HoldToEquipPickup:
	case EMKHGeneralOption::AutoQuickSlotLoadout:
		return 1;
	default:
		return 0;
	}
}

void UMKHGeneralSettingsWidget::SetOptionIndex(EMKHGeneralOption Option, int32 NewIndex)
{
	if (Option == EMKHGeneralOption::Language)
	{
		const TArray<FString> Languages = UMKHLocalizationLibrary::GetAvailableLanguages();
		if (Languages.IsValidIndex(NewIndex))
		{
			UMKHLocalizationLibrary::SetLanguage(Languages[NewIndex]);
		}
		return;
	}

	UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(UserSettings))
	{
		return;
	}

	switch (Option)
	{
	case EMKHGeneralOption::InvertYAxis:      UserSettings->SetInvertYAxis(NewIndex != 0); break;
	case EMKHGeneralOption::CameraShake:      UserSettings->SetCameraShakeEnabled(NewIndex != 0); break;
	case EMKHGeneralOption::ControllerInvertYAxis: UserSettings->SetInvertGamepadYAxis(NewIndex != 0); break;
	case EMKHGeneralOption::ControllerVibration:   UserSettings->SetControllerVibrationEnabled(NewIndex != 0, this); break;
	case EMKHGeneralOption::ShowNetworkStats:
		UserSettings->SetNetStatsMode(static_cast<EMKHNetStatsMode>(FMath::Clamp(NewIndex, 0, static_cast<int32>(EMKHNetStatsMode::Detailed))));
		break;
	case EMKHGeneralOption::ShowFPS:          UserSettings->SetShowFPS(NewIndex != 0, this); break;
	case EMKHGeneralOption::HoldToEquipPickup:    UserSettings->SetHoldToEquipPickupEnabled(NewIndex != 0); break;
	case EMKHGeneralOption::AutoQuickSlotLoadout: UserSettings->SetAutoQuickSlotLoadoutEnabled(NewIndex != 0); break;
	case EMKHGeneralOption::ScoreboardStyle:
		UserSettings->SetScoreboardStyle(NewIndex != 0 ? EMKHScoreboardStyle::Epigraph : EMKHScoreboardStyle::Pediment);
		break;
	default: break;
	}
}

TArray<FText> UMKHGeneralSettingsWidget::GetOptionValues(EMKHGeneralOption Option) const
{
	static const TArray<FText> OffOn = { LOCTEXT("Off", "Off"), LOCTEXT("On", "On") };

	// Indices match EMKHNetStatsMode.
	static const TArray<FText> NetStatsModes = {
		LOCTEXT("Off", "Off"), LOCTEXT("NetStatsCompact", "Compact"), LOCTEXT("NetStatsDetailed", "Detailed") };

	// Indices match EMKHScoreboardStyle.
	static const TArray<FText> ScoreboardStyles = {
		LOCTEXT("ScoreboardPediment", "Pediment"), LOCTEXT("ScoreboardEpigraph", "Epigraph") };

	if (Option == EMKHGeneralOption::ShowNetworkStats)
	{
		return NetStatsModes;
	}

	if (Option == EMKHGeneralOption::ScoreboardStyle)
	{
		return ScoreboardStyles;
	}

	if (Option == EMKHGeneralOption::Language)
	{
		TArray<FText> LanguageNames;
		for (const FString& Language : UMKHLocalizationLibrary::GetAvailableLanguages())
		{
			LanguageNames.Add(UMKHLocalizationLibrary::GetLanguageDisplayName(Language));
		}
		return LanguageNames;
	}

	return OffOn;
}

FText UMKHGeneralSettingsWidget::GetOptionDisplayName(EMKHGeneralOption Option) const
{
	switch (Option)
	{
	case EMKHGeneralOption::Language:         return LOCTEXT("OptLanguage", "Language");
	case EMKHGeneralOption::Brightness:       return LOCTEXT("OptBrightness", "Brightness");
	case EMKHGeneralOption::MouseSensitivity: return LOCTEXT("OptSensitivity", "Mouse Sensitivity");
	case EMKHGeneralOption::InvertYAxis:      return LOCTEXT("OptInvertY", "Invert Y Axis");
	case EMKHGeneralOption::CameraShake:      return LOCTEXT("OptShake", "Camera Shake");
	case EMKHGeneralOption::ControllerLookSensitivity: return LOCTEXT("OptControllerSensitivity", "Look Sensitivity");
	case EMKHGeneralOption::ControllerInvertYAxis:     return LOCTEXT("OptInvertY", "Invert Y Axis");
	case EMKHGeneralOption::ControllerVibration:       return LOCTEXT("OptVibration", "Vibration");
	case EMKHGeneralOption::ShowNetworkStats: return LOCTEXT("OptNetStatsMode", "Network Stats");
	case EMKHGeneralOption::ShowFPS:          return LOCTEXT("OptFPS", "Show FPS");
	case EMKHGeneralOption::HoldToEquipPickup:    return LOCTEXT("OptHoldToEquip", "Hold to Equip Pickups");
	case EMKHGeneralOption::AutoQuickSlotLoadout: return LOCTEXT("OptAutoQuickSlots", "Remember Quick Slots");
	case EMKHGeneralOption::ScoreboardStyle:      return LOCTEXT("OptScoreboardStyle", "Scoreboard Style");
	default:                                  return FText::GetEmpty();
	}
}

FText UMKHGeneralSettingsWidget::GetOptionDescription(EMKHGeneralOption Option) const
{
	switch (Option)
	{
	case EMKHGeneralOption::Language:         return LOCTEXT("DescLanguage", "Interface language");
	case EMKHGeneralOption::Brightness:       return LOCTEXT("DescBrightness", "Lightens or darkens the game world; the interface is unchanged");
	case EMKHGeneralOption::MouseSensitivity: return LOCTEXT("DescSensitivity", "Look speed multiplier");
	case EMKHGeneralOption::InvertYAxis:      return LOCTEXT("DescInvertY", "Flips vertical mouse look");
	case EMKHGeneralOption::CameraShake:      return LOCTEXT("DescShake", "Screen shake on hits and impacts");
	case EMKHGeneralOption::ControllerLookSensitivity: return LOCTEXT("DescControllerSensitivity", "Right stick look speed multiplier");
	case EMKHGeneralOption::ControllerInvertYAxis:     return LOCTEXT("DescControllerInvertY", "Flips vertical look on the right stick");
	case EMKHGeneralOption::ControllerVibration:       return LOCTEXT("DescVibration", "Lets the controller vibrate");
	case EMKHGeneralOption::ShowNetworkStats: return LOCTEXT("DescNetStatsMode", "Ping, connection route and packet loss during online matches");
	case EMKHGeneralOption::ShowFPS:          return LOCTEXT("DescFPS", "Frame rate counter");
	case EMKHGeneralOption::HoldToEquipPickup:
		return LOCTEXT("DescHoldToEquip", "Hold the pick up button to also equip armor or put weapons and consumables in a quick slot");
	case EMKHGeneralOption::AutoQuickSlotLoadout:
		return LOCTEXT("DescAutoQuickSlots", "At the start of a match, quick slots the items you used in the last match and leaves off the armor you took off");
	case EMKHGeneralOption::ScoreboardStyle:
		return LOCTEXT("DescScoreboardStyle", "Look of the score panel at the top of the screen during a match");
	default:                                  return FText::GetEmpty();
	}
}

FName UMKHGeneralSettingsWidget::ToOptionId(EMKHGeneralOption Option)
{
	return FName(*FString::Printf(TEXT("GeneralOption_%d"), static_cast<int32>(Option)));
}

bool UMKHGeneralSettingsWidget::FromOptionId(FName OptionId, EMKHGeneralOption& OutOption)
{
	FString Left;
	FString Right;
	if (!OptionId.ToString().Split(TEXT("_"), &Left, &Right))
	{
		return false;
	}

	OutOption = static_cast<EMKHGeneralOption>(FCString::Atoi(*Right));
	return true;
}

#undef LOCTEXT_NAMESPACE
