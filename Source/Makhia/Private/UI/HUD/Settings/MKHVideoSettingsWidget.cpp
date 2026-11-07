// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHVideoSettingsWidget.h"

#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameUserSettings.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MKHLogChannels.h"
#include "Online/Backend/MKHOnlineServicesSubsystem.h"
#include "UI/HUD/Settings/MKHKeybindCategoryWidget.h"
#include "UI/HUD/Settings/MKHOptionRowWidget.h"
#include "UI/HUD/Settings/MKHVideoConfirmWidget.h"

#define LOCTEXT_NAMESPACE "MKHSettings"

namespace MKHVideo
{
	/** Sections in display order. */
	static const TArray<EMKHVideoOption> DisplaySection = {
		EMKHVideoOption::WindowMode, EMKHVideoOption::Resolution,
		EMKHVideoOption::VSync, EMKHVideoOption::FrameRateLimit };

	/** Global Illumination is left out: with baked lighting and no Lumen it changes nothing. */
	static const TArray<EMKHVideoOption> QualitySection = {
		EMKHVideoOption::QualityPreset, EMKHVideoOption::AntiAliasing,
		EMKHVideoOption::PostProcessing, EMKHVideoOption::Shadows,
		EMKHVideoOption::Reflections,
		EMKHVideoOption::Textures, EMKHVideoOption::Effects, EMKHVideoOption::Shading };

	static const TArray<EMKHVideoOption> AdvancedSection = { EMKHVideoOption::ResolutionScale };

	/** Index used by the preset row when the eight groups disagree. */
	constexpr int32 CustomPresetIndex = 4;
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHVideoSettingsWidget::NativeOnPageActivated()
{
	Super::NativeOnPageActivated();

	CollectSupportedResolutions();
	ReadFromUserSettings();
	RebuildOptionList();
}

void UMKHVideoSettingsWidget::NativeOnPageDeactivated()
{
	CloseConfirmModal();

	Super::NativeOnPageDeactivated();
}

void UMKHVideoSettingsWidget::NativeDestruct()
{
	CloseConfirmModal();

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHVideoSettingsWidget::ApplyChanges()
{
	UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	if (!IsValid(UserSettings))
	{
		return;
	}

	const bool bModeChanged =
		StagedValues.FindRef(EMKHVideoOption::Resolution) != AppliedValues.FindRef(EMKHVideoOption::Resolution) ||
		StagedValues.FindRef(EMKHVideoOption::WindowMode) != AppliedValues.FindRef(EMKHVideoOption::WindowMode);

	// --- display ---
	const int32 WindowIndex = StagedValues.FindRef(EMKHVideoOption::WindowMode);
	UserSettings->SetFullscreenMode(static_cast<EWindowMode::Type>(FMath::Clamp(WindowIndex, 0, 2)));

	if (ResolutionOptions.IsValidIndex(StagedValues.FindRef(EMKHVideoOption::Resolution)))
	{
		UserSettings->SetScreenResolution(ResolutionOptions[StagedValues.FindRef(EMKHVideoOption::Resolution)]);
	}

	UserSettings->SetVSyncEnabled(StagedValues.FindRef(EMKHVideoOption::VSync) != 0);

	if (FrameRateLimits.IsValidIndex(StagedValues.FindRef(EMKHVideoOption::FrameRateLimit)))
	{
		UserSettings->SetFrameRateLimit(static_cast<float>(
			FrameRateLimits[StagedValues.FindRef(EMKHVideoOption::FrameRateLimit)]));
	}

	// --- quality groups ---
	UserSettings->SetAntiAliasingQuality(StagedValues.FindRef(EMKHVideoOption::AntiAliasing));
	UserSettings->SetPostProcessingQuality(StagedValues.FindRef(EMKHVideoOption::PostProcessing));
	UserSettings->SetShadowQuality(StagedValues.FindRef(EMKHVideoOption::Shadows));
	UserSettings->SetGlobalIlluminationQuality(StagedValues.FindRef(EMKHVideoOption::GlobalIllumination));
	UserSettings->SetReflectionQuality(StagedValues.FindRef(EMKHVideoOption::Reflections));
	UserSettings->SetTextureQuality(StagedValues.FindRef(EMKHVideoOption::Textures));
	UserSettings->SetVisualEffectQuality(StagedValues.FindRef(EMKHVideoOption::Effects));
	UserSettings->SetShadingQuality(StagedValues.FindRef(EMKHVideoOption::Shading));

	if (ResolutionScaleSteps.IsValidIndex(StagedValues.FindRef(EMKHVideoOption::ResolutionScale)))
	{
		const int32 Percent = ResolutionScaleSteps[StagedValues.FindRef(EMKHVideoOption::ResolutionScale)];
		UserSettings->SetResolutionScaleNormalized(static_cast<float>(Percent) / 100.0f);
	}

	UserSettings->ApplySettings(false);
	ReadFromUserSettings();
	RebuildOptionList();
	NotifyDirtyStateChanged();

	// A mode the display cannot show hides the panel itself, so it has to be confirmed.
	if (bModeChanged && IsValid(ConfirmWidgetClass) && !IsValid(ActiveConfirmWidget))
	{
		ActiveConfirmWidget = CreateWidget<UMKHVideoConfirmWidget>(GetOwningPlayer(), ConfirmWidgetClass);
		if (IsValid(ActiveConfirmWidget))
		{
			ActiveConfirmWidget->OnModeConfirmed.AddUObject(this, &UMKHVideoSettingsWidget::HandleModeConfirmed);
			ActiveConfirmWidget->OnModeReverted.AddUObject(this, &UMKHVideoSettingsWidget::HandleModeReverted);
			ActiveConfirmWidget->AddToViewport(ConfirmModalZOrder);
			ActiveConfirmWidget->BeginCountdown();
		}
	}
}

void UMKHVideoSettingsWidget::RevertChanges()
{
	ReadFromUserSettings();
	RebuildOptionList();
	NotifyDirtyStateChanged();
}

void UMKHVideoSettingsWidget::ResetToDefaults()
{
	UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	if (!IsValid(UserSettings))
	{
		return;
	}

	UserSettings->SetToDefaults();

	// Defaults mean "what this machine can run", not Epic: the engine default would be Epic.
	if (UMKHGameUserSettings* MKHSettings = Cast<UMKHGameUserSettings>(UserSettings); IsValid(MKHSettings))
	{
		const UMKHOnlineServicesSubsystem* OnlineServices = UMKHOnlineServicesSubsystem::Get(this);
		MKHSettings->StageAutoDetectedQuality(IsValid(OnlineServices) && OnlineServices->IsRunningOnHandheld());
	}

	ReadFromUserSettings();

	// Defaults are staged like any other edit: nothing reaches the screen without Apply.
	AppliedValues.Empty();
	RebuildOptionList();
	NotifyDirtyStateChanged();
}

bool UMKHVideoSettingsWidget::HasPendingChanges() const
{
	for (const TPair<EMKHVideoOption, int32>& Staged : StagedValues)
	{
		if (AppliedValues.FindRef(Staged.Key) != Staged.Value)
		{
			return true;
		}
	}

	return false;
}

bool UMKHVideoSettingsWidget::IsBlockingClose() const
{
	return IsValid(ActiveConfirmWidget);
}

void UMKHVideoSettingsWidget::RebuildOptionList()
{
	if (!IsValid(OptionListContainer))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("VideoSettings: OptionListContainer is not bound."));
		return;
	}

	for (const TObjectPtr<UMKHOptionRowWidget>& Row : RowWidgets)
	{
		if (IsValid(Row))
		{
			Row->OnOptionStepped.RemoveAll(this);
			Row->OnOptionReset.RemoveAll(this);
		}
	}
	RowWidgets.Reset();
	OptionListContainer->ClearChildren();

	BuildSection(LOCTEXT("VideoDisplay", "Display"),   MKHVideo::DisplaySection);
	BuildSection(LOCTEXT("VideoQuality", "Quality"),   MKHVideo::QualitySection);
	BuildSection(LOCTEXT("VideoAdvanced", "Advanced"), MKHVideo::AdvancedSection);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHVideoSettingsWidget::CollectSupportedResolutions()
{
	ResolutionOptions.Reset();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(ResolutionOptions);

	if (ResolutionOptions.IsEmpty())
	{
		// Windowed-only setups report nothing; the current mode is still a valid choice.
		if (const UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings(); IsValid(UserSettings))
		{
			ResolutionOptions.Add(UserSettings->GetScreenResolution());
		}
	}
}

void UMKHVideoSettingsWidget::ReadFromUserSettings()
{
	const UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	if (!IsValid(UserSettings))
	{
		return;
	}

	StagedValues.Reset();
	StagedValues.Add(EMKHVideoOption::WindowMode, static_cast<int32>(UserSettings->GetFullscreenMode()));

	const int32 ResIndex = ResolutionOptions.IndexOfByKey(UserSettings->GetScreenResolution());
	StagedValues.Add(EMKHVideoOption::Resolution, ResIndex != INDEX_NONE ? ResIndex : 0);

	StagedValues.Add(EMKHVideoOption::VSync, UserSettings->IsVSyncEnabled() ? 1 : 0);

	const int32 Limit = FMath::RoundToInt(UserSettings->GetFrameRateLimit());
	const int32 LimitIndex = FrameRateLimits.IndexOfByKey(Limit);
	StagedValues.Add(EMKHVideoOption::FrameRateLimit,
		LimitIndex != INDEX_NONE ? LimitIndex : FMath::Max(0, FrameRateLimits.Num() - 1));

	StagedValues.Add(EMKHVideoOption::AntiAliasing,       UserSettings->GetAntiAliasingQuality());
	StagedValues.Add(EMKHVideoOption::PostProcessing,     UserSettings->GetPostProcessingQuality());
	StagedValues.Add(EMKHVideoOption::Shadows,            UserSettings->GetShadowQuality());
	StagedValues.Add(EMKHVideoOption::GlobalIllumination, UserSettings->GetGlobalIlluminationQuality());
	StagedValues.Add(EMKHVideoOption::Reflections,        UserSettings->GetReflectionQuality());
	StagedValues.Add(EMKHVideoOption::Textures,           UserSettings->GetTextureQuality());
	StagedValues.Add(EMKHVideoOption::Effects,            UserSettings->GetVisualEffectQuality());
	StagedValues.Add(EMKHVideoOption::Shading,            UserSettings->GetShadingQuality());

	const int32 ScalePercent = FMath::RoundToInt(UserSettings->GetResolutionScaleNormalized() * 100.0f);
	int32 ClosestStep = 0;
	int32 ClosestDelta = MAX_int32;
	for (int32 Index = 0; Index < ResolutionScaleSteps.Num(); ++Index)
	{
		const int32 Delta = FMath::Abs(ResolutionScaleSteps[Index] - ScalePercent);
		if (Delta < ClosestDelta)
		{
			ClosestDelta = Delta;
			ClosestStep = Index;
		}
	}
	StagedValues.Add(EMKHVideoOption::ResolutionScale, ClosestStep);

	RefreshPresetFromGroups();
	AppliedValues = StagedValues;
}

void UMKHVideoSettingsWidget::RefreshPresetFromGroups()
{
	int32 Shared = INDEX_NONE;
	bool bUniform = true;

	for (const EMKHVideoOption Option : MKHVideo::QualitySection)
	{
		if (!IsQualityGroup(Option))
		{
			continue;
		}

		const int32 Level = StagedValues.FindRef(Option);
		if (Shared == INDEX_NONE)
		{
			Shared = Level;
		}
		else if (Shared != Level)
		{
			bUniform = false;
			break;
		}
	}

	StagedValues.Add(EMKHVideoOption::QualityPreset,
		bUniform && Shared != INDEX_NONE ? Shared : MKHVideo::CustomPresetIndex);
}

void UMKHVideoSettingsWidget::BuildSection(const FText& SectionLabel, const TArray<EMKHVideoOption>& Options)
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

	if (!IsValid(OptionRowClass))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("VideoSettings: OptionRowClass is not set."));
		return;
	}

	for (const EMKHVideoOption Option : Options)
	{
		UMKHOptionRowWidget* Row = CreateWidget<UMKHOptionRowWidget>(GetOwningPlayer(), OptionRowClass);
		if (!IsValid(Row))
		{
			continue;
		}

		Row->OnOptionStepped.AddUObject(this, &UMKHVideoSettingsWidget::HandleOptionStepped);
		Row->OnOptionReset.AddUObject(this, &UMKHVideoSettingsWidget::HandleOptionReset);

		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(OptionListContainer->AddChild(Row)); RowSlot != nullptr)
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		Row->SetupRow(ToOptionId(Option), GetOptionDisplayName(Option), GetOptionDescription(Option));

		const TArray<FText> Values = GetOptionValues(Option);
		const int32 Index = FMath::Clamp(StagedValues.FindRef(Option), 0, FMath::Max(0, Values.Num() - 1));
		const bool bModified = StagedValues.FindRef(Option) != AppliedValues.FindRef(Option);
		const bool bCustom = Option == EMKHVideoOption::QualityPreset && Index == MKHVideo::CustomPresetIndex;

		Row->SetValue(Values.IsValidIndex(Index) ? Values[Index] : FText::GetEmpty(),
			Index, Values.Num(), bModified, bCustom);

		RowWidgets.Add(Row);
	}
}

void UMKHVideoSettingsWidget::HandleOptionStepped(FName OptionId, int32 Delta)
{
	EMKHVideoOption Option;
	if (!FromOptionId(OptionId, Option))
	{
		return;
	}

	const TArray<FText> Values = GetOptionValues(Option);

	// The preset lists Custom as a readout only: the player steps between the real levels.
	const int32 MaxIndex = Option == EMKHVideoOption::QualityPreset
		? MKHVideo::CustomPresetIndex - 1
		: Values.Num() - 1;

	const int32 NewIndex = FMath::Clamp(StagedValues.FindRef(Option) + Delta, 0, MaxIndex);
	StagedValues.Add(Option, NewIndex);

	if (Option == EMKHVideoOption::QualityPreset)
	{
		// The preset drives the eight groups, exactly like SetOverallScalabilityLevel would.
		for (const EMKHVideoOption Group : MKHVideo::QualitySection)
		{
			if (IsQualityGroup(Group))
			{
				StagedValues.Add(Group, NewIndex);
			}
		}
	}
	else if (IsQualityGroup(Option))
	{
		RefreshPresetFromGroups();
	}

	RebuildOptionList();
	NotifyDirtyStateChanged();
}

void UMKHVideoSettingsWidget::HandleOptionReset(FName OptionId)
{
	EMKHVideoOption Option;
	if (!FromOptionId(OptionId, Option))
	{
		return;
	}

	StagedValues.Add(Option, AppliedValues.FindRef(Option));

	if (Option == EMKHVideoOption::QualityPreset)
	{
		for (const EMKHVideoOption Group : MKHVideo::QualitySection)
		{
			if (IsQualityGroup(Group))
			{
				StagedValues.Add(Group, AppliedValues.FindRef(Group));
			}
		}
	}

	RefreshPresetFromGroups();
	RebuildOptionList();
	NotifyDirtyStateChanged();
}

void UMKHVideoSettingsWidget::HandleModeConfirmed()
{
	if (UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->ConfirmVideoMode();
		UserSettings->SaveSettings();
	}

	CloseConfirmModal();
}

void UMKHVideoSettingsWidget::HandleModeReverted()
{
	if (UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->RevertVideoMode();
		UserSettings->ApplyResolutionSettings(false);
	}

	CloseConfirmModal();
	ReadFromUserSettings();
	RebuildOptionList();
	NotifyDirtyStateChanged();
}

void UMKHVideoSettingsWidget::CloseConfirmModal()
{
	if (!IsValid(ActiveConfirmWidget))
	{
		return;
	}

	ActiveConfirmWidget->OnModeConfirmed.RemoveAll(this);
	ActiveConfirmWidget->OnModeReverted.RemoveAll(this);
	ActiveConfirmWidget->RemoveFromParent();
	ActiveConfirmWidget = nullptr;
}

// ============================================================
// Option metadata
// ============================================================

TArray<FText> UMKHVideoSettingsWidget::GetOptionValues(EMKHVideoOption Option) const
{
	static const TArray<FText> QualityLevels = {
		LOCTEXT("QualityLow", "Low"), LOCTEXT("QualityMedium", "Medium"),
		LOCTEXT("QualityHigh", "High"), LOCTEXT("QualityEpic", "Epic") };

	switch (Option)
	{
	case EMKHVideoOption::WindowMode:
		return { LOCTEXT("WindowFullscreen", "Fullscreen"),
				 LOCTEXT("WindowBorderless", "Borderless"),
				 LOCTEXT("WindowWindowed", "Windowed") };

	case EMKHVideoOption::Resolution:
	{
		TArray<FText> Values;
		for (const FIntPoint& Res : ResolutionOptions)
		{
			Values.Add(FText::Format(LOCTEXT("ResolutionFormat", "{0} × {1}"),
				FText::AsNumber(Res.X, &FNumberFormattingOptions::DefaultNoGrouping()),
				FText::AsNumber(Res.Y, &FNumberFormattingOptions::DefaultNoGrouping())));
		}
		return Values;
	}

	case EMKHVideoOption::VSync:
		return { LOCTEXT("Off", "Off"), LOCTEXT("On", "On") };

	case EMKHVideoOption::FrameRateLimit:
	{
		TArray<FText> Values;
		for (const int32 Limit : FrameRateLimits)
		{
			Values.Add(Limit > 0 ? FText::AsNumber(Limit) : LOCTEXT("Unlimited", "Unlimited"));
		}
		return Values;
	}

	case EMKHVideoOption::QualityPreset:
	{
		TArray<FText> Values = QualityLevels;
		Values.Add(LOCTEXT("QualityCustom", "Custom"));
		return Values;
	}

	case EMKHVideoOption::ResolutionScale:
	{
		TArray<FText> Values;
		for (const int32 Percent : ResolutionScaleSteps)
		{
			Values.Add(FText::AsPercent(Percent / 100.0f, &FNumberFormattingOptions::DefaultNoGrouping()));
		}
		return Values;
	}

	default:
		return QualityLevels;
	}
}

FText UMKHVideoSettingsWidget::GetOptionDisplayName(EMKHVideoOption Option) const
{
	switch (Option)
	{
	case EMKHVideoOption::WindowMode:         return LOCTEXT("OptWindowMode", "Window Mode");
	case EMKHVideoOption::Resolution:         return LOCTEXT("OptResolution", "Resolution");
	case EMKHVideoOption::VSync:              return LOCTEXT("OptVSync", "V-Sync");
	case EMKHVideoOption::FrameRateLimit:     return LOCTEXT("OptFrameRate", "Frame Rate Limit");
	case EMKHVideoOption::QualityPreset:      return LOCTEXT("OptPreset", "Quality Preset");
	case EMKHVideoOption::AntiAliasing:       return LOCTEXT("OptAA", "Anti-Aliasing");
	case EMKHVideoOption::PostProcessing:     return LOCTEXT("OptPost", "Post Processing");
	case EMKHVideoOption::Shadows:            return LOCTEXT("OptShadows", "Shadows");
	case EMKHVideoOption::GlobalIllumination: return LOCTEXT("OptGI", "Global Illumination");
	case EMKHVideoOption::Reflections:        return LOCTEXT("OptReflections", "Reflections");
	case EMKHVideoOption::Textures:           return LOCTEXT("OptTextures", "Textures");
	case EMKHVideoOption::Effects:            return LOCTEXT("OptEffects", "Effects");
	case EMKHVideoOption::Shading:            return LOCTEXT("OptShading", "Shading");
	case EMKHVideoOption::ResolutionScale:    return LOCTEXT("OptResScale", "Resolution Scale");
	default:                                  return FText::GetEmpty();
	}
}

FText UMKHVideoSettingsWidget::GetOptionDescription(EMKHVideoOption Option) const
{
	switch (Option)
	{
	case EMKHVideoOption::WindowMode:         return LOCTEXT("DescWindowMode", "How the game fills the screen");
	case EMKHVideoOption::Resolution:         return LOCTEXT("DescResolution", "Asks for confirmation after Apply");
	case EMKHVideoOption::VSync:              return LOCTEXT("DescVSync", "Avoids tearing, adds latency");
	case EMKHVideoOption::FrameRateLimit:     return LOCTEXT("DescFrameRate", "Caps how many frames are rendered");
	case EMKHVideoOption::QualityPreset:      return LOCTEXT("DescPreset", "Sets every option below at once");
	case EMKHVideoOption::GlobalIllumination: return LOCTEXT("DescGI", "Indirect lighting; the heaviest setting here");
	case EMKHVideoOption::Shadows:            return LOCTEXT("DescShadows", "Shadow resolution and draw distance");
	case EMKHVideoOption::Textures:           return LOCTEXT("DescTextures", "Texture detail; costs video memory");
	case EMKHVideoOption::ResolutionScale:    return LOCTEXT("DescResScale", "Renders below native resolution and upscales");
	default:                                  return FText::GetEmpty();
	}
}

FName UMKHVideoSettingsWidget::ToOptionId(EMKHVideoOption Option)
{
	return FName(*FString::Printf(TEXT("VideoOption_%d"), static_cast<int32>(Option)));
}

bool UMKHVideoSettingsWidget::FromOptionId(FName OptionId, EMKHVideoOption& OutOption)
{
	FString Left;
	FString Right;
	if (!OptionId.ToString().Split(TEXT("_"), &Left, &Right))
	{
		return false;
	}

	OutOption = static_cast<EMKHVideoOption>(FCString::Atoi(*Right));
	return true;
}

bool UMKHVideoSettingsWidget::IsQualityGroup(EMKHVideoOption Option)
{
	return Option == EMKHVideoOption::AntiAliasing
		|| Option == EMKHVideoOption::PostProcessing
		|| Option == EMKHVideoOption::Shadows
		|| Option == EMKHVideoOption::GlobalIllumination
		|| Option == EMKHVideoOption::Reflections
		|| Option == EMKHVideoOption::Textures
		|| Option == EMKHVideoOption::Effects
		|| Option == EMKHVideoOption::Shading;
}

#undef LOCTEXT_NAMESPACE
