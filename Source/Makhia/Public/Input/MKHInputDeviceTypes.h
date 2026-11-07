// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHInputDeviceTypes.generated.h"

/**
 * Family of input devices a key binding belongs to. Keyboard and mouse share one set of bindings, the
 * gamepad has its own; see Documentation/SettingsArchitecture.md ("Keyboard and controller bindings").
 */
UENUM(BlueprintType)
enum class EMKHInputDevice : uint8
{
	KeyboardMouse	UMETA(DisplayName = "Keyboard & Mouse"),
	Gamepad			UMETA(DisplayName = "Gamepad")
};
