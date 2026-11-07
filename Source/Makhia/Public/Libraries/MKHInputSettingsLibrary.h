// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/MKHInputDeviceTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "MKHInputSettingsLibrary.generated.h"

class APlayerController;
class UInputAction;
class UMKHActiveInputDeviceSubsystem;

/**
 * Shared access to the Enhanced Input user settings, used by every widget of the keybind settings
 * pages (keyboard and controller).
 *
 * The settings object lives on the local player's Enhanced Input subsystem and only exists when
 * bEnableUserSettings is on in Config/DefaultInput.ini; every accessor here returns null instead
 * of asserting, so the UI degrades to an empty list rather than crashing on a misconfigured build.
 */
UCLASS()
class MAKHIA_API UMKHInputSettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the Enhanced Input user settings of a local player.
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @return The settings object, or nullptr when user settings are disabled or the player is remote.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	static UEnhancedInputUserSettings* GetInputUserSettings(const APlayerController* PlayerController);

	/**
	 * Returns the key profile currently in use by a local player.
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @return The active key profile, or nullptr when unavailable.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	static UEnhancedPlayerMappableKeyProfile* GetActiveKeyProfile(const APlayerController* PlayerController);

	/**
	 * Player-facing name of a key, suitable for a keycap label. Gamepad buttons use their short Xbox
	 * label ("A", "RB", "LT") instead of the engine's long name, which does not fit a keycap. Other keys use the
	 * engine's English name in every game language ("Tab", never the Italian "Scheda"), see Localization.md.
	 *
	 * @param Key  Key to describe.
	 * @return The key's display name, or an empty text for an invalid key.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	static FText GetKeyDisplayName(const FKey& Key);

	/**
	 * Player-facing name of the key currently bound to an action, for on-screen prompts: the player's
	 * primary (possibly remapped) key for that device when the action is player mappable, otherwise the
	 * first key of that device the applied mapping contexts map to it. A chorded mapping is shown with
	 * its chord first ("LB + RB").
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @param Action            Action to describe.
	 * @param Device            Device family whose key is wanted.
	 * @return The key's display name, or an empty text when the action has no key on that device.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	static FText GetActionKeyDisplayName(const APlayerController* PlayerController, const UInputAction* Action,
		EMKHInputDevice Device = EMKHInputDevice::KeyboardMouse);

	/**
	 * Key name for an on-screen prompt, on the device the player is using right now (see
	 * UMKHActiveInputDeviceSubsystem). Falls back to the keyboard key when the action has no gamepad key.
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @param Action            Action to describe.
	 * @param bIncludeChord     False to show only the mapping's own button ("RB" instead of "LB + RB"), for keycaps
	 *                          too small for both, whose host shows the chord elsewhere.
	 * @return The key's display name, or an empty text when the action has no key at all.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	static FText GetActionKeyDisplayNameForActiveDevice(const APlayerController* PlayerController, const UInputAction* Action,
		bool bIncludeChord = true);

	/**
	 * Device family a local player used last.
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @return The active device, or keyboard and mouse when unknown (remote player, no Slate).
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	static EMKHInputDevice GetActiveInputDevice(const APlayerController* PlayerController);

	/**
	 * The subsystem tracking a local player's active device, for widgets that refresh on a switch.
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @return The subsystem, or nullptr for a remote or missing player.
	 */
	static UMKHActiveInputDeviceSubsystem* GetActiveInputDeviceSubsystem(const APlayerController* PlayerController);

	/**
	 * Whether a key belongs to a device family.
	 *
	 * @param Key     Key to test.
	 * @param Device  Device family.
	 * @return True for gamepad keys when Device is Gamepad, for every other key otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	static bool IsKeyForDevice(const FKey& Key, EMKHInputDevice Device);

	/**
	 * Whether a player mapping is one of a device family's bindings (see UMKHInputUserSettings).
	 *
	 * @param Mapping  Mapping from the key profile.
	 * @param Device   Device family.
	 * @return True when the mapping was registered for that device.
	 */
	static bool IsMappingForDevice(const FPlayerKeyMapping& Mapping, EMKHInputDevice Device);

	/**
	 * Hardware device id to put in FMapPlayerKeyArgs so MapPlayerKey targets a device's mapping.
	 *
	 * @param Device  Device family.
	 * @return The id the engine matches against FPlayerKeyMapping::GetHardwareDeviceId().
	 */
	static FName GetHardwareDeviceIdName(EMKHInputDevice Device);

	/**
	 * Finds the mapping of one action in one slot of one device. Replaces FindCurrentMappingForSlot,
	 * which ignores the device and would return either the keyboard or the gamepad mapping.
	 *
	 * @param UserSettings  Settings to search.
	 * @param MappingName   Action mapping name.
	 * @param Slot          Slot wanted.
	 * @param Device        Device family wanted.
	 * @return The mapping, or nullptr when the action has none there.
	 */
	static const FPlayerKeyMapping* FindMappingForSlot(const UEnhancedInputUserSettings* UserSettings, FName MappingName,
		EPlayerMappableKeySlot Slot, EMKHInputDevice Device);

	/**
	 * Label of a player mapping for a keycap: its key, preceded by the chord the mapping context requires
	 * (e.g. "LB + RB" for a skill chorded on Block).
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @param Mapping           Mapping from the key profile.
	 * @param Device            Device family the mapping belongs to.
	 * @return The label, or an empty text when the mapping has no key.
	 */
	static FText GetMappingKeyDisplayName(const APlayerController* PlayerController, const FPlayerKeyMapping& Mapping, EMKHInputDevice Device);

	/**
	 * Action that must be held for an action's mapping on a device to fire (Chorded Action trigger),
	 * looked up in every mapping context registered with the user settings, applied or not.
	 *
	 * @param UserSettings  Settings whose registered contexts are searched.
	 * @param Action        Action whose mapping is inspected.
	 * @param Device        Device family of the mapping.
	 * @return The chord action, or nullptr when the mapping is not chorded.
	 */
	static const UInputAction* FindChordAction(const UEnhancedInputUserSettings* UserSettings, const UInputAction* Action, EMKHInputDevice Device);

	/**
	 * Key currently bound to an action on a device: the player's primary (possibly remapped) key when the
	 * action is player mappable, otherwise the first key of that device the applied mapping contexts map to it.
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @param Action            Action to look up.
	 * @param Device            Device family whose key is wanted.
	 * @return The key, or an invalid key when the action has none on that device.
	 */
	static FKey GetActionKey(const APlayerController* PlayerController, const UInputAction* Action, EMKHInputDevice Device);

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Key of an action on a device, without any chord prefix (see GetActionKeyDisplayName).
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @param Action            Action to describe.
	 * @param Device            Device family whose key is wanted.
	 * @return The key's display name, or an empty text when the action has no key on that device.
	 */
	static FText GetPlainActionKeyDisplayName(const APlayerController* PlayerController, const UInputAction* Action, EMKHInputDevice Device);

	/**
	 * Prepends the chord of an action's mapping to a key label.
	 *
	 * @param PlayerController  Owning controller; must be a local one.
	 * @param Action            Action whose mapping may be chorded.
	 * @param Device            Device family of the mapping.
	 * @param KeyName           Label of the mapping's own key.
	 * @return "Chord + Key" for a chorded mapping, KeyName otherwise.
	 */
	static FText AddChordPrefix(const APlayerController* PlayerController, const UInputAction* Action, EMKHInputDevice Device, const FText& KeyName);
};
