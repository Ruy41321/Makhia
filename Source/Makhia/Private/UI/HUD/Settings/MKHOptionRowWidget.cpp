// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHOptionRowWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/MKHInputDeviceTypes.h"
#include "Libraries/MKHInputSettingsLibrary.h"
#include "Rendering/SlateRenderer.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHOptionRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(PreviousButton))
	{
		PreviousButton->OnClicked.AddDynamic(this, &UMKHOptionRowWidget::HandlePreviousClicked);
	}
	if (IsValid(NextButton))
	{
		NextButton->OnClicked.AddDynamic(this, &UMKHOptionRowWidget::HandleNextClicked);
	}
	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.AddDynamic(this, &UMKHOptionRowWidget::HandleResetClicked);
	}
	StopMarquee();
}

void UMKHOptionRowWidget::NativeDestruct()
{
	if (IsValid(PreviousButton))
	{
		PreviousButton->OnClicked.RemoveDynamic(this, &UMKHOptionRowWidget::HandlePreviousClicked);
	}
	if (IsValid(NextButton))
	{
		NextButton->OnClicked.RemoveDynamic(this, &UMKHOptionRowWidget::HandleNextClicked);
	}
	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.RemoveDynamic(this, &UMKHOptionRowWidget::HandleResetClicked);
	}

	Super::NativeDestruct();
}

void UMKHOptionRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bCursorOverRow = true;
}

void UMKHOptionRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bCursorOverRow = false;
}

void UMKHOptionRowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshValueOverflow(false);
	if (bValueOverflows && IsRowActive())
	{
		AdvanceMarquee(InDeltaTime);
	}
	else if (bMarqueeRunning)
	{
		StopMarquee();
	}
}

// ============================================================
// Public Interface
// ============================================================

void UMKHOptionRowWidget::SetupRow(FName InOptionId, const FText& InDisplayName, const FText& InDescription)
{
	OptionId = InOptionId;

	if (IsValid(OptionNameText))
	{
		OptionNameText->SetText(InDisplayName);
	}

	if (IsValid(OptionDescriptionText))
	{
		OptionDescriptionText->SetText(InDescription);
		OptionDescriptionText->SetVisibility(InDescription.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}
}

void UMKHOptionRowWidget::SetValue(const FText& InValueText, int32 InIndex, int32 InNumOptions,
	bool bIsModified, bool bIsCustom)
{
	if (IsValid(OptionValueText))
	{
		OptionValueText->SetText(InValueText);
		OptionValueText->SetColorAndOpacity(bIsCustom ? CustomValueColor : ValueColor);
		ValueTextWidth = MeasureValueTextWidth();
		StopMarquee();
		RefreshValueOverflow(true);
	}

	// The arrows go dead at the ends rather than wrapping around: a settings list should never
	// jump from Epic back to Low on one extra click.
	if (IsValid(PreviousButton))
	{
		PreviousButton->SetIsEnabled(InIndex > 0);
	}
	if (IsValid(NextButton))
	{
		NextButton->SetIsEnabled(InIndex < InNumOptions - 1);
	}

	if (IsValid(ResetButton))
	{
		ResetButton->SetVisibility(bIsModified ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHOptionRowWidget::HandlePreviousClicked()
{
	OnOptionStepped.Broadcast(OptionId, -1);
}

void UMKHOptionRowWidget::HandleNextClicked()
{
	OnOptionStepped.Broadcast(OptionId, 1);
}

void UMKHOptionRowWidget::HandleResetClicked()
{
	OnOptionReset.Broadcast(OptionId);
}

void UMKHOptionRowWidget::RefreshValueOverflow(bool bForceApply)
{
	if (!IsValid(OptionValueText))
	{
		return;
	}

	// The text's own slot is the visible box: its width comes from layout, never from the text.
	// Not laid out yet: a new value starts as fitting, the next tick measures it.
	const float BoxWidth = OptionValueText->GetCachedGeometry().GetLocalSize().X;
	if (BoxWidth <= 0.0f && !bForceApply)
	{
		return;
	}

	ValueOverflow = BoxWidth > 0.0f ? FMath::Max(0.0f, ValueTextWidth - BoxWidth) : 0.0f;
	const bool bOverflows = ValueOverflow > 0.5f;
	if (bOverflows == bValueOverflows && !bForceApply)
	{
		return;
	}

	// The engine draws the ellipsis only on left aligned text: a value that fits stays centred.
	bValueOverflows = bOverflows;
	if (!bMarqueeRunning)
	{
		OptionValueText->SetJustification(bOverflows ? ETextJustify::Left : ETextJustify::Center);
	}

	// Full value as tooltip: on the text for the mouse, on the arrows for the gamepad (the focused widget's tooltip).
	const FText ToolTip = bOverflows ? OptionValueText->GetText() : FText::GetEmpty();
	OptionValueText->SetToolTipText(ToolTip);
	for (UButton* Arrow : { PreviousButton.Get(), NextButton.Get() })
	{
		if (IsValid(Arrow))
		{
			Arrow->SetToolTipText(ToolTip);
		}
	}
}

void UMKHOptionRowWidget::AdvanceMarquee(float DeltaTime)
{
	if (!bMarqueeRunning)
	{
		// Not cut while it scrolls: the clipping wrapper hides what is outside the box.
		bMarqueeRunning = true;
		MarqueeOffset = 0.0f;
		MarqueeDirection = 1.0f;
		MarqueePauseRemaining = MarqueeEndPause;
		OptionValueText->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
	}

	if (MarqueePauseRemaining > 0.0f)
	{
		MarqueePauseRemaining -= DeltaTime;
		return;
	}

	MarqueeOffset += MarqueeDirection * MarqueeSpeed * DeltaTime;
	if (MarqueeOffset >= ValueOverflow || MarqueeOffset <= 0.0f)
	{
		MarqueeOffset = FMath::Clamp(MarqueeOffset, 0.0f, ValueOverflow);
		MarqueeDirection = -MarqueeDirection;
		MarqueePauseRemaining = MarqueeEndPause;
	}
	OptionValueText->SetRenderTranslation(FVector2D(-MarqueeOffset, 0.0f));
}

void UMKHOptionRowWidget::StopMarquee()
{
	bMarqueeRunning = false;
	MarqueeOffset = 0.0f;
	if (IsValid(OptionValueText))
	{
		OptionValueText->SetRenderTranslation(FVector2D::ZeroVector);
		OptionValueText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		OptionValueText->SetJustification(bValueOverflows ? ETextJustify::Left : ETextJustify::Center);
	}
}

bool UMKHOptionRowWidget::IsRowActive() const
{
	if (HasFocusedDescendants())
	{
		return true;
	}

	// With the gamepad the hidden cursor may still sit over a row, so only the focus counts (as in UMKHButton).
	return bCursorOverRow && UMKHInputSettingsLibrary::GetActiveInputDevice(GetOwningPlayer()) != EMKHInputDevice::Gamepad;
}

float UMKHOptionRowWidget::MeasureValueTextWidth() const
{
	if (!IsValid(OptionValueText) || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		return 0.0f;
	}

	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return FontMeasure->Measure(OptionValueText->GetText(), OptionValueText->GetFont()).X;
}
