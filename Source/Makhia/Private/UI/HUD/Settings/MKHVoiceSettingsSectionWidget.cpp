// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHVoiceSettingsSectionWidget.h"

#include "Audio/MKHAudioDeviceSubsystem.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "MKHLogChannels.h"
#include "Online/Backend/MKHOnlineServicesSubsystem.h"
#include "UI/HUD/Settings/MKHKeybindCategoryWidget.h"
#include "UI/HUD/Settings/MKHOptionRowWidget.h"
#include "UI/HUD/Settings/MKHSliderRowWidget.h"
#include "UI/HUD/Settings/MKHVoiceMicMeterWidget.h"
#include "Voice/MKHVoiceChatSubsystem.h"

#define LOCTEXT_NAMESPACE "MKHVoiceSettings"

namespace MKHVoiceSettings
{
	/** Rows of the device section. */
	static const TArray<EMKHVoiceSettingOption> DeviceSection = { EMKHVoiceSettingOption::GameOutputDevice };

	/** Rows of the voice chat section. */
	static const TArray<EMKHVoiceSettingOption> VoiceSection = {
		EMKHVoiceSettingOption::VoiceChat, EMKHVoiceSettingOption::MicMode, EMKHVoiceSettingOption::NonFriends,
		EMKHVoiceSettingOption::InputVolume, EMKHVoiceSettingOption::MicLevel, EMKHVoiceSettingOption::ActivationThreshold,
		EMKHVoiceSettingOption::OutputVolume, EMKHVoiceSettingOption::VoiceOutputDevice, EMKHVoiceSettingOption::InputDevice };

	/** Slider rows show percentages: settings in [0, 1] are shown in [0, 100]. */
	static constexpr float SliderScale = 100.f;

	/** Index of "On" in the on/off steppers. */
	static constexpr int32 OnIndex = 1;
}

// ============================================================
// Public Interface
// ============================================================

void UMKHVoiceSettingsSectionWidget::RebuildOptionList()
{
	if (!IsValid(OptionListContainer))
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("VoiceSettings: OptionListContainer is not bound."));
		return;
	}
	UnbindRows();
	OptionRows.Reset();
	SliderRows.Reset();
	MicMeter = nullptr;
	OptionListContainer->ClearChildren();

	BuildSection(LOCTEXT("DeviceSection", "Output Device"), MKHVoiceSettings::DeviceSection);
	BuildSection(LOCTEXT("VoiceSection", "Voice Chat"), MKHVoiceSettings::VoiceSection);
	RefreshRowValues();
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHVoiceSettingsSectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RebuildOptionList();
	if (UMKHAudioDeviceSubsystem* Devices = UMKHAudioDeviceSubsystem::Get(this); IsValid(Devices))
	{
		Devices->OnAudioDevicesChanged.AddDynamic(this, &UMKHVoiceSettingsSectionWidget::HandleExternalChange);
		Devices->RefreshDevices();
	}
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->OnVoiceStateChanged.AddDynamic(this, &UMKHVoiceSettingsSectionWidget::HandleExternalChange);
	}
}

void UMKHVoiceSettingsSectionWidget::NativeDestruct()
{
	UnbindRows();
	if (UMKHAudioDeviceSubsystem* Devices = UMKHAudioDeviceSubsystem::Get(this); IsValid(Devices))
	{
		Devices->OnAudioDevicesChanged.RemoveAll(this);
	}
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->OnVoiceStateChanged.RemoveAll(this);
	}
	if (UMKHGameUserSettings* Settings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(Settings))
	{
		Settings->SaveSettings();
	}
	Super::NativeDestruct();
}

// ============================================================
// Protected / Internal Logic - Building
// ============================================================

void UMKHVoiceSettingsSectionWidget::BuildSection(const FText& SectionLabel, const TArray<EMKHVoiceSettingOption>& Options)
{
	const TArray<EMKHVoiceSettingOption> Shown = Options.FilterByPredicate([this](EMKHVoiceSettingOption Option)
	{
		return IsOptionAvailable(Option);
	});
	if (Shown.IsEmpty())
	{
		return;
	}
	if (IsValid(SectionHeaderClass))
	{
		if (UMKHKeybindCategoryWidget* Header = CreateWidget<UMKHKeybindCategoryWidget>(GetOwningPlayer(), SectionHeaderClass); IsValid(Header))
		{
			Header->SetCategoryName(SectionLabel, Shown.Num());
			Header->SetColumnHeadersVisible(false);
			AddRowToList(Header);
		}
	}
	for (const EMKHVoiceSettingOption Option : Shown)
	{
		if (Option == EMKHVoiceSettingOption::MicLevel)
		{
			BuildMeterRow();
		}
		else if (IsSliderOption(Option))
		{
			BuildSliderRow(Option);
		}
		else
		{
			BuildOptionRow(Option);
		}
	}
}

void UMKHVoiceSettingsSectionWidget::BuildOptionRow(EMKHVoiceSettingOption Option)
{
	UMKHOptionRowWidget* Row = IsValid(OptionRowClass) ? CreateWidget<UMKHOptionRowWidget>(GetOwningPlayer(), OptionRowClass) : nullptr;
	if (!IsValid(Row))
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("VoiceSettings: OptionRowClass is not set."));
		return;
	}
	Row->OnOptionStepped.AddUObject(this, &UMKHVoiceSettingsSectionWidget::HandleOptionStepped);
	Row->OnOptionReset.AddUObject(this, &UMKHVoiceSettingsSectionWidget::HandleOptionReset);
	AddRowToList(Row);
	Row->SetupRow(ToOptionId(Option), GetOptionDisplayName(Option), GetOptionDescription(Option));
	OptionRows.Add(Row);
}

void UMKHVoiceSettingsSectionWidget::BuildSliderRow(EMKHVoiceSettingOption Option)
{
	UMKHSliderRowWidget* Row = IsValid(SliderRowClass) ? CreateWidget<UMKHSliderRowWidget>(GetOwningPlayer(), SliderRowClass) : nullptr;
	if (!IsValid(Row))
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("VoiceSettings: SliderRowClass is not set."));
		return;
	}
	Row->OnValueChanged.AddUObject(this, &UMKHVoiceSettingsSectionWidget::HandleSliderChanged);
	Row->OnValueCommitted.AddUObject(this, &UMKHVoiceSettingsSectionWidget::HandleSliderCommitted);
	Row->OnValueReset.AddUObject(this, &UMKHVoiceSettingsSectionWidget::HandleSliderReset);
	AddRowToList(Row);
	Row->SetupRow(ToOptionId(Option), GetOptionDisplayName(Option), GetOptionDescription(Option), 0.f, MKHVoiceSettings::SliderScale);
	Row->SetValueFormat(LOCTEXT("PercentFormat", "{0}%"), 0);
	SliderRows.Add(Row);
}

void UMKHVoiceSettingsSectionWidget::BuildMeterRow()
{
	MicMeter = IsValid(MicMeterClass) ? CreateWidget<UMKHVoiceMicMeterWidget>(GetOwningPlayer(), MicMeterClass) : nullptr;
	if (!IsValid(MicMeter))
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("VoiceSettings: MicMeterClass is not set."));
		return;
	}
	AddRowToList(MicMeter);
	MicMeter->SetupRow(GetOptionDisplayName(EMKHVoiceSettingOption::MicLevel), GetOptionDescription(EMKHVoiceSettingOption::MicLevel));
}

void UMKHVoiceSettingsSectionWidget::AddRowToList(UWidget* Row) const
{
	if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(OptionListContainer->AddChild(Row)); RowSlot != nullptr)
	{
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

bool UMKHVoiceSettingsSectionWidget::IsOptionAvailable(EMKHVoiceSettingOption Option) const
{
	const UMKHOnlineServicesSubsystem* Services = UMKHOnlineServicesSubsystem::Get(this);
	const FMKHOnlineCapabilities Capabilities = IsValid(Services) ? Services->GetCapabilities() : FMKHOnlineCapabilities();
	switch (Option)
	{
	case EMKHVoiceSettingOption::GameOutputDevice:
		return true;
	case EMKHVoiceSettingOption::VoiceOutputDevice:
		return Capabilities.bSupportsVoiceChat && Capabilities.bSupportsVoiceOutputDevice;
	default:
		return Capabilities.bSupportsVoiceChat;
	}
}

bool UMKHVoiceSettingsSectionWidget::IsSliderOption(EMKHVoiceSettingOption Option)
{
	return Option == EMKHVoiceSettingOption::InputVolume || Option == EMKHVoiceSettingOption::ActivationThreshold
		|| Option == EMKHVoiceSettingOption::OutputVolume;
}

// ============================================================
// Protected / Internal Logic - Values
// ============================================================

void UMKHVoiceSettingsSectionWidget::RefreshRowValues() const
{
	for (const TObjectPtr<UMKHOptionRowWidget>& Row : OptionRows)
	{
		EMKHVoiceSettingOption Option;
		if (IsValid(Row) && FromOptionId(Row->GetOptionId(), Option))
		{
			RefreshOptionRow(*Row, Option);
		}
	}
	for (const TObjectPtr<UMKHSliderRowWidget>& Row : SliderRows)
	{
		EMKHVoiceSettingOption Option;
		if (IsValid(Row) && FromOptionId(Row->GetOptionId(), Option))
		{
			const float Value = GetSliderValue(Option);
			Row->SetValue(Value, !FMath::IsNearlyEqual(Value, GetDefaultSliderValue(Option), 0.5f));
		}
	}
	if (IsValid(MicMeter))
	{
		const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
		const bool bOpenMic = IsValid(VoiceChat) && VoiceChat->GetMicMode() == EMKHVoiceMicMode::OpenMic;
		MicMeter->SetThreshold(GetSliderValue(EMKHVoiceSettingOption::ActivationThreshold) / MKHVoiceSettings::SliderScale, bOpenMic);
	}
}

void UMKHVoiceSettingsSectionWidget::RefreshOptionRow(UMKHOptionRowWidget& Row, EMKHVoiceSettingOption Option) const
{
	const TArray<FText> Values = GetOptionValues(Option);
	const int32 Index = FMath::Clamp(GetOptionIndex(Option), 0, FMath::Max(0, Values.Num() - 1));
	const bool bReadOnly = Option == EMKHVoiceSettingOption::InputDevice;
	Row.SetValue(Values.IsValidIndex(Index) ? Values[Index] : FText::GetEmpty(), Index, Values.Num(),
		!bReadOnly && Index != GetDefaultOptionIndex(Option), false);
}

TArray<FText> UMKHVoiceSettingsSectionWidget::GetOptionValues(EMKHVoiceSettingOption Option) const
{
	switch (Option)
	{
	case EMKHVoiceSettingOption::VoiceChat:
	case EMKHVoiceSettingOption::NonFriends:
		return { LOCTEXT("Off", "Off"), LOCTEXT("On", "On") };
	case EMKHVoiceSettingOption::MicMode:
		return { LOCTEXT("OpenMic", "Open Mic"), LOCTEXT("PushToTalk", "Push-to-Talk"), LOCTEXT("MicDisabled", "Disabled (Listen Only)") };
	case EMKHVoiceSettingOption::GameOutputDevice:
	case EMKHVoiceSettingOption::VoiceOutputDevice:
	{
		TArray<FText> Values = { Option == EMKHVoiceSettingOption::GameOutputDevice ? LOCTEXT("SystemDefault", "System Default")
			: LOCTEXT("SameAsGame", "Same as Game") };
		const UMKHAudioDeviceSubsystem* Devices = UMKHAudioDeviceSubsystem::Get(this);
		for (const FMKHAudioOutputDevice& Device : IsValid(Devices) ? Devices->GetOutputDevices() : TArray<FMKHAudioOutputDevice>())
		{
			// Device names come from the system: shown as they are, never translated.
			Values.Add(FText::AsCultureInvariant(Device.DisplayName));
		}
		return Values;
	}
	case EMKHVoiceSettingOption::InputDevice:
	{
		const UMKHAudioDeviceSubsystem* Devices = UMKHAudioDeviceSubsystem::Get(this);
		const FString Name = IsValid(Devices) ? Devices->GetInputDeviceName() : FString();
		return { Name.IsEmpty() ? LOCTEXT("WindowsDefaultMic", "Windows Default Microphone") : FText::AsCultureInvariant(Name) };
	}
	default:
		return {};
	}
}

int32 UMKHVoiceSettingsSectionWidget::GetOptionIndex(EMKHVoiceSettingOption Option) const
{
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	const UMKHAudioDeviceSubsystem* Devices = UMKHAudioDeviceSubsystem::Get(this);
	switch (Option)
	{
	case EMKHVoiceSettingOption::VoiceChat:
		return IsValid(VoiceChat) && VoiceChat->IsVoiceChatEnabled() ? MKHVoiceSettings::OnIndex : 0;
	case EMKHVoiceSettingOption::NonFriends:
		return IsValid(VoiceChat) && VoiceChat->AllowsVoiceWithNonFriends() ? MKHVoiceSettings::OnIndex : 0;
	case EMKHVoiceSettingOption::MicMode:
		return IsValid(VoiceChat) ? static_cast<int32>(VoiceChat->GetMicMode()) : GetDefaultOptionIndex(Option);
	case EMKHVoiceSettingOption::GameOutputDevice:
	case EMKHVoiceSettingOption::VoiceOutputDevice:
	{
		const FString Saved = !IsValid(Devices) ? FString() : Option == EMKHVoiceSettingOption::GameOutputDevice
			? Devices->GetGameOutputDeviceId() : Devices->GetVoiceOutputDeviceId();
		// A saved device that is not connected shows the default entry (the game uses the default meanwhile).
		return FMath::Max(0, GetDeviceIds().IndexOfByKey(Saved));
	}
	default:
		return 0;
	}
}

int32 UMKHVoiceSettingsSectionWidget::GetDefaultOptionIndex(EMKHVoiceSettingOption Option)
{
	switch (Option)
	{
	case EMKHVoiceSettingOption::VoiceChat:
		return MKHVoiceSettings::OnIndex;
	case EMKHVoiceSettingOption::MicMode:
		return static_cast<int32>(EMKHVoiceMicMode::PushToTalk);
	default:
		return 0;
	}
}

void UMKHVoiceSettingsSectionWidget::SetOptionIndex(EMKHVoiceSettingOption Option, int32 NewIndex) const
{
	UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	UMKHAudioDeviceSubsystem* Devices = UMKHAudioDeviceSubsystem::Get(this);
	const TArray<FString> DeviceIds = GetDeviceIds();
	const FString DeviceId = DeviceIds.IsValidIndex(NewIndex) ? DeviceIds[NewIndex] : FString();
	switch (Option)
	{
	case EMKHVoiceSettingOption::VoiceChat:
		if (IsValid(VoiceChat)) { VoiceChat->SetVoiceChatEnabled(NewIndex == MKHVoiceSettings::OnIndex); }
		break;
	case EMKHVoiceSettingOption::NonFriends:
		if (IsValid(VoiceChat)) { VoiceChat->SetAllowVoiceWithNonFriends(NewIndex == MKHVoiceSettings::OnIndex); }
		break;
	case EMKHVoiceSettingOption::MicMode:
		if (IsValid(VoiceChat)) { VoiceChat->SetMicMode(static_cast<EMKHVoiceMicMode>(FMath::Clamp(NewIndex, 0, 2))); }
		break;
	case EMKHVoiceSettingOption::GameOutputDevice:
		if (IsValid(Devices)) { Devices->SetGameOutputDeviceId(DeviceId); }
		break;
	case EMKHVoiceSettingOption::VoiceOutputDevice:
		if (IsValid(Devices)) { Devices->SetVoiceOutputDeviceId(DeviceId); }
		break;
	default:
		break;
	}
}

TArray<FString> UMKHVoiceSettingsSectionWidget::GetDeviceIds() const
{
	TArray<FString> Ids = { FString() };
	const UMKHAudioDeviceSubsystem* Devices = UMKHAudioDeviceSubsystem::Get(this);
	for (const FMKHAudioOutputDevice& Device : IsValid(Devices) ? Devices->GetOutputDevices() : TArray<FMKHAudioOutputDevice>())
	{
		Ids.Add(Device.DeviceId);
	}
	return Ids;
}

float UMKHVoiceSettingsSectionWidget::GetSliderValue(EMKHVoiceSettingOption Option) const
{
	const UMKHGameUserSettings* Settings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(Settings))
	{
		return GetDefaultSliderValue(Option);
	}
	switch (Option)
	{
	case EMKHVoiceSettingOption::InputVolume:
		return Settings->GetVoiceInputVolume() * MKHVoiceSettings::SliderScale;
	case EMKHVoiceSettingOption::ActivationThreshold:
		return Settings->GetVoiceActivationThreshold() * MKHVoiceSettings::SliderScale;
	case EMKHVoiceSettingOption::OutputVolume:
		return Settings->GetVoiceOutputVolume() * MKHVoiceSettings::SliderScale;
	default:
		return 0.f;
	}
}

void UMKHVoiceSettingsSectionWidget::SetSliderValue(EMKHVoiceSettingOption Option, float Value) const
{
	UMKHGameUserSettings* Settings = UMKHGameUserSettings::GetMKHGameUserSettings();
	UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	if (!IsValid(Settings))
	{
		return;
	}
	const float Normalized = Value / MKHVoiceSettings::SliderScale;
	switch (Option)
	{
	case EMKHVoiceSettingOption::InputVolume:
		Settings->SetVoiceInputVolume(Normalized);
		break;
	case EMKHVoiceSettingOption::ActivationThreshold:
		Settings->SetVoiceActivationThreshold(Normalized);
		break;
	case EMKHVoiceSettingOption::OutputVolume:
		Settings->SetVoiceOutputVolume(Normalized);
		break;
	default:
		break;
	}
	if (IsValid(VoiceChat))
	{
		VoiceChat->ApplyVoiceSettings();
	}
}

float UMKHVoiceSettingsSectionWidget::GetDefaultSliderValue(EMKHVoiceSettingOption Option)
{
	switch (Option)
	{
	case EMKHVoiceSettingOption::InputVolume:
		return 50.f;
	case EMKHVoiceSettingOption::ActivationThreshold:
		return 20.f;
	default:
		return 100.f;
	}
}

FText UMKHVoiceSettingsSectionWidget::GetOptionDisplayName(EMKHVoiceSettingOption Option) const
{
	switch (Option)
	{
	case EMKHVoiceSettingOption::GameOutputDevice:    return LOCTEXT("GameOutputDevice", "Game Output Device");
	case EMKHVoiceSettingOption::VoiceChat:           return LOCTEXT("VoiceChat", "Voice Chat");
	case EMKHVoiceSettingOption::MicMode:             return LOCTEXT("MicMode", "Microphone Mode");
	case EMKHVoiceSettingOption::NonFriends:          return LOCTEXT("NonFriends", "Voice with Non-Friends");
	case EMKHVoiceSettingOption::InputVolume:         return LOCTEXT("InputVolume", "Microphone Volume");
	case EMKHVoiceSettingOption::MicLevel:            return LOCTEXT("MicLevel", "Microphone Level");
	case EMKHVoiceSettingOption::ActivationThreshold: return LOCTEXT("ActivationThreshold", "Open Mic Sensitivity");
	case EMKHVoiceSettingOption::OutputVolume:        return LOCTEXT("OutputVolume", "Voice Volume");
	case EMKHVoiceSettingOption::VoiceOutputDevice:   return LOCTEXT("VoiceOutputDevice", "Voice Output Device");
	case EMKHVoiceSettingOption::InputDevice:         return LOCTEXT("InputDevice", "Microphone");
	default:                                          return FText::GetEmpty();
	}
}

FText UMKHVoiceSettingsSectionWidget::GetOptionDescription(EMKHVoiceSettingOption Option) const
{
	switch (Option)
	{
	case EMKHVoiceSettingOption::GameOutputDevice:    return LOCTEXT("GameOutputDeviceDesc", "Where the game sound plays.");
	case EMKHVoiceSettingOption::VoiceChat:           return LOCTEXT("VoiceChatDesc", "Off: you neither hear the others nor speak.");
	case EMKHVoiceSettingOption::MicMode:             return LOCTEXT("MicModeDesc", "Set the push-to-talk key on the Keyboard and Controller pages.");
	case EMKHVoiceSettingOption::NonFriends:          return LOCTEXT("NonFriendsDesc", "Voice with opponents who are neither friends nor in your party. Both players must turn it on.");
	case EMKHVoiceSettingOption::InputVolume:         return LOCTEXT("InputVolumeDesc", "How loud the others hear you.");
	case EMKHVoiceSettingOption::MicLevel:            return LOCTEXT("MicLevelDesc", "Speak to test your microphone. The mark shows the open mic sensitivity.");
	case EMKHVoiceSettingOption::ActivationThreshold: return LOCTEXT("ActivationThresholdDesc", "Open mic: your voice is sent only above this level.");
	case EMKHVoiceSettingOption::OutputVolume:        return LOCTEXT("OutputVolumeDesc", "How loud you hear the others. The master volume does not change it.");
	case EMKHVoiceSettingOption::VoiceOutputDevice:   return LOCTEXT("VoiceOutputDeviceDesc", "Where the voice of the others plays.");
	case EMKHVoiceSettingOption::InputDevice:         return LOCTEXT("InputDeviceDesc", "Change it in the Windows sound settings.");
	default:                                          return FText::GetEmpty();
	}
}

FName UMKHVoiceSettingsSectionWidget::ToOptionId(EMKHVoiceSettingOption Option)
{
	return FName(*UEnum::GetValueAsString(Option));
}

bool UMKHVoiceSettingsSectionWidget::FromOptionId(FName OptionId, EMKHVoiceSettingOption& OutOption)
{
	const int64 Value = StaticEnum<EMKHVoiceSettingOption>()->GetValueByName(OptionId);
	if (Value == INDEX_NONE)
	{
		return false;
	}
	OutOption = static_cast<EMKHVoiceSettingOption>(Value);
	return true;
}

// ============================================================
// Protected / Internal Logic - Callbacks
// ============================================================

void UMKHVoiceSettingsSectionWidget::HandleOptionStepped(FName OptionId, int32 Delta)
{
	EMKHVoiceSettingOption Option;
	if (!FromOptionId(OptionId, Option) || Option == EMKHVoiceSettingOption::InputDevice)
	{
		return;
	}
	const int32 NumValues = GetOptionValues(Option).Num();
	SetOptionIndex(Option, FMath::Clamp(GetOptionIndex(Option) + Delta, 0, FMath::Max(0, NumValues - 1)));
	RefreshRowValues();
}

void UMKHVoiceSettingsSectionWidget::HandleOptionReset(FName OptionId)
{
	EMKHVoiceSettingOption Option;
	if (FromOptionId(OptionId, Option) && Option != EMKHVoiceSettingOption::InputDevice)
	{
		SetOptionIndex(Option, GetDefaultOptionIndex(Option));
		RefreshRowValues();
	}
}

void UMKHVoiceSettingsSectionWidget::HandleSliderChanged(FName OptionId, float Value)
{
	EMKHVoiceSettingOption Option;
	if (FromOptionId(OptionId, Option))
	{
		SetSliderValue(Option, Value);
		RefreshRowValues();
	}
}

void UMKHVoiceSettingsSectionWidget::HandleSliderCommitted(FName OptionId)
{
	if (UMKHGameUserSettings* Settings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(Settings))
	{
		Settings->SaveSettings();
	}
}

void UMKHVoiceSettingsSectionWidget::HandleSliderReset(FName OptionId)
{
	EMKHVoiceSettingOption Option;
	if (FromOptionId(OptionId, Option))
	{
		SetSliderValue(Option, GetDefaultSliderValue(Option));
		HandleSliderCommitted(OptionId);
		RefreshRowValues();
	}
}

void UMKHVoiceSettingsSectionWidget::HandleExternalChange()
{
	RefreshRowValues();
}

void UMKHVoiceSettingsSectionWidget::UnbindRows()
{
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
}

#undef LOCTEXT_NAMESPACE
