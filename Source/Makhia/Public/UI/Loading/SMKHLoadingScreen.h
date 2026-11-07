// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;

/**
 * Full-screen loading screen, drawn like the boot overlay (W_BootOverlay): black base, gradient, game name, divider,
 * throbber and status line.
 *
 * Pure Slate so it can run on the MoviePlayer loading thread during a blocking map load (a UMG widget there is not
 * thread safe). UMKHLoadingScreenSubsystem builds one instance for the game viewport and another for each blocking
 * load; the assets behind the style are kept alive by the subsystem. Full model: see Documentation/LoadingScreen.md.
 */
class MAKHIA_API SMKHLoadingScreen : public SCompoundWidget
{
public:
	/** Look of the screen, resolved by the subsystem from UMKHLoadingScreenSettings. */
	struct FStyle
	{
		/** Gradient over the black base; may be null (black only). Must outlive the widget. */
		const FSlateBrush* BackgroundBrush = nullptr;

		/** Font of the game name. */
		FSlateFontInfo TitleFont;

		/** Font of the status line. */
		FSlateFontInfo StatusFont;

		/** Colour of the game name. */
		FLinearColor TitleColor = FLinearColor::White;

		/** Colour of the divider. */
		FLinearColor DividerColor = FLinearColor::White;

		/** Colour of the status line. */
		FLinearColor StatusColor = FLinearColor::White;

		/** Case transformation of the game name (from its text style). */
		ETextTransformPolicy TitleTransform = ETextTransformPolicy::None;

		/** Case transformation of the status line (from its text style). */
		ETextTransformPolicy StatusTransform = ETextTransformPolicy::None;
	};

	SLATE_BEGIN_ARGS(SMKHLoadingScreen) {}
		/** First status line. */
		SLATE_ARGUMENT(FText, StatusText)
	SLATE_END_ARGS()

	/**
	 * Builds the widget.
	 *
	 * @param InArgs   Slate arguments.
	 * @param InStyle  Look of the screen.
	 */
	void Construct(const FArguments& InArgs, const FStyle& InStyle);

	/**
	 * Changes the status line (game thread instance only).
	 *
	 * @param InText  New status line.
	 */
	void SetStatusText(const FText& InText);

private:
	/** Status line. */
	TSharedPtr<STextBlock> StatusTextBlock;

	/** Plain black base, drawn even while UI material shaders are still compiling. */
	FSlateColorBrush SolidBrush = FSlateColorBrush(FLinearColor::Black);

	/** Plain brush of the divider, tinted with the divider colour. */
	FSlateColorBrush DividerBrush = FSlateColorBrush(FLinearColor::White);
};
