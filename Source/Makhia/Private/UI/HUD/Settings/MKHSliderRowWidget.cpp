// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHSliderRowWidget.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHSliderRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(ValueSlider))
	{
		ValueSlider->OnValueChanged.AddUniqueDynamic(this, &UMKHSliderRowWidget::HandleSliderChanged);
		ValueSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UMKHSliderRowWidget::HandleCaptureEnd);
		ValueSlider->OnControllerCaptureEnd.AddUniqueDynamic(this, &UMKHSliderRowWidget::HandleCaptureEnd);
	}

	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.AddDynamic(this, &UMKHSliderRowWidget::HandleResetClicked);
	}
}

void UMKHSliderRowWidget::NativeDestruct()
{
	if (IsValid(ValueSlider))
	{
		ValueSlider->OnValueChanged.RemoveDynamic(this, &UMKHSliderRowWidget::HandleSliderChanged);
		ValueSlider->OnMouseCaptureEnd.RemoveDynamic(this, &UMKHSliderRowWidget::HandleCaptureEnd);
		ValueSlider->OnControllerCaptureEnd.RemoveDynamic(this, &UMKHSliderRowWidget::HandleCaptureEnd);
	}

	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.RemoveDynamic(this, &UMKHSliderRowWidget::HandleResetClicked);
	}

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHSliderRowWidget::SetupRow(FName InOptionId, const FText& InDisplayName, const FText& InDescription,
	float InMinValue, float InMaxValue)
{
	OptionId = InOptionId;
	MinValue = InMinValue;
	MaxValue = FMath::Max(InMaxValue, InMinValue + KINDA_SMALL_NUMBER);

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

void UMKHSliderRowWidget::SetValueFormat(const FText& InFormat, int32 InDecimals)
{
	ValueFormat = InFormat;
	ValueDecimals = FMath::Clamp(InDecimals, 0, 3);
}

void UMKHSliderRowWidget::SetValue(float InValue, bool bIsModified)
{
	const float Clamped = FMath::Clamp(InValue, MinValue, MaxValue);
	const float Normalized = (Clamped - MinValue) / (MaxValue - MinValue);

	if (IsValid(ValueSlider))
	{
		ValueSlider->SetValue(Normalized);
	}

	if (IsValid(OptionValueText))
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = ValueDecimals;
		Format.MaximumFractionalDigits = ValueDecimals;
		OptionValueText->SetText(FText::Format(ValueFormat, FText::AsNumber(Clamped, &Format)));
	}

	if (IsValid(ResetButton))
	{
		ResetButton->SetVisibility(bIsModified ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	RefreshTrackFill(Normalized);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSliderRowWidget::HandleSliderChanged(float NewValue)
{
	const float Value = MinValue + NewValue * (MaxValue - MinValue);

	RefreshTrackFill(NewValue);

	if (IsValid(OptionValueText))
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = ValueDecimals;
		Format.MaximumFractionalDigits = ValueDecimals;
		OptionValueText->SetText(FText::Format(ValueFormat, FText::AsNumber(Value, &Format)));
	}

	OnValueChanged.Broadcast(OptionId, Value);
}

void UMKHSliderRowWidget::HandleCaptureEnd()
{
	OnValueCommitted.Broadcast(OptionId);
}

void UMKHSliderRowWidget::HandleResetClicked()
{
	OnValueReset.Broadcast(OptionId);
}

void UMKHSliderRowWidget::RefreshTrackFill(float Normalized) const
{
	if (!IsValid(TrackRow) || TrackRow->GetChildrenCount() < 2)
	{
		return;
	}

	// A zero-weight slot collapses to nothing, so both halves keep a sliver of size.
	const float Filled = FMath::Clamp(Normalized, 0.0001f, 0.9999f);

	FSlateChildSize FillSize(ESlateSizeRule::Fill);
	FillSize.Value = Filled;

	FSlateChildSize RestSize(ESlateSizeRule::Fill);
	RestSize.Value = 1.0f - Filled;

	if (UHorizontalBoxSlot* FillSlot = Cast<UHorizontalBoxSlot>(TrackRow->GetChildAt(0)->Slot); FillSlot != nullptr)
	{
		FillSlot->SetSize(FillSize);
	}
	if (UHorizontalBoxSlot* RestSlot = Cast<UHorizontalBoxSlot>(TrackRow->GetChildAt(1)->Slot); RestSlot != nullptr)
	{
		RestSlot->SetSize(RestSize);
	}
}
