// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "SMKHFocusHighlightLayer.h"

#include "Rendering/DrawElements.h"
#include "UI/Navigation/MKHMenuNavigationSettings.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCanvas.h"

// ============================================================
// Lifecycle
// ============================================================

void SMKHFocusHighlightLayer::Construct(const FArguments& InArgs)
{
	const UMKHMenuNavigationSettings* Settings = GetDefault<UMKHMenuNavigationSettings>();
	FrameBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, Settings->FocusFrameCornerRadius,
		Settings->FocusFrameColor, Settings->FocusFrameThickness);
	FramePadding = Settings->FocusFramePadding;
	ToolTipGap = Settings->ToolTipGap;

	SetVisibility(EVisibility::HitTestInvisible);
	// Repainted every frame: the focused widget moves (scrolling, animations) without telling this layer.
	ForceVolatile(true);

	ChildSlot
	[
		SNew(SCanvas)
		+ SCanvas::Slot()
		.Position(TAttribute<FVector2D>::CreateSP(this, &SMKHFocusHighlightLayer::GetToolTipPosition))
		.Size(TAttribute<FVector2D>::CreateSP(this, &SMKHFocusHighlightLayer::GetToolTipSize))
		[
			SAssignNew(ToolTipBox, SBox)
		]
	];
}

int32 SMKHFocusHighlightLayer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LastGeometry = AllottedGeometry;

	FSlateRect TargetRect;
	if (GetLocalRect(FrameTarget.Pin(), TargetRect))
	{
		const FSlateRect FrameRect = TargetRect.ExtendBy(FMargin(FramePadding));
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(FrameRect.GetSize()), FSlateLayoutTransform(FVector2f(FrameRect.GetTopLeft()))),
			&FrameBrush, ESlateDrawEffect::None, FrameBrush.GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint());
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}

// ============================================================
// Public Interface
// ============================================================

void SMKHFocusHighlightLayer::SetFrameTarget(const TSharedPtr<SWidget>& InTarget)
{
	FrameTarget = InTarget;
}

void SMKHFocusHighlightLayer::SetToolTip(const TSharedPtr<SWidget>& InContent, const TSharedPtr<SWidget>& InAnchor)
{
	ToolTipAnchor = InAnchor;
	if (ToolTipBox.IsValid())
	{
		ToolTipBox->SetContent(InContent.IsValid() ? InContent.ToSharedRef() : SNullWidget::NullWidget);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool SMKHFocusHighlightLayer::GetLocalRect(const TSharedPtr<SWidget>& Widget, FSlateRect& OutRect) const
{
	if (!Widget.IsValid())
	{
		return false;
	}

	const FGeometry& WidgetGeometry = Widget->GetPaintSpaceGeometry();
	const FVector2f AbsoluteSize = WidgetGeometry.GetAbsoluteSize();
	if (AbsoluteSize.X < 1.f || AbsoluteSize.Y < 1.f)
	{
		return false;
	}

	const FVector2f TopLeft = LastGeometry.AbsoluteToLocal(WidgetGeometry.GetAbsolutePosition());
	const FVector2f BottomRight = LastGeometry.AbsoluteToLocal(WidgetGeometry.GetAbsolutePosition() + AbsoluteSize);
	OutRect = FSlateRect(TopLeft, BottomRight);
	return true;
}

FVector2D SMKHFocusHighlightLayer::GetToolTipPosition() const
{
	FSlateRect AnchorRect;
	if (!GetLocalRect(ToolTipAnchor.Pin(), AnchorRect))
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D Size = GetToolTipSize();
	const FVector2D LayerSize = FVector2D(LastGeometry.GetLocalSize());

	double X = AnchorRect.Right + ToolTipGap;
	if (X + Size.X > LayerSize.X)
	{
		X = AnchorRect.Left - ToolTipGap - Size.X;
	}
	const double Y = FMath::Clamp(static_cast<double>(AnchorRect.Top), 0.0, FMath::Max(0.0, LayerSize.Y - Size.Y));
	return FVector2D(FMath::Max(0.0, X), Y);
}

FVector2D SMKHFocusHighlightLayer::GetToolTipSize() const
{
	return ToolTipBox.IsValid() ? FVector2D(ToolTipBox->GetDesiredSize()) : FVector2D::ZeroVector;
}
