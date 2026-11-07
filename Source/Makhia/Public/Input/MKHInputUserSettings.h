// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "MKHInputUserSettings.generated.h"

/**
 * Enhanced Input user settings that keep keyboard/mouse and gamepad bindings apart.
 *
 * The engine numbers the slots of a mapping per hardware device. By default every mapping goes to the
 * same (invalid) device, so a gamepad key authored next to a keyboard key in a mapping context would
 * take the keyboard's Secondary slot. Here gamepad keys are registered on the default gamepad device:
 * each device gets its own First/Second slots and the two settings pages never see each other's keys.
 *
 * Keyboard and mouse keys stay on the invalid device, so profiles saved before controller support keep
 * matching their slots. Enabled through UserSettingsClass in Config/DefaultInput.ini.
 *
 * Also warns at start-up about mapping context keys the engine does not know (see Documentation/SettingsArchitecture.md).
 */
UCLASS()
class MAKHIA_API UMKHInputUserSettings : public UEnhancedInputUserSettings
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Registers the player mappable keys of a context, then warns about keys the engine does not know.
	 *
	 * @param Profile  Key profile being filled.
	 * @param IMC      Context being registered.
	 * @return True when the engine registered the context.
	 */
	virtual bool RegisterKeyMappingsToProfile(UEnhancedPlayerMappableKeyProfile& Profile, const UInputMappingContext* IMC) override;

	/**
	 * Logs a warning for every mapping of a context whose key the engine does not know.
	 *
	 * @param IMC  Context being registered.
	 */
	void WarnAboutInvalidKeys(const UInputMappingContext* IMC) const;

	/**
	 * Picks the hardware device a default mapping is registered under.
	 *
	 * @param ActionMapping  Mapping authored in the mapping context.
	 * @param IMC            Context the mapping comes from.
	 * @return The default gamepad device for gamepad keys, the invalid device for everything else.
	 */
	virtual FHardwareDeviceIdentifier DetermineHardwareDeviceForActionMapping(
		const FEnhancedActionKeyMapping& ActionMapping, const UInputMappingContext* IMC = nullptr) const override;
};
