// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Styling/SlateTypes.h"
#include "MKHTextStyle.generated.h"

/**
 * One entry of the game's type scale (e.g. Title, Label, HUD Timer).
 *
 * Every UMKHTextBlock pointing at this asset takes its font, size, letter spacing, outline and
 * case from here, so editing the asset restyles every text that uses it.
 * Colours stay on the widgets: they depend on context (rarity, state), not on the role.
 * Available styles and when to use them: see Documentation/UITypography.md.
 */
UCLASS(BlueprintType, Const)
class MAKHIA_API UMKHTextStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Font family, typeface, size, letter spacing and outline applied by this style. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo Font;

	/** Case transformation applied to the displayed text (e.g. titles are shown upper case). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Typography")
	ETextTransformPolicy TextTransform = ETextTransformPolicy::None;
};
