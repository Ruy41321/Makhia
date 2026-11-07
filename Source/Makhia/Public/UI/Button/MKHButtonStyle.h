// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Styling/SlateTypes.h"
#include "MKHButtonStyle.generated.h"

/**
 * Shared look of a family of buttons (e.g. the large buttons of the game menu).
 *
 * Every UMKHButton pointing at this asset takes its brushes, padding, sounds and content tints
 * from here, so editing the asset restyles every button that uses it. The button's label stays
 * an MKH Text with its own TS_ text style. Catalogue and rules: see Documentation/GameMenuArchitecture.md.
 */
UCLASS(BlueprintType, Const)
class MAKHIA_API UMKHButtonStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Brushes for every state (normal, hovered, pressed, disabled), content padding and sounds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Button")
	FButtonStyle Style;

	/** Tint multiplied over every brush. Left white so the brushes show their own colours. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Button")
	FLinearColor BackgroundColor = FLinearColor::White;

	/** Tint multiplied over the button content (label, icon) while the button is idle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Button|Content")
	FLinearColor ContentColor = FLinearColor::White;

	/** Tint multiplied over the button content while the cursor is over the button. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Button|Content")
	FLinearColor HoveredContentColor = FLinearColor::White;
};
