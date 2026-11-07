// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "MKHTextBlock.generated.h"

class UMKHTextStyle;

/**
 * Text block whose typography comes from a shared UMKHTextStyle asset instead of its own Font.
 *
 * Use it for every text in the game: pick the style that matches the text's role and leave
 * Font alone, since the style overwrites it. With no style assigned it behaves like a plain
 * UTextBlock. Style catalogue and rules: see Documentation/UITypography.md.
 */
UCLASS(meta = (DisplayName = "MKH Text"))
class MAKHIA_API UMKHTextBlock : public UTextBlock
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	//~ End UWidget Interface
#endif

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Switches this text to another style and applies it immediately.
	 *
	 * @param InTextStyle  Style to use; null keeps the current font.
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance|Typography")
	void SetTextStyle(UMKHTextStyle* InTextStyle);

	/** Returns the style this text takes its typography from (may be null). */
	UFUNCTION(BlueprintPure, Category = "Appearance|Typography")
	UMKHTextStyle* GetTextStyle() const { return TextStyle; }

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Copies font and case from TextStyle onto this widget. Does nothing without a style. */
	void ApplyTextStyle();

	// ============================================================
	// Properties
	// ============================================================

	/** Typography role of this text. Overrides Font and Transform Policy when set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UMKHTextStyle> TextStyle;
};
