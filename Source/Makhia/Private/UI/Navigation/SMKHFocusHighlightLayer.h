// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SCompoundWidget.h"

class SBox;

/**
 * Full-screen, hit-test invisible layer of the gamepad menu navigation: draws a frame around the focused
 * widget and shows the tooltip of the focused widget next to it. Owned by UMKHMenuNavigationSubsystem.
 */
class SMKHFocusHighlightLayer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMKHFocusHighlightLayer) {}
	SLATE_END_ARGS()

	// ============================================================
	// Lifecycle
	// ============================================================

	/** Builds the tooltip canvas and reads the frame look from UMKHMenuNavigationSettings. */
	void Construct(const FArguments& InArgs);

	//~ Begin SWidget Interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return false; }
	//~ End SWidget Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Sets the widget the frame is drawn around.
	 *
	 * @param InTarget  Focused widget; null hides the frame.
	 */
	void SetFrameTarget(const TSharedPtr<SWidget>& InTarget);

	/**
	 * Shows a tooltip next to a widget, or hides it.
	 *
	 * @param InContent  Tooltip content; null hides the tooltip.
	 * @param InAnchor   Widget the tooltip is placed next to.
	 */
	void SetToolTip(const TSharedPtr<SWidget>& InContent, const TSharedPtr<SWidget>& InAnchor);

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Rectangle of a widget in this layer's local space.
	 *
	 * @param Widget  Widget to locate.
	 * @param OutRect Its rectangle, in local units.
	 * @return False when the widget is gone or has no size yet.
	 */
	bool GetLocalRect(const TSharedPtr<SWidget>& Widget, FSlateRect& OutRect) const;

	/** Canvas position of the tooltip: right of the anchor, or left when it would leave the screen. */
	FVector2D GetToolTipPosition() const;

	/** Canvas size of the tooltip: its desired size. */
	FVector2D GetToolTipSize() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Outline brush of the focus frame. */
	FSlateRoundedBoxBrush FrameBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 4.f, FLinearColor::White, 2.f);

	/** Space between the focused widget and the frame. */
	float FramePadding = 3.f;

	/** Gap between the anchor and the tooltip. */
	float ToolTipGap = 0.f;

	/** Widget the frame is drawn around. */
	TWeakPtr<SWidget> FrameTarget;

	/** Widget the tooltip is placed next to. */
	TWeakPtr<SWidget> ToolTipAnchor;

	/** Holds the tooltip content. */
	TSharedPtr<SBox> ToolTipBox;

	/** Last painted geometry of this layer, used to place the frame and the tooltip. */
	mutable FGeometry LastGeometry;
};
