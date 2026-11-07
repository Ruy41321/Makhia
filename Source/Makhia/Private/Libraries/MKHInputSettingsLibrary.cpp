// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Libraries/MKHInputSettingsLibrary.h"

#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Input/MKHActiveInputDeviceSubsystem.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace MKHInputSettings
{
	/** Short keycap label of a gamepad button (Xbox naming), or an empty text for keys without one. */
	FText GetGamepadShortLabel(const FKey& Key)
	{
		// Button names stay English in every language, like the engine key names (see Localization.md).
		static const TMap<FKey, FText> Labels = {
			{ EKeys::Gamepad_FaceButton_Bottom, INVTEXT("A") },
			{ EKeys::Gamepad_FaceButton_Right, INVTEXT("B") },
			{ EKeys::Gamepad_FaceButton_Left, INVTEXT("X") },
			{ EKeys::Gamepad_FaceButton_Top, INVTEXT("Y") },
			{ EKeys::Gamepad_LeftShoulder, INVTEXT("LB") },
			{ EKeys::Gamepad_RightShoulder, INVTEXT("RB") },
			{ EKeys::Gamepad_LeftTrigger, INVTEXT("LT") },
			{ EKeys::Gamepad_RightTrigger, INVTEXT("RT") },
			{ EKeys::Gamepad_LeftTriggerAxis, INVTEXT("LT") },
			{ EKeys::Gamepad_RightTriggerAxis, INVTEXT("RT") },
			{ EKeys::Gamepad_LeftThumbstick, INVTEXT("L3") },
			{ EKeys::Gamepad_RightThumbstick, INVTEXT("R3") },
			{ EKeys::Gamepad_DPad_Up, INVTEXT("\u2191") },
			{ EKeys::Gamepad_DPad_Down, INVTEXT("\u2193") },
			{ EKeys::Gamepad_DPad_Left, INVTEXT("\u2190") },
			{ EKeys::Gamepad_DPad_Right, INVTEXT("\u2192") },
			{ EKeys::Gamepad_Special_Left, INVTEXT("View") },
			{ EKeys::Gamepad_Special_Right, INVTEXT("Menu") },
			{ EKeys::Gamepad_Left2D, INVTEXT("LS") },
			{ EKeys::Gamepad_Right2D, INVTEXT("RS") },
		};

		const FText* Label = Labels.Find(Key);
		return Label != nullptr ? *Label : FText::GetEmpty();
	}

	/** The engine's English name of a key, whatever the game language (Engine.locres would turn "Tab" into "Scheda"). */
	FText GetEnglishKeyName(const FKey& Key)
	{
		const FText DisplayName = Key.GetDisplayName();
		const FString* SourceName = FTextInspector::GetSourceString(DisplayName);
		return SourceName != nullptr ? FText::AsCultureInvariant(*SourceName) : DisplayName;
	}
}

UEnhancedInputUserSettings* UMKHInputSettingsLibrary::GetInputUserSettings(const APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return nullptr;
	}

	const UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());

	return IsValid(Subsystem) ? Subsystem->GetUserSettings() : nullptr;
}

UEnhancedPlayerMappableKeyProfile* UMKHInputSettingsLibrary::GetActiveKeyProfile(const APlayerController* PlayerController)
{
	const UEnhancedInputUserSettings* UserSettings = GetInputUserSettings(PlayerController);
	return IsValid(UserSettings) ? UserSettings->GetActiveKeyProfile() : nullptr;
}

FText UMKHInputSettingsLibrary::GetKeyDisplayName(const FKey& Key)
{
	if (!Key.IsValid())
	{
		return FText::GetEmpty();
	}

	if (Key.IsGamepadKey())
	{
		const FText ShortLabel = MKHInputSettings::GetGamepadShortLabel(Key);
		if (!ShortLabel.IsEmpty())
		{
			return ShortLabel;
		}
	}
	return MKHInputSettings::GetEnglishKeyName(Key);
}

FText UMKHInputSettingsLibrary::GetActionKeyDisplayNameForActiveDevice(const APlayerController* PlayerController, const UInputAction* Action,
	bool bIncludeChord)
{
	const EMKHInputDevice Device = GetActiveInputDevice(PlayerController);
	const FText KeyName = bIncludeChord
		? GetActionKeyDisplayName(PlayerController, Action, Device)
		: GetPlainActionKeyDisplayName(PlayerController, Action, Device);

	// Actions with no gamepad button (e.g. Toggle HUD) still show something the player can press.
	if (KeyName.IsEmpty() && Device != EMKHInputDevice::KeyboardMouse)
	{
		return GetActionKeyDisplayName(PlayerController, Action, EMKHInputDevice::KeyboardMouse);
	}
	return KeyName;
}

EMKHInputDevice UMKHInputSettingsLibrary::GetActiveInputDevice(const APlayerController* PlayerController)
{
	const UMKHActiveInputDeviceSubsystem* Subsystem = GetActiveInputDeviceSubsystem(PlayerController);
	return IsValid(Subsystem) ? Subsystem->GetActiveInputDevice() : EMKHInputDevice::KeyboardMouse;
}

UMKHActiveInputDeviceSubsystem* UMKHInputSettingsLibrary::GetActiveInputDeviceSubsystem(const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = IsValid(PlayerController) ? PlayerController->GetLocalPlayer() : nullptr;
	return ULocalPlayer::GetSubsystem<UMKHActiveInputDeviceSubsystem>(LocalPlayer);
}

FText UMKHInputSettingsLibrary::GetActionKeyDisplayName(const APlayerController* PlayerController, const UInputAction* Action,
	EMKHInputDevice Device)
{
	const FText KeyName = GetPlainActionKeyDisplayName(PlayerController, Action, Device);
	return KeyName.IsEmpty() ? KeyName : AddChordPrefix(PlayerController, Action, Device, KeyName);
}

FText UMKHInputSettingsLibrary::GetPlainActionKeyDisplayName(const APlayerController* PlayerController, const UInputAction* Action,
	EMKHInputDevice Device)
{
	return GetKeyDisplayName(GetActionKey(PlayerController, Action, Device));
}

FKey UMKHInputSettingsLibrary::GetActionKey(const APlayerController* PlayerController, const UInputAction* Action, EMKHInputDevice Device)
{
	if (!IsValid(Action))
	{
		return EKeys::Invalid;
	}

	// The profile knows the remapped key even while the action's mapping context is not applied (e.g. UI focus).
	const UEnhancedInputUserSettings* UserSettings = GetInputUserSettings(PlayerController);
	const UPlayerMappableKeySettings* MappableSettings = Action->GetPlayerMappableKeySettings();
	if (IsValid(UserSettings) && IsValid(MappableSettings))
	{
		const FPlayerKeyMapping* Mapping = FindMappingForSlot(UserSettings, MappableSettings->Name, EPlayerMappableKeySlot::First, Device);
		if (Mapping != nullptr && Mapping->GetCurrentKey().IsValid())
		{
			return Mapping->GetCurrentKey();
		}
	}

	const ULocalPlayer* LocalPlayer = IsValid(PlayerController) ? PlayerController->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!IsValid(Subsystem))
	{
		return EKeys::Invalid;
	}

	for (const FKey& Key : Subsystem->QueryKeysMappedToAction(Action))
	{
		if (IsKeyForDevice(Key, Device))
		{
			return Key;
		}
	}
	return EKeys::Invalid;
}

FText UMKHInputSettingsLibrary::GetMappingKeyDisplayName(const APlayerController* PlayerController, const FPlayerKeyMapping& Mapping,
	EMKHInputDevice Device)
{
	const FText KeyName = GetKeyDisplayName(Mapping.GetCurrentKey());
	return KeyName.IsEmpty() ? KeyName : AddChordPrefix(PlayerController, Mapping.GetAssociatedInputAction(), Device, KeyName);
}

const UInputAction* UMKHInputSettingsLibrary::FindChordAction(const UEnhancedInputUserSettings* UserSettings, const UInputAction* Action,
	EMKHInputDevice Device)
{
	if (!IsValid(UserSettings) || !IsValid(Action))
	{
		return nullptr;
	}

	// The registered contexts, not the applied ones: the settings screen runs under the UI context.
	for (const TObjectPtr<const UInputMappingContext>& Context : UserSettings->GetRegisteredInputMappingContexts())
	{
		if (!IsValid(Context))
		{
			continue;
		}

		for (const FEnhancedActionKeyMapping& ContextMapping : Context->GetMappings())
		{
			if (ContextMapping.Action != Action || !IsKeyForDevice(ContextMapping.Key, Device))
			{
				continue;
			}

			for (const TObjectPtr<UInputTrigger>& Trigger : ContextMapping.Triggers)
			{
				if (const UInputTriggerChordAction* Chord = Cast<UInputTriggerChordAction>(Trigger); IsValid(Chord) && IsValid(Chord->ChordAction))
				{
					return Chord->ChordAction;
				}
			}
		}
	}
	return nullptr;
}

FText UMKHInputSettingsLibrary::AddChordPrefix(const APlayerController* PlayerController, const UInputAction* Action, EMKHInputDevice Device,
	const FText& KeyName)
{
	const UInputAction* ChordAction = FindChordAction(GetInputUserSettings(PlayerController), Action, Device);
	if (!IsValid(ChordAction))
	{
		return KeyName;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Chord"), GetPlainActionKeyDisplayName(PlayerController, ChordAction, Device));
	Args.Add(TEXT("Key"), KeyName);
	return FText::Format(INVTEXT("{Chord} + {Key}"), Args);
}

bool UMKHInputSettingsLibrary::IsKeyForDevice(const FKey& Key, EMKHInputDevice Device)
{
	return Key.IsGamepadKey() == (Device == EMKHInputDevice::Gamepad);
}

bool UMKHInputSettingsLibrary::IsMappingForDevice(const FPlayerKeyMapping& Mapping, EMKHInputDevice Device)
{
	const bool bIsGamepadMapping = Mapping.GetPrimaryDeviceType() == EHardwareDevicePrimaryType::Gamepad;
	return bIsGamepadMapping == (Device == EMKHInputDevice::Gamepad);
}

FName UMKHInputSettingsLibrary::GetHardwareDeviceIdName(EMKHInputDevice Device)
{
	// Must mirror UMKHInputUserSettings::DetermineHardwareDeviceForActionMapping.
	return Device == EMKHInputDevice::Gamepad
		? FHardwareDeviceIdentifier::DefaultGamepad.HardwareDeviceIdentifier
		: FHardwareDeviceIdentifier::Invalid.HardwareDeviceIdentifier;
}

const FPlayerKeyMapping* UMKHInputSettingsLibrary::FindMappingForSlot(const UEnhancedInputUserSettings* UserSettings, FName MappingName,
	EPlayerMappableKeySlot Slot, EMKHInputDevice Device)
{
	if (!IsValid(UserSettings))
	{
		return nullptr;
	}

	for (const FPlayerKeyMapping& Mapping : UserSettings->FindMappingsInRow(MappingName))
	{
		if (Mapping.GetSlot() == Slot && IsMappingForDevice(Mapping, Device))
		{
			return &Mapping;
		}
	}
	return nullptr;
}
