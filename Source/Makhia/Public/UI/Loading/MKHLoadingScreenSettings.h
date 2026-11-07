// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MKHLoadingScreenSettings.generated.h"

class UMaterialInterface;
class UMKHTextStyle;

/**
 * Loading screen settings exposed in Project Settings -> Game -> Makhia Loading Screen: look (same as the boot
 * overlay), fades and the timeouts that turn a stuck travel into an error.
 *
 * Values are stored in Config/DefaultGame.ini under [/Script/Makhia.MKHLoadingScreenSettings].
 * Full model: see Documentation/LoadingScreen.md.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Makhia Loading Screen"))
class MAKHIA_API UMKHLoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Places the settings under Project Settings -> Game and sets the default assets (those of W_BootOverlay). */
	UMKHLoadingScreenSettings();

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Convenience accessor for the loading screen settings.
	 *
	 * @return The CDO of UMKHLoadingScreenSettings.
	 */
	static const UMKHLoadingScreenSettings* Get();

	// ============================================================
	// Properties - Look
	// ============================================================

	/** Full-screen gradient drawn over the black base (MI_UI_Background, like the boot overlay). */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	TSoftObjectPtr<UMaterialInterface> BackgroundMaterial;

	/** Style of the game name (TS_Display). */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	TSoftObjectPtr<UMKHTextStyle> TitleStyle;

	/** Style of the status line (TS_Status). */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	TSoftObjectPtr<UMKHTextStyle> StatusStyle;

	/** Colour of the game name. */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	FLinearColor TitleColor = FLinearColor(0.847f, 0.768f, 0.638f);

	/** Colour of the divider under the game name. */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	FLinearColor DividerColor = FLinearColor(0.584f, 0.309f, 0.048f, 0.55f);

	/** Colour of the status line. */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	FLinearColor StatusColor = FLinearColor(0.302f, 0.25f, 0.195f);

	// ============================================================
	// Properties - Timing
	// ============================================================

	/** Seconds the screen takes to cover the game before a travel starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInDuration = 0.2f;

	/** Seconds the screen takes to fade out once the destination is ready. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutDuration = 0.35f;

	/** Shortest time the screen stays up, so it never just flickers. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float MinimumDisplayTime = 1.f;

	/** Extra time the screen stays up once the destination is ready (textures streaming in). */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float HoldAfterReady = 0.3f;

	/** Seconds spent waiting in a lobby for a match that should be starting before giving up (MATCH-START-TIMEOUT). */
	UPROPERTY(Config, EditAnywhere, Category = "Timeouts", meta = (ClampMin = "1.0", Units = "s"))
	float MatchStartTimeout = 15.f;

	/** Longest time the screen may stay up before the travel counts as failed (TRAVEL-TIMEOUT). */
	UPROPERTY(Config, EditAnywhere, Category = "Timeouts", meta = (ClampMin = "1.0", Units = "s"))
	float TravelTimeout = 60.f;
};
