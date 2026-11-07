// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHLobbyNoticeWidget.h"

#include "Components/TextBlock.h"
#include "UI/Lobby/MKHLobbyWidgetUtils.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHLobbyNoticeWidget::ShowNotice(const FText& Message)
{
	MKHLobbyUI::SetText(MessageText, Message);
	Elapsed = 0.f;
	SetRenderOpacity(ComputeOpacity(Elapsed));
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHLobbyNoticeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// A toast never blocks the lobby.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(ComputeOpacity(Elapsed));
}

void UMKHLobbyNoticeWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	Elapsed += InDeltaTime;
	SetRenderOpacity(ComputeOpacity(Elapsed));
	if (Elapsed >= FadeInDuration + HoldDuration + FadeOutDuration)
	{
		RemoveFromParent();
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

float UMKHLobbyNoticeWidget::ComputeOpacity(const float InElapsed) const
{
	if (InElapsed < FadeInDuration)
	{
		return FadeInDuration > 0.f ? InElapsed / FadeInDuration : 1.f;
	}

	const float FadeOutStart = FadeInDuration + HoldDuration;
	if (InElapsed < FadeOutStart)
	{
		return 1.f;
	}

	return FadeOutDuration > 0.f ? FMath::Clamp(1.f - (InElapsed - FadeOutStart) / FadeOutDuration, 0.f, 1.f) : 0.f;
}
