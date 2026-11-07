// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHKeyboardSettingsWidget.h"

#include "Algo/Find.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Libraries/MKHInputSettingsLibrary.h"
#include "MKHLogChannels.h"
#include "UI/HUD/Settings/MKHKeyCaptureWidget.h"
#include "UI/HUD/Settings/MKHKeybindCategoryWidget.h"
#include "UI/HUD/Settings/MKHKeybindRowWidget.h"

namespace
{
	/**
	 * Whether two labels name the same category. Compares the English source text, so the grouping and
	 * CategoryOrder keep working whatever the current language and however each copy was translated.
	 */
	bool IsSameCategoryLabel(const FText& A, const FText& B)
	{
		const FString* SourceA = FTextInspector::GetSourceString(A);
		const FString* SourceB = FTextInspector::GetSourceString(B);
		return SourceA != nullptr && SourceB != nullptr ? SourceA->Equals(*SourceB) : A.EqualTo(B);
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHKeyboardSettingsWidget::NativeOnPageActivated()
{
	Super::NativeOnPageActivated();

	SetIsFocusable(true);
	TakeSnapshot();
	RebuildKeybindList();
}

void UMKHKeyboardSettingsWidget::NativeOnPageDeactivated()
{
	CloseCaptureModal();

	// The keyboard and controller pages share one live profile: an Apply on the other page would save
	// these rebinds without this page knowing, so unsaved ones are dropped when the page is left.
	if (bHasUnsavedRebinds)
	{
		RevertChanges();
	}

	Super::NativeOnPageDeactivated();
}

void UMKHKeyboardSettingsWidget::NativeDestruct()
{
	CloseCaptureModal();

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHKeyboardSettingsWidget::ApplyChanges()
{
	UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	if (!IsValid(UserSettings))
	{
		return;
	}

	UserSettings->SaveSettings();

	// The saved state is the new baseline for Revert.
	TakeSnapshot();
	bHasUnsavedRebinds = false;
	NotifyDirtyStateChanged();
}

void UMKHKeyboardSettingsWidget::RevertChanges()
{
	for (const FMKHKeybindSnapshotEntry& Entry : SavedSnapshot)
	{
		WriteMapping(Entry.MappingName, Entry.Slot, Entry.Key);
	}

	bHasUnsavedRebinds = false;
	RebuildKeybindList();
	NotifyDirtyStateChanged();
}

void UMKHKeyboardSettingsWidget::ResetToDefaults()
{
	const UEnhancedPlayerMappableKeyProfile* Profile = UMKHInputSettingsLibrary::GetActiveKeyProfile(GetOwningPlayer());
	if (!IsValid(Profile))
	{
		return;
	}

	// Not ResetKeyProfileIdToDefault(): that would also reset the other device's page.
	TArray<FName> MappingNames;
	Profile->GetPlayerMappingRows().GetKeys(MappingNames);
	for (const FName& MappingName : MappingNames)
	{
		ResetRowToDefaults(MappingName);
	}

	bHasUnsavedRebinds = true;
	RebuildKeybindList();
	NotifyDirtyStateChanged();
}

bool UMKHKeyboardSettingsWidget::HasPendingChanges() const
{
	return bHasUnsavedRebinds;
}

bool UMKHKeyboardSettingsWidget::IsBlockingClose() const
{
	return IsValid(ActiveCaptureWidget);
}

void UMKHKeyboardSettingsWidget::RebuildKeybindList()
{
	if (!IsValid(KeybindListContainer))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("KeyboardSettings: KeybindListContainer is not bound, nothing can be listed."));
		return;
	}

	for (const TObjectPtr<UMKHKeybindRowWidget>& Row : RowWidgets)
	{
		if (IsValid(Row))
		{
			Row->OnSlotClicked.RemoveAll(this);
			Row->OnResetClicked.RemoveAll(this);
		}
	}
	RowWidgets.Reset();
	KeybindListContainer->ClearChildren();

	TArray<FMKHKeybindCategoryGroup> Categories;
	CollectMappingsByCategory(Categories);

	for (const FMKHKeybindCategoryGroup& Category : Categories)
	{
		BuildCategorySection(Category.DisplayName, Category.MappingNames);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHKeyboardSettingsWidget::CollectMappingsByCategory(TArray<FMKHKeybindCategoryGroup>& OutCategories) const
{
	const UEnhancedPlayerMappableKeyProfile* Profile = UMKHInputSettingsLibrary::GetActiveKeyProfile(GetOwningPlayer());
	if (!IsValid(Profile))
	{
		UE_LOG(LogMKHInput, Warning,
			TEXT("KeyboardSettings: no active key profile. Check bEnableUserSettings and that the IMC is registered."));
		return;
	}

	for (const TPair<FName, FKeyMappingRow>& Row : Profile->GetPlayerMappingRows())
	{
		if (ExcludedMappingNames.Contains(Row.Key))
		{
			continue;
		}

		// Only actions that have a binding on this page's device are listed.
		const FPlayerKeyMapping* DeviceMapping = Algo::FindByPredicate(Row.Value.Mappings, [this](const FPlayerKeyMapping& Mapping)
		{
			return UMKHInputSettingsLibrary::IsMappingForDevice(Mapping, InputDevice);
		});
		if (DeviceMapping == nullptr)
		{
			continue;
		}

		const FText CategoryLabel = DeviceMapping->GetDisplayCategory().IsEmpty()
			? UncategorizedLabel
			: DeviceMapping->GetDisplayCategory();

		FMKHKeybindCategoryGroup* Group = OutCategories.FindByPredicate([&CategoryLabel](const FMKHKeybindCategoryGroup& Candidate)
		{
			return IsSameCategoryLabel(Candidate.DisplayName, CategoryLabel);
		});

		if (Group == nullptr)
		{
			Group = &OutCategories.AddDefaulted_GetRef();
			Group->DisplayName = CategoryLabel;
		}

		Group->MappingNames.Add(Row.Key);
	}

	// CategoryOrder / MappingOrder win; whatever the designer did not pin falls back to
	// alphabetical, so the list order never depends on map iteration order.
	OutCategories.Sort([this](const FMKHKeybindCategoryGroup& A, const FMKHKeybindCategoryGroup& B)
	{
		const int32 RankA = GetCategoryRank(A.DisplayName);
		const int32 RankB = GetCategoryRank(B.DisplayName);
		return RankA != RankB ? RankA < RankB : A.DisplayName.CompareTo(B.DisplayName) < 0;
	});

	for (FMKHKeybindCategoryGroup& Group : OutCategories)
	{
		Group.MappingNames.Sort([this](const FName& A, const FName& B)
		{
			const int32 RankA = GetMappingRank(A);
			const int32 RankB = GetMappingRank(B);
			return RankA != RankB
				? RankA < RankB
				: GetMappingDisplayName(A).CompareTo(GetMappingDisplayName(B)) < 0;
		});
	}
}

void UMKHKeyboardSettingsWidget::BuildCategorySection(const FText& CategoryLabel, const TArray<FName>& MappingNames)
{
	if (IsValid(CategoryHeaderClass))
	{
		if (UMKHKeybindCategoryWidget* Header = CreateWidget<UMKHKeybindCategoryWidget>(GetOwningPlayer(), CategoryHeaderClass); IsValid(Header))
		{
			Header->SetCategoryName(CategoryLabel, MappingNames.Num());
			// The Action / Primary / Secondary captions describe the two keyboard columns only.
			Header->SetColumnHeadersVisible(InputDevice == EMKHInputDevice::KeyboardMouse);
			if (UVerticalBoxSlot* HeaderSlot = Cast<UVerticalBoxSlot>(KeybindListContainer->AddChild(Header)); HeaderSlot != nullptr)
			{
				HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
	}

	if (!IsValid(KeybindRowClass))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("KeyboardSettings: KeybindRowClass is not set, no row can be created."));
		return;
	}

	for (const FName& MappingName : MappingNames)
	{
		UMKHKeybindRowWidget* Row = CreateWidget<UMKHKeybindRowWidget>(GetOwningPlayer(), KeybindRowClass);
		if (!IsValid(Row))
		{
			continue;
		}

		Row->OnSlotClicked.AddUObject(this, &UMKHKeyboardSettingsWidget::HandleSlotClicked);
		Row->OnResetClicked.AddUObject(this, &UMKHKeyboardSettingsWidget::HandleRowResetClicked);

		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(KeybindListContainer->AddChild(Row)); RowSlot != nullptr)
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		Row->SetupRow(MappingName, GetMappingDisplayName(MappingName), InputDevice);

		RowWidgets.Add(Row);
	}
}

void UMKHKeyboardSettingsWidget::HandleSlotClicked(FName InMappingName, EPlayerMappableKeySlot InSlot)
{
	if (!IsValid(KeyCaptureWidgetClass) || IsValid(ActiveCaptureWidget))
	{
		return;
	}

	ActiveCaptureWidget = CreateWidget<UMKHKeyCaptureWidget>(GetOwningPlayer(), KeyCaptureWidgetClass);
	if (!IsValid(ActiveCaptureWidget))
	{
		return;
	}

	PendingMappingName = InMappingName;
	PendingSlot = InSlot;
	PendingConflictName = NAME_None;

	ActiveCaptureWidget->OnKeyCaptured.AddUObject(this, &UMKHKeyboardSettingsWidget::HandleKeyCaptured);
	ActiveCaptureWidget->OnConflictConfirmed.AddUObject(this, &UMKHKeyboardSettingsWidget::HandleConflictConfirmed);
	ActiveCaptureWidget->OnClearRequested.AddUObject(this, &UMKHKeyboardSettingsWidget::HandleClearRequested);
	ActiveCaptureWidget->OnCaptureCancelled.AddUObject(this, &UMKHKeyboardSettingsWidget::HandleCaptureCancelled);

	ActiveCaptureWidget->AddToViewport(CaptureModalZOrder);
	const FKey ChordKey = UMKHInputSettingsLibrary::GetActionKey(GetOwningPlayer(), GetMappingChordAction(InMappingName), InputDevice);
	ActiveCaptureWidget->BeginCapture(GetMappingDisplayName(InMappingName), InSlot, InputDevice, ChordKey);
}

void UMKHKeyboardSettingsWidget::HandleRowResetClicked(FName InMappingName)
{
	ResetRowToDefaults(InMappingName);

	bHasUnsavedRebinds = true;
	RebuildKeybindList();
	NotifyDirtyStateChanged();
}

void UMKHKeyboardSettingsWidget::HandleKeyCaptured(const FKey& CapturedKey)
{
	FName ConflictName = NAME_None;
	if (FindConflictingMapping(CapturedKey, ConflictName))
	{
		PendingConflictName = ConflictName;

		if (IsValid(ActiveCaptureWidget))
		{
			ActiveCaptureWidget->ShowConflict(BuildConflictMessage(CapturedKey, ConflictName));
		}
		return;
	}

	ApplyPendingRebind(CapturedKey);
}

void UMKHKeyboardSettingsWidget::HandleConflictConfirmed(const FKey& CapturedKey)
{
	ApplyPendingRebind(CapturedKey);
}

void UMKHKeyboardSettingsWidget::HandleClearRequested()
{
	ApplyPendingRebind(FKey());
}

void UMKHKeyboardSettingsWidget::HandleCaptureCancelled()
{
	CloseCaptureModal();
}

void UMKHKeyboardSettingsWidget::ApplyPendingRebind(const FKey& NewKey)
{
	const UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	const FPlayerKeyMapping* PendingMapping =
		UMKHInputSettingsLibrary::FindMappingForSlot(UserSettings, PendingMappingName, PendingSlot, InputDevice);
	const FKey PreviousKey = PendingMapping != nullptr ? PendingMapping->GetCurrentKey() : FKey();

	// Whoever owned this key loses it: a replace never leaves the same key on two actions.
	if (!PendingConflictName.IsNone() && NewKey.IsValid())
	{
		ReleaseConflictingKey(NewKey, PreviousKey);
	}

	if (WriteMapping(PendingMappingName, PendingSlot, NewKey))
	{
		bHasUnsavedRebinds = true;
	}

	CloseCaptureModal();
	RebuildKeybindList();
	NotifyDirtyStateChanged();
}

void UMKHKeyboardSettingsWidget::ReleaseConflictingKey(const FKey& NewKey, const FKey& PreviousKey)
{
	const UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	if (!IsValid(UserSettings))
	{
		return;
	}

	for (const FPlayerKeyMapping& Mapping : UserSettings->FindMappingsInRow(PendingConflictName))
	{
		if (Mapping.GetCurrentKey() == NewKey && UMKHInputSettingsLibrary::IsMappingForDevice(Mapping, InputDevice))
		{
			// An empty gamepad slot would fall back to its default key, so the controller swaps instead.
			const FKey ReplacementKey = InputDevice == EMKHInputDevice::Gamepad ? PreviousKey : FKey();
			WriteMapping(PendingConflictName, Mapping.GetSlot(), ReplacementKey);
			return;
		}
	}
}

FText UMKHKeyboardSettingsWidget::BuildConflictMessage(const FKey& Key, FName ConflictName) const
{
	if (InputDevice == EMKHInputDevice::Gamepad)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Button"), UMKHInputSettingsLibrary::GetKeyDisplayName(Key));
		Args.Add(TEXT("Action"), GetMappingDisplayName(ConflictName));
		return FText::Format(
			NSLOCTEXT("MKHSettings", "ControllerBindConflict", "{Button} is already bound to {Action}. Replacing it swaps the two buttons."),
			Args);
	}

	return FText::Format(
		NSLOCTEXT("MKHSettings", "KeybindConflict", "{0} is already bound to {1}. Replacing it will leave that slot empty."),
		UMKHInputSettingsLibrary::GetKeyDisplayName(Key),
		GetMappingDisplayName(ConflictName));
}

void UMKHKeyboardSettingsWidget::ResetRowToDefaults(FName InMappingName)
{
	const UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	if (!IsValid(UserSettings))
	{
		return;
	}

	// Copied out first: writing a key rehashes the mapping inside the row's set.
	TArray<TPair<EPlayerMappableKeySlot, FKey>> Defaults;
	for (const FPlayerKeyMapping& Mapping : UserSettings->FindMappingsInRow(InMappingName))
	{
		if (UMKHInputSettingsLibrary::IsMappingForDevice(Mapping, InputDevice) && Mapping.IsCustomized())
		{
			Defaults.Emplace(Mapping.GetSlot(), Mapping.GetDefaultKey());
		}
	}

	for (const TPair<EPlayerMappableKeySlot, FKey>& Default : Defaults)
	{
		WriteMapping(InMappingName, Default.Key, Default.Value);
	}
}

bool UMKHKeyboardSettingsWidget::FindConflictingMapping(const FKey& Key, FName& OutConflictName) const
{
	const UEnhancedPlayerMappableKeyProfile* Profile = UMKHInputSettingsLibrary::GetActiveKeyProfile(GetOwningPlayer());
	if (!IsValid(Profile) || !Key.IsValid())
	{
		return false;
	}

	TArray<FName> MappingNames;
	Profile->GetMappingNamesForKey(Key, MappingNames);

	// LB + RB and RB alone are different inputs: only mappings sharing the same chord collide.
	const UInputAction* PendingChord = GetMappingChordAction(PendingMappingName);
	for (const FName& Name : MappingNames)
	{
		if (Name != PendingMappingName && !ExcludedMappingNames.Contains(Name) && GetMappingChordAction(Name) == PendingChord)
		{
			OutConflictName = Name;
			return true;
		}
	}

	return false;
}

const UInputAction* UMKHKeyboardSettingsWidget::GetMappingChordAction(FName InMappingName) const
{
	const UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	if (!IsValid(UserSettings))
	{
		return nullptr;
	}

	for (const FPlayerKeyMapping& Mapping : UserSettings->FindMappingsInRow(InMappingName))
	{
		if (UMKHInputSettingsLibrary::IsMappingForDevice(Mapping, InputDevice))
		{
			return UMKHInputSettingsLibrary::FindChordAction(UserSettings, Mapping.GetAssociatedInputAction(), InputDevice);
		}
	}
	return nullptr;
}

void UMKHKeyboardSettingsWidget::CloseCaptureModal()
{
	if (!IsValid(ActiveCaptureWidget))
	{
		return;
	}

	ActiveCaptureWidget->OnKeyCaptured.RemoveAll(this);
	ActiveCaptureWidget->OnConflictConfirmed.RemoveAll(this);
	ActiveCaptureWidget->OnClearRequested.RemoveAll(this);
	ActiveCaptureWidget->OnCaptureCancelled.RemoveAll(this);

	ActiveCaptureWidget->RemoveFromParent();
	ActiveCaptureWidget = nullptr;

	// Hand the focus back inside the screen, otherwise Esc has nowhere to land after a rebind.
	SetKeyboardFocus();

	PendingMappingName = NAME_None;
	PendingConflictName = NAME_None;
}

FText UMKHKeyboardSettingsWidget::GetMappingDisplayName(FName InMappingName) const
{
	const UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	if (IsValid(UserSettings))
	{
		for (const FPlayerKeyMapping& Mapping : UserSettings->FindMappingsInRow(InMappingName))
		{
			if (!Mapping.GetDisplayName().IsEmpty())
			{
				return Mapping.GetDisplayName();
			}
		}
	}

	return FText::FromName(InMappingName);
}

int32 UMKHKeyboardSettingsWidget::GetCategoryRank(const FText& CategoryLabel) const
{
	for (int32 Index = 0; Index < CategoryOrder.Num(); ++Index)
	{
		if (IsSameCategoryLabel(CategoryOrder[Index], CategoryLabel))
		{
			return Index;
		}
	}

	return MAX_int32;
}

int32 UMKHKeyboardSettingsWidget::GetMappingRank(FName InMappingName) const
{
	const int32 Index = MappingOrder.IndexOfByKey(InMappingName);
	return Index != INDEX_NONE ? Index : MAX_int32;
}

void UMKHKeyboardSettingsWidget::TakeSnapshot()
{
	SavedSnapshot.Reset();

	const UEnhancedPlayerMappableKeyProfile* Profile = UMKHInputSettingsLibrary::GetActiveKeyProfile(GetOwningPlayer());
	if (!IsValid(Profile))
	{
		return;
	}

	for (const TPair<FName, FKeyMappingRow>& Row : Profile->GetPlayerMappingRows())
	{
		for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
		{
			if (!UMKHInputSettingsLibrary::IsMappingForDevice(Mapping, InputDevice))
			{
				continue;
			}

			FMKHKeybindSnapshotEntry Entry;
			Entry.MappingName = Row.Key;
			Entry.Slot = Mapping.GetSlot();
			Entry.Key = Mapping.GetCurrentKey();

			SavedSnapshot.Add(Entry);
		}
	}
}

bool UMKHKeyboardSettingsWidget::WriteMapping(FName InMappingName, EPlayerMappableKeySlot InSlot, const FKey& NewKey)
{
	UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	if (!IsValid(UserSettings) || InMappingName.IsNone())
	{
		return false;
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = InMappingName;
	Args.Slot = InSlot;
	Args.HardwareDeviceId = UMKHInputSettingsLibrary::GetHardwareDeviceIdName(InputDevice);
	Args.ProfileIdString = UserSettings->GetActiveKeyProfileId();

	FGameplayTagContainer FailureReason;

	if (NewKey.IsValid())
	{
		Args.NewKey = NewKey;
		UserSettings->MapPlayerKey(Args, FailureReason);
	}
	else
	{
		// Clearing a slot needs the key currently in it, so look it up first.
		const FPlayerKeyMapping* Current = UMKHInputSettingsLibrary::FindMappingForSlot(UserSettings, InMappingName, InSlot, InputDevice);
		if (Current == nullptr || !Current->GetCurrentKey().IsValid())
		{
			return false;
		}

		Args.NewKey = Current->GetCurrentKey();
		UserSettings->UnMapPlayerKey(Args, FailureReason);
	}

	if (!FailureReason.IsEmpty())
	{
		UE_LOG(LogMKHInput, Warning, TEXT("KeyboardSettings: rebind of %s rejected (%s)"),
			*InMappingName.ToString(), *FailureReason.ToString());
		return false;
	}

	UserSettings->ApplySettings();
	return true;
}
