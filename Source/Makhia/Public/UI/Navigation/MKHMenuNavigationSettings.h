// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MKHMenuNavigationSettings.generated.h"

/**
 * Look and timing of the gamepad menu navigation (Project Settings -> Game -> Makhia Menu Navigation).
 * See Documentation/GamepadMenuNavigation.md.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Makhia Menu Navigation"))
class MAKHIA_API UMKHMenuNavigationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	//~ End UDeveloperSettings Interface

	// ============================================================
	// Properties
	// ============================================================

	/** Outline colour of the frame drawn around the focused widget (UMKHButton draws its own hover look instead). */
	UPROPERTY(Config, EditAnywhere, Category = "Focus Frame")
	FLinearColor FocusFrameColor = FLinearColor(0.80f, 0.55f, 0.20f, 0.95f);

	/** Outline thickness of the focus frame, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Focus Frame", meta = (ClampMin = "1.0"))
	float FocusFrameThickness = 2.f;

	/** Space between the focused widget and the focus frame, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Focus Frame", meta = (ClampMin = "0.0"))
	float FocusFramePadding = 3.f;

	/** Corner radius of the focus frame, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Focus Frame", meta = (ClampMin = "0.0"))
	float FocusFrameCornerRadius = 4.f;

	/** Gap between a widget and the tooltip shown next to it (inventory item details): 0 = they touch. */
	UPROPERTY(Config, EditAnywhere, Category = "Tooltip", meta = (ClampMin = "0.0"))
	float ToolTipGap = 0.f;

	/** Speed at which the right stick scrolls the list under the focus, in slate units per second at full tilt. */
	UPROPERTY(Config, EditAnywhere, Category = "Scrolling", meta = (ClampMin = "0.0"))
	float RightStickScrollSpeed = 1400.f;

	/** Right stick deflection below which it does not scroll. */
	UPROPERTY(Config, EditAnywhere, Category = "Scrolling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RightStickDeadZone = 0.25f;

	/** Seconds between two checks of the focus (new screen on top, focus lost, focused widget hidden). */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0"))
	float FocusCheckInterval = 0.05f;

	/**
	 * Viewport Z order of the layer that draws the focus frame and the tooltip: above every menu and modal (the key
	 * capture and video confirm modals sit at 1000). Hidden while the loading screen is up.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Timing")
	int32 HighlightLayerZOrder = 1100;
};
