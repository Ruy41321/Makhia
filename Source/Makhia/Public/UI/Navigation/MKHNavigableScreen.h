// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MKHNavigableScreen.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMKHNavigableScreen : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional hooks a screen (a widget on the viewport, or a modal pushed with
 * UMKHMenuNavigationSubsystem::PushModalScreen) gives the gamepad menu navigation.
 *
 * Screens without it get the default behaviour: the focus goes to the remembered widget, the
 * Desired Focus Widget or the first focusable widget. See Documentation/GamepadMenuNavigation.md.
 */
class MAKHIA_API IMKHNavigableScreen
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Whether the screen keeps the focus on its own root widget instead of on a child (it reads the pad
	 * buttons itself, e.g. the key capture modal while it listens).
	 *
	 * @return True to leave the focus on the root.
	 */
	virtual bool WantsRootFocus() const { return false; }
};
