// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Input/MKHInputUserSettings.h"

#include "EnhancedActionKeyMapping.h"
#include "InputMappingContext.h"
#include "MKHLogChannels.h"

FHardwareDeviceIdentifier UMKHInputUserSettings::DetermineHardwareDeviceForActionMapping(
	const FEnhancedActionKeyMapping& ActionMapping, const UInputMappingContext* IMC) const
{
	return ActionMapping.Key.IsGamepadKey()
		? FHardwareDeviceIdentifier::DefaultGamepad
		: FHardwareDeviceIdentifier::Invalid;
}

bool UMKHInputUserSettings::RegisterKeyMappingsToProfile(UEnhancedPlayerMappableKeyProfile& Profile, const UInputMappingContext* IMC)
{
	const bool bRegistered = Super::RegisterKeyMappingsToProfile(Profile, IMC);
	WarnAboutInvalidKeys(IMC);
	return bRegistered;
}

void UMKHInputUserSettings::WarnAboutInvalidKeys(const UInputMappingContext* IMC) const
{
	if (!IsValid(IMC))
	{
		return;
	}

	// An unknown key name (e.g. "(" from a bad FKey import) is shown as is by every prompt: catch it at start-up.
	for (const FEnhancedActionKeyMapping& Mapping : IMC->GetMappings())
	{
		UE_CLOG(!Mapping.Key.IsValid(), LogMKHInput, Warning, TEXT("Input settings: %s maps %s to the unknown key '%s'."),
			*IMC->GetName(), *GetNameSafe(Mapping.Action), *Mapping.Key.ToString());
	}
}
