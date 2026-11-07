// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHVoiceMicMeterWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Voice/MKHVoiceChatSubsystem.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHVoiceMicMeterWidget::SetupRow(const FText& InDisplayName, const FText& InDescription)
{
	if (IsValid(OptionNameText))
	{
		OptionNameText->SetText(InDisplayName);
	}
	if (IsValid(OptionDescriptionText))
	{
		OptionDescriptionText->SetText(InDescription);
		OptionDescriptionText->SetVisibility(InDescription.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UMKHVoiceMicMeterWidget::SetThreshold(float InThreshold, bool bInVisible)
{
	Threshold = FMath::Clamp(InThreshold, 0.f, 1.f);
	bThresholdVisible = bInVisible;
	RefreshMarker();
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHVoiceMicMeterWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->StartMicrophoneTest();
	}
	ShownLevel = 0.f;
	RefreshMarker();
}

void UMKHVoiceMicMeterWidget::NativeDestruct()
{
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->StopMicrophoneTest();
	}
	Super::NativeDestruct();
}

void UMKHVoiceMicMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	const float Level = IsValid(VoiceChat) ? VoiceChat->GetMicrophoneTestLevel() : 0.f;
	ShownLevel = Level >= ShownLevel ? Level : FMath::Max(Level, ShownLevel - FallSpeed * InDeltaTime);
	if (IsValid(LevelBar))
	{
		LevelBar->SetPercent(ShownLevel);
		LevelBar->SetFillColorAndOpacity(!bThresholdVisible || ShownLevel >= Threshold ? ActiveColor : QuietColor);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHVoiceMicMeterWidget::RefreshMarker() const
{
	if (!IsValid(ThresholdRow) || ThresholdRow->GetChildrenCount() < 3)
	{
		return;
	}
	ThresholdRow->SetVisibility(bThresholdVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	const float Left = FMath::Max(Threshold, 0.001f);
	const float Right = FMath::Max(1.f - Threshold, 0.001f);
	FSlateChildSize LeftSize(ESlateSizeRule::Fill);
	LeftSize.Value = Left;
	FSlateChildSize RightSize(ESlateSizeRule::Fill);
	RightSize.Value = Right;
	if (UHorizontalBoxSlot* LeftSlot = Cast<UHorizontalBoxSlot>(ThresholdRow->GetChildAt(0)->Slot))
	{
		LeftSlot->SetSize(LeftSize);
	}
	if (UHorizontalBoxSlot* RightSlot = Cast<UHorizontalBoxSlot>(ThresholdRow->GetChildAt(2)->Slot))
	{
		RightSlot->SetSize(RightSize);
	}
}
